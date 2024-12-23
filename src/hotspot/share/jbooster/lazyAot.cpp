/*
 * Copyright (c) 2020, 2023, Huawei Technologies Co., Ltd. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "classfile/classLoaderData.inline.hpp"
#include "classfile/classLoaderDataGraph.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmClasses.hpp"
#include "classfile/vmSymbols.hpp"
#include "jbooster/jBoosterManager.hpp"
#include "jbooster/lazyAot.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/utilities/debugUtils.hpp"
#include "jbooster/utilities/scalarHashMap.inline.hpp"
#include "memory/resourceArea.inline.hpp"
#include "oops/instanceKlass.inline.hpp"
#include "oops/method.inline.hpp"
#include "oops/methodData.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/timerTrace.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/growableArray.hpp"

enum {
  ALL_KLASSES,
  RESOLVED_KLASSES,
  INITIALIZED_KLASSES
};

ClassLoaderKeepAliveMark::~ClassLoaderKeepAliveMark() {
  for (GrowableArrayIterator<OopHandle> iter = _handles.begin(); iter != _handles.end(); ++iter) {
    (*iter).release(Universe::vm_global());
  }
}

void ClassLoaderKeepAliveMark::add(ClassLoaderData* cld){
  _handles.append(OopHandle(Universe::vm_global(), cld->holder_phantom()));
}

void ClassLoaderKeepAliveMark::add_all(GrowableArray<ClassLoaderData*>* clds){
  for (GrowableArrayIterator<ClassLoaderData*> iter = clds->begin(); iter != clds->end(); ++iter) {
    add(*iter);
  }
}

InstanceKlass* LazyAOT::get_klass_from_class_tag(const constantPoolHandle& cp,
                                                 int index,
                                                 Handle loader,
                                                 int which_klasses,
                                                 TRAPS) {
  Klass* klass = nullptr;
  if (which_klasses == ALL_KLASSES) {
    // resolve class if not resolved
    klass = cp->klass_at(index, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      ResourceMark rm;
      CLEAR_PENDING_EXCEPTION;
      CPKlassSlot kslot = cp->klass_slot_at(index);
      int name_index = kslot.name_index();
      Symbol* name = cp->symbol_at(name_index);
      klass = SystemDictionary::resolve_or_fail(name, loader, Handle(), true, THREAD);
      if (HAS_PENDING_EXCEPTION) {
        if (PENDING_EXCEPTION->is_a(vmClasses::OutOfMemoryError_klass())) return nullptr;
        LogTarget(Trace, jbooster, compilation) lt;
        if (lt.is_enabled()) {
          lt.print("Class in constant pool not found: class=\"%s\", loader=\"%s\", holder=\"%s\"",
                   name->as_C_string(),
                   ClassLoaderData::class_loader_data_or_null(loader())->loader_name(),
                   cp->pool_holder()->internal_name());
        }
        CLEAR_PENDING_EXCEPTION;
        return nullptr;
      }
    }
    if (klass == nullptr || !klass->is_instance_klass()) return nullptr;
    return InstanceKlass::cast(klass);
  } else {  // RESOLVED_KLASSES or INITIALIZED_KLASSES
    klass = ConstantPool::klass_at_if_loaded(cp, index);
    if (klass == nullptr || !klass->is_instance_klass()) return nullptr;
    InstanceKlass* ik = InstanceKlass::cast(klass);
    if (which_klasses == INITIALIZED_KLASSES && !ik->is_initialized()) {
      return nullptr;
    }
    return ik;
  }
}

void LazyAOT::get_klasses_from_name_and_type_tag(const constantPoolHandle& cp,
                                                 int index,
                                                 Handle loader,
                                                 GrowableArray<InstanceKlass*> &res,
                                                 int which_klasses,
                                                 TRAPS) {
  int sym_index = cp->signature_ref_index_at(index);
  Symbol* sym = cp->symbol_at(sym_index);
  // The sym could be like "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"
  // or "Ljava/lang/Object;".
  for (SignatureStream ss(sym, Signature::is_method(sym)); !ss.is_done(); ss.next()) {
    if (ss.is_array()) ss.skip_array_prefix();
    if (ss.is_reference()) {
      assert(!ss.is_array(), "sanity");
      SignatureStream::FailureMode mode = which_klasses == ALL_KLASSES
                                          ? SignatureStream::ReturnNull
                                          : SignatureStream::CachedOrNull;
      Klass* klass = ss.as_klass(loader, Handle(), mode, THREAD);
      if (HAS_PENDING_EXCEPTION) {
        if (PENDING_EXCEPTION->is_a(vmClasses::OutOfMemoryError_klass())) return;
        LogTarget(Trace, jbooster, compilation) lt;
        if (lt.is_enabled()) {
          ResourceMark rm;
          lt.print("Class in constant pool not found: method=\"%s\", loader=\"%s\", holder=\"%s\"",
                   sym->as_C_string(),
                   ClassLoaderData::class_loader_data_or_null(loader())
                                     ->loader_name(),
                   cp->pool_holder()->internal_name());
        }
        CLEAR_PENDING_EXCEPTION;
        continue;
      }
      if (klass == nullptr || !klass->is_instance_klass()) continue;
      InstanceKlass* ik = InstanceKlass::cast(klass);
      if (which_klasses == INITIALIZED_KLASSES && !ik->is_initialized()) {
        continue;
      }
      res.append(ik);
    }
  }
}

void LazyAOT::collect_klasses_by_inheritance(GrowableArray<InstanceKlass*>* dst,
                                             ScalarHashSet<InstanceKlass*>* vis,
                                             InstanceKlass* ik,
                                             TRAPS) {
  if (!vis->add(ik)) return;
  InstanceKlass* super = ik->java_super();
  if (super != nullptr) {
    collect_klasses_by_inheritance(dst, vis, super, CHECK);
  }
  Array<InstanceKlass*>* interfaces = ik->local_interfaces();
  for (int i = 0; i < interfaces->length(); ++i) {
    InstanceKlass* interface = interfaces->at(i);
    collect_klasses_by_inheritance(dst, vis, interface, CHECK);
  }
  dst->append(ik);
}

void LazyAOT::collect_klasses_in_constant_pool(GrowableArray<InstanceKlass*>* dst,
                                               ScalarHashSet<InstanceKlass*>* vis,
                                               InstanceKlass* ik,
                                               int which_klasses,
                                               TRAPS) {
  constantPoolHandle cp(THREAD, ik->constants());
  Handle class_loader(THREAD, ik->class_loader());
  for (int i = 0; i < cp->length(); ++i) {
    constantTag tag = cp->tag_at(i);
    if (tag.is_unresolved_klass() || tag.is_klass()) {
      InstanceKlass* next = get_klass_from_class_tag(cp, i, class_loader, which_klasses, CHECK);
      if (next != nullptr) {  // next is null when it is not a InstanceKlass
        collect_klasses_by_inheritance(dst, vis, next, CHECK);
      }
    } else if (tag.is_name_and_type()) {
      GrowableArray<InstanceKlass*> next_list;
      get_klasses_from_name_and_type_tag(cp, i, class_loader, next_list, which_klasses, CHECK);
      for (GrowableArrayIterator<InstanceKlass*> iter = next_list.begin();
                                                 iter != next_list.end();
                                                 ++iter) {
        collect_klasses_by_inheritance(dst, vis, *iter, CHECK);
      }
    }
  }
}

void LazyAOT::collect_klasses_in_constant_pool(GrowableArray<InstanceKlass*>* dst,
                                               ScalarHashSet<InstanceKlass*>* vis,
                                               TRAPS) {
  int last_len = 0;
  for (int lvl = _compilation_related_klasses_layers; lvl > 0; --lvl) {
    int len = dst->length();
    for (int i = last_len; i < len; ++i) {
      ThreadInVMfromNative tiv(THREAD);
      if (JBoosterResolveExtraKlasses) {
        collect_klasses_in_constant_pool(dst, vis, dst->at(i), ALL_KLASSES, CHECK);
      } else {
        collect_klasses_in_constant_pool(dst, vis, dst->at(i), RESOLVED_KLASSES, CHECK);
      }
    }
    last_len = len;
  }
}

void LazyAOT::collect_klasses_in_method_data(GrowableArray<InstanceKlass*>* dst_ik,
                                             GrowableArray<ArrayKlass*>* dst_ak,
                                             ScalarHashSet<InstanceKlass*>* ik_vis,
                                             TRAPS) {
  int last_len = 0;
  ScalarHashSet<ArrayKlass*> ak_vis;
  for (int lvl = _compilation_related_klasses_layers; lvl > 0; --lvl) {
    int len = dst_ik->length();
    for (int i = last_len; i < len; ++i) {
      Array<Method*>* methods = dst_ik->at(i)->methods();
      for (int j = 0; j < methods->length(); j++) {
        MethodData* method_data = methods->at(j)->method_data();
        if (method_data != nullptr) {
          collect_klasses_in_method_data(dst_ik, dst_ak, ik_vis, &ak_vis,
                                         method_data, ALL_KLASSES, CHECK);
        }
      }
    }
    last_len = len;
  }
}

void LazyAOT::collect_klasses_in_method_data(GrowableArray<InstanceKlass*>* dst_ik,
                                             GrowableArray<ArrayKlass*>* dst_ak,
                                             ScalarHashSet<InstanceKlass*>* ik_vis,
                                             ScalarHashSet<ArrayKlass*>* ak_vis,
                                             MethodData* method_data,
                                             int which_klasses,
                                             TRAPS) {
  int position = 0;
  while (position < method_data->data_size()) {
    ProfileData* profile_data = method_data->data_at(position);
    GrowableArray<InstanceKlass*> ik_array;
    GrowableArray<ArrayKlass*> ak_array;
    profile_data->collect_klass(&ik_array, &ak_array);
    for (GrowableArrayIterator<InstanceKlass*> iter = ik_array.begin();
                                               iter != ik_array.end();
                                               ++iter) {
      if (which_klasses == INITIALIZED_KLASSES && !(*iter)->is_initialized()) {
        continue;
      }
      collect_klasses_by_inheritance(dst_ik, ik_vis, *iter, CHECK);
    }
    for (GrowableArrayIterator<ArrayKlass*> iter = ak_array.begin();
                                            iter != ak_array.end();
                                            ++iter) {
      if (!ak_vis->add(*iter)) continue;
      dst_ak->append(*iter);
    }
    position += profile_data->size_in_bytes();
  }
}

void LazyAOT::sort_klasses_by_inheritance(GrowableArray<InstanceKlass*>* dst_ik,
                                          GrowableArray<ArrayKlass*>* dst_ak,
                                          GrowableArray<InstanceKlass*>* src,
                                          bool reverse_scan_src,
                                          TRAPS) {
  ScalarHashSet<InstanceKlass*> visited;
  if (reverse_scan_src) {
    for (int i = src->length() - 1; i >= 0; --i) {
      collect_klasses_by_inheritance(dst_ik, &visited, src->at(i), CHECK);
    }
  } else {
    for (GrowableArrayIterator<InstanceKlass*> iter = src->begin();
                                               iter != src->end();
                                               ++iter) {
      collect_klasses_by_inheritance(dst_ik, &visited, *iter, CHECK);
    }
  }
  collect_klasses_in_constant_pool(dst_ik, &visited, CHECK);
  collect_klasses_in_method_data(dst_ik, dst_ak, &visited, CHECK);
}

bool LazyAOT::can_be_compiled(const methodHandle& mh) {
  ResourceMark rm;
  InstanceKlass* ik = mh->method_holder();
  return can_be_compiled(ik);
}

bool LazyAOT::can_be_compiled(InstanceKlass* ik, bool check_cld) {
  if (check_cld && !can_be_compiled(ik->class_loader_data())) {
    return false;
  }

  // AOT does not support it
  if (ik->is_hidden()) return false;

  // Error occurs when proxy classes are transferred to the server.
  if (ik->is_dynamic_proxy()) return false;

  return true;
}

bool LazyAOT::can_be_compiled(ClassLoaderData* cld) {
  Handle loader_h(JavaThread::current(), cld->class_loader());

  // Skip the bootstrap loaders used by LambdaForm.
  // Each LambdaForm has its own bootstrap class loader.
  // @see ClassLoaderData::is_boot_class_loader_data()
  if (cld->is_the_null_class_loader_data()) return true;
  if (loader_h.is_null()) return false;

  // Skip klasses like GeneratedMethodAccessor, GeneratedConstructorAccessor
  // and GeneratedSerializationConstructorAccessor.
  if (loader_h->is_a(vmClasses::reflect_DelegatingClassLoader_klass())) {
    return false;
  }

  return true;
}

int data_layout[] = {
  in_bytes(Method::const_offset()),
  in_bytes(Method::access_flags_offset()),
  in_bytes(Method::flags_offset()),
  in_bytes(Method::from_compiled_offset()),
  in_bytes(Method::code_offset()),
  Method::method_data_offset_in_bytes(),
  in_bytes(Method::method_counters_offset()),
  in_bytes(Method::native_function_offset()),
  in_bytes(Method::from_interpreted_offset()),
  in_bytes(Method::interpreter_entry_offset()),
  in_bytes(Method::signature_handler_offset()),
  in_bytes(Method::itable_index_offset()),
  (int)Method::last_method_flags(),

  in_bytes(ConstMethod::constants_offset()),
  in_bytes(ConstMethod::max_stack_offset()),
  in_bytes(ConstMethod::size_of_locals_offset()),
  in_bytes(ConstMethod::size_of_parameters_offset()),
  in_bytes(ConstMethod::result_type_offset()),

  ConstantPool::tags_offset_in_bytes(),
  ConstantPool::cache_offset_in_bytes(),
  ConstantPool::pool_holder_offset_in_bytes(),
  ConstantPool::resolved_klasses_offset_in_bytes(),

  Array<Method*>::length_offset_in_bytes(),
  Array<Method*>::base_offset_in_bytes(),

  in_bytes(GrowableArrayBase::len_offset()),
  in_bytes(GrowableArrayBase::max_offset()),
  in_bytes(GrowableArrayView<address>::data_offset())
};

class KlassGetAllInstanceKlassesClosure: public KlassClosure {
  GrowableArray<InstanceKlass*>* _klasses;
  GrowableArray<Method*>* _methods_to_compile;
  GrowableArray<Method*>* _methods_not_compile;
  ScalarHashSet<Symbol*>* _visited;

public:
  KlassGetAllInstanceKlassesClosure(GrowableArray<InstanceKlass*>* klasses,
                                    GrowableArray<Method*>* methods_to_compile,
                                    GrowableArray<Method*>* methods_not_compile,
                                    ScalarHashSet<Symbol*>* visited):
                                    _klasses(klasses),
                                    _methods_to_compile(methods_to_compile),
                                    _methods_not_compile(methods_not_compile),
                                    _visited(visited) {}

  void do_klass(Klass* k) override {
    if (!k->is_instance_klass()) return;
    InstanceKlass* ik = InstanceKlass::cast(k);
    if (!ik->is_loaded()) return;
    if (!_visited->add(ik->name())) return; // skip dup klass

    if (PrintAllClassInfo) {
      ResourceMark rm;
      Symbol* class_name = ik->name();
      const char* class_name_c = class_name == nullptr ? nullptr
                               : class_name->as_C_string();
      tty->print_cr("        %s:%p", class_name_c, ik);
    }

    // Maybe we should add "if (!ik->is_initialized()) return;".
    if (!LazyAOT::can_be_compiled(ik, /* check_cld */ false)) return;

    Array<Method*>* methods = ik->methods();
    if (methods->length() == 0) return;
    if (os::Linux::is_jboosterLazyAOT_do_valid()) {
      int current_tc_count = _methods_to_compile->length();
      int current_nc_count = _methods_not_compile->length();
      int current_k_count = _klasses->length();
      _methods_to_compile->at_put_grow(current_tc_count + methods->length() - 1, (Method*)(uintptr_t)current_tc_count, nullptr);
      _methods_not_compile->at_put_grow(current_nc_count + methods->length() - 1, (Method*)(uintptr_t)current_nc_count, nullptr);
      _klasses->at_put_grow(current_k_count, (InstanceKlass*)(uintptr_t)current_k_count, nullptr);
      os::Linux::jboosterLazyAOT_do(data_layout, (address)methods, (address)_methods_to_compile,
                                    (address)_methods_not_compile, (address)_klasses);
    }
  }
};

class CLDGetAllInstanceKlassesClosure: public CLDClosure {
  GrowableArray<ClassLoaderData*>* _loaders;
  GrowableArray<InstanceKlass*>* _klasses;
  GrowableArray<Method*>* _methods_to_compile;
  GrowableArray<Method*>* _methods_not_compile;
  ScalarHashSet<Symbol*> _visited;

private:
  void for_each(ClassLoaderData* cld) {
    if (PrintAllClassInfo) {
      ResourceMark rm;
      Klass* loader_class = cld->class_loader_klass();
      Symbol* loader_class_name = loader_class == nullptr ? nullptr : loader_class->name();
      Symbol* loader_name = cld->name();
      const char* loader_class_name_c = loader_class_name == nullptr ? nullptr
                                      : loader_class_name->as_C_string();
      const char* loader_name_c = loader_name == nullptr ? nullptr
                                : loader_name->as_C_string();
      tty->print_cr("Class loader: \"%s:%s:%p\", can_be_compiled=%s.",
                    loader_class_name_c, loader_name_c, cld,
                    BOOL_TO_STR(LazyAOT::can_be_compiled(cld)));
      tty->print_cr("  -");
    }
    if (!cld->has_class_mirror_holder() && LazyAOT::can_be_compiled(cld)) {
      if (_loaders != nullptr) _loaders->append(cld);
      KlassGetAllInstanceKlassesClosure cl(_klasses, _methods_to_compile, _methods_not_compile, &_visited);
      cld->classes_do(&cl);
    }
  }

public:
  CLDGetAllInstanceKlassesClosure(GrowableArray<ClassLoaderData*>* all_loaders,
                                  GrowableArray<InstanceKlass*>* klasses_to_compile,
                                  GrowableArray<Method*>* methods_to_compile,
                                  GrowableArray<Method*>* methods_not_compile,
                                  bool no_boot_platform = false):
                                  _loaders(all_loaders),
                                  _klasses(klasses_to_compile),
                                  _methods_to_compile(methods_to_compile),
                                  _methods_not_compile(methods_not_compile),
                                  _visited() {}

  void do_cld(ClassLoaderData* cld) override { for_each(cld); }
};

void LazyAOT::collect_all_klasses_to_compile(ClassLoaderKeepAliveMark& clka,
                                             GrowableArray<ClassLoaderData*>* all_loaders,
                                             GrowableArray<InstanceKlass*>* klasses_to_compile,
                                             GrowableArray<Method*>* methods_to_compile,
                                             GrowableArray<Method*>* methods_not_compile,
                                             GrowableArray<InstanceKlass*>* all_sorted_klasses,
                                             GrowableArray<ArrayKlass*>* array_klasses,
                                             TRAPS) {
  {
    {
      TraceTime tt("Collect all classes", TRACETIME_LOG(Info, jbooster, aot));
      CLDGetAllInstanceKlassesClosure cl(all_loaders, klasses_to_compile, methods_to_compile, methods_not_compile);
      ThreadInVMfromNative tivm(THREAD);
      MutexLocker ml(THREAD, ClassLoaderDataGraph_lock);
      ClassLoaderDataGraph::loaded_cld_do(&cl);
      clka.add_all(all_loaders);
    }
    TraceTime tt("Sort klasses", TRACETIME_LOG(Info, jbooster, aot));
    sort_klasses_by_inheritance(all_sorted_klasses, array_klasses, klasses_to_compile,
                                /* reverse_scan_src */ true, CHECK);
  }
  log_info(jbooster, compilation)("Klasses to send: %d", all_sorted_klasses->length());
  log_info(jbooster, compilation)("Klasses to compile: %d", klasses_to_compile->length());
}

static Handle create_hash_set_instance(InstanceKlass** ik, TRAPS) {
  TempNewSymbol hash_set_name = SymbolTable::new_symbol("java/util/HashSet");
  Klass* hash_set_k = SystemDictionary::resolve_or_fail(hash_set_name, Handle(), Handle(), true, CHECK_NH);
  assert(hash_set_k != nullptr && hash_set_k->is_instance_klass(), "sanity");
  InstanceKlass* hash_set_ik = InstanceKlass::cast(hash_set_k);
  *ik = hash_set_ik;
  Handle hash_set_h = JavaCalls::construct_new_instance(hash_set_ik,
                                                        vmSymbols::void_method_signature(),
                                                        CHECK_NH);
  return hash_set_h;
}

static Handle add_klasses_to_java_hash_set(GrowableArray<InstanceKlass*>* klasses, TRAPS) {
  DebugUtils::assert_thread_in_vm();

  // create a HashSet object
  InstanceKlass* hash_set_ik = nullptr;
  Handle hash_set_h = create_hash_set_instance(&hash_set_ik, CHECK_NH);

  // add all klasses to the hash set
  for (GrowableArrayIterator<InstanceKlass*> iter = klasses->begin(); iter != klasses->end(); ++iter) {
    JavaValue result(T_BOOLEAN);
    JavaCalls::call_virtual(&result, hash_set_h, hash_set_ik,
                            vmSymbols::add_method_name(),
                            vmSymbols::object_boolean_signature(),
                            Handle(THREAD, (*iter)->java_mirror()),
                            CHECK_NH);
  }
  return hash_set_h;
}

static Handle add_methods_names_to_java_hash_set(GrowableArray<Method*>* methods, TRAPS) {
  DebugUtils::assert_thread_in_vm();

  // create a HashSet object
  InstanceKlass* hash_set_ik = nullptr;
  Handle hash_set_h = create_hash_set_instance(&hash_set_ik, CHECK_NH);

  // add all names of methods to the hash set
  for (GrowableArrayIterator<Method*> iter = methods->begin(); iter != methods->end(); ++iter) {
    ResourceMark rm(THREAD);
    Method* m = *iter;
    const char* s = m->name_and_sig_as_C_string();
    Handle s_h = java_lang_String::create_from_str(s, CHECK_NH);

    JavaValue result(T_BOOLEAN);
    JavaCalls::call_virtual(&result, hash_set_h, hash_set_ik,
                            vmSymbols::add_method_name(),
                            vmSymbols::object_boolean_signature(),
                            s_h, CHECK_NH);
  }
  return hash_set_h;
}

bool LazyAOT::compile_classes_by_graal(int session_id,
                                       const char* file_path,
                                       GrowableArray<InstanceKlass*>* klasses,
                                       bool use_pgo,
                                       bool resolve_no_extra_klasses,
                                       TRAPS) {
  DebugUtils::assert_thread_in_vm();

  // arg2
  Handle file_path_h = java_lang_String::create_from_str(file_path, CHECK_false);

  // arg3
  Handle hash_set_h = add_klasses_to_java_hash_set(klasses, CHECK_false);

  JavaValue result(T_BOOLEAN);
  JavaCallArguments java_args;
  java_args.push_int(session_id);
  java_args.push_oop(file_path_h);
  java_args.push_oop(hash_set_h);
  java_args.push_int(use_pgo);
  java_args.push_int(resolve_no_extra_klasses);

  TempNewSymbol compile_classes_name = SymbolTable::new_symbol("compileClasses");
  TempNewSymbol compile_classes_signature = SymbolTable::new_symbol("(ILjava/lang/String;Ljava/util/Set;ZZ)Z");
  JavaCalls::call_static(&result, ServerDataManager::get().main_klass(),
                         compile_classes_name,
                         compile_classes_signature,
                         &java_args, CHECK_false);
  return (bool) result.get_jboolean();
}

bool LazyAOT::compile_methods_by_graal(int session_id,
                                       const char* file_path,
                                       GrowableArray<InstanceKlass*>* klasses,
                                       GrowableArray<Method*>* methods_to_compile,
                                       GrowableArray<Method*>* methods_not_compile,
                                       bool use_pgo,
                                       bool resolve_no_extra_klasses,
                                       TRAPS) {
  DebugUtils::assert_thread_in_vm();

  // arg2
  Handle file_path_h = java_lang_String::create_from_str(file_path, CHECK_false);

  // arg3
  Handle klass_set_h = add_klasses_to_java_hash_set(klasses, CHECK_false);

  // arg4
  Handle method_name_set_h = add_methods_names_to_java_hash_set(methods_to_compile, CHECK_false);

  // arg5
  Handle not_method_name_set_h = add_methods_names_to_java_hash_set(methods_not_compile, CHECK_false);

  JavaValue result(T_BOOLEAN);
  JavaCallArguments java_args;
  java_args.push_int(session_id);
  java_args.push_oop(file_path_h);
  java_args.push_oop(klass_set_h);
  java_args.push_oop(method_name_set_h);
  java_args.push_oop(not_method_name_set_h);
  java_args.push_int(use_pgo);
  java_args.push_int(resolve_no_extra_klasses);

  TempNewSymbol compile_methods_name = SymbolTable::new_symbol("compileMethods");
  TempNewSymbol compile_methods_signature = SymbolTable::new_symbol("(ILjava/lang/String;Ljava/util/Set;Ljava/util/Set;Ljava/util/Set;ZZ)Z");
  JavaCalls::call_static(&result, ServerDataManager::get().main_klass(),
                         compile_methods_name,
                         compile_methods_signature,
                         &java_args, CHECK_false);
  return (bool) result.get_jboolean();
}
