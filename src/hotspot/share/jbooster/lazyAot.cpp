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
#include "jbooster/utilities/ptrHashSet.inline.hpp"
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
        log_trace(jbooster, compilation)("Class in constant pool not found: "
                                         "class=\"%s\", loader=\"%s\", holder=\"%s\"",
                                         name->as_C_string(),
                                         ClassLoaderData::class_loader_data_or_null(loader())
                                                          ->loader_name(),
                                         cp->pool_holder()->internal_name());
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
        ResourceMark rm;
        log_trace(jbooster, compilation)("Class in constant pool not found: "
                                         "method=\"%s\", loader=\"%s\", holder=\"%s\"",
                                         sym->as_C_string(),
                                         ClassLoaderData::class_loader_data_or_null(loader())
                                                          ->loader_name(),
                                         cp->pool_holder()->internal_name());
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
                                             PtrHashSet<InstanceKlass*>* vis,
                                             InstanceKlass* ik,
                                             TRAPS) {
  if (!vis->add(ik)) return;
  InstanceKlass* super = ik->java_super();
  if (super != nullptr) {
    collect_klasses_by_inheritance(dst, vis, super, THREAD);
  }
  Array<InstanceKlass*>* interfaces = ik->local_interfaces();
  for (int i = 0; i < interfaces->length(); ++i) {
    InstanceKlass* interface = interfaces->at(i);
    collect_klasses_by_inheritance(dst, vis, interface, THREAD);
  }
  dst->append(ik);
}

void LazyAOT::collect_klasses_in_constant_pool(GrowableArray<InstanceKlass*>* dst,
                                               PtrHashSet<InstanceKlass*>* vis,
                                               InstanceKlass* ik,
                                               int which_klasses,
                                               TRAPS) {
  constantPoolHandle cp(THREAD, ik->constants());
  Handle class_loader(THREAD, ik->class_loader());
  for (int i = 0; i < cp->length(); ++i) {
    constantTag tag = cp->tag_at(i);
    if (tag.is_unresolved_klass() || tag.is_klass()) {
      InstanceKlass* next = get_klass_from_class_tag(cp, i, class_loader, which_klasses, THREAD);
      if (next != nullptr) {  // next is null when it is not a InstanceKlass
        collect_klasses_by_inheritance(dst, vis, next, THREAD);
      }
    } else if (tag.is_name_and_type()) {
      GrowableArray<InstanceKlass*> next_list;
      get_klasses_from_name_and_type_tag(cp, i, class_loader, next_list, which_klasses, THREAD);
      for (GrowableArrayIterator<InstanceKlass*> iter = next_list.begin();
                                                 iter != next_list.end();
                                                 ++iter) {
        collect_klasses_by_inheritance(dst, vis, *iter, THREAD);
      }
    }
  }
}

void LazyAOT::collect_klasses_in_constant_pool(GrowableArray<InstanceKlass*>* dst,
                                               PtrHashSet<InstanceKlass*>* vis,
                                               TRAPS) {
  int last_len = 0;
  for (int lvl = _compilation_related_klasses_layers; lvl > 0; --lvl) {
    int len = dst->length();
    for (int i = last_len; i < len; ++i) {
      ThreadInVMfromNative tiv(THREAD);
      collect_klasses_in_constant_pool(dst, vis, dst->at(i), ALL_KLASSES, THREAD);
    }
    last_len = len;
  }
}

void LazyAOT::collect_klasses_in_method_data(GrowableArray<InstanceKlass*>* dst_ik,
                                             GrowableArray<ArrayKlass*>* dst_ak,
                                             PtrHashSet<InstanceKlass*>* ik_vis,
                                             TRAPS) {
  int last_len = 0;
  PtrHashSet<ArrayKlass*> ak_vis;
  for (int lvl = _compilation_related_klasses_layers; lvl > 0; --lvl) {
    int len = dst_ik->length();
    for (int i = last_len; i < len; ++i) {
      Array<Method*>* methods = dst_ik->at(i)->methods();
      for (int j = 0; j < methods->length(); j++) {
        MethodData* method_data = methods->at(j)->method_data();
        if (method_data != nullptr) {
          collect_klasses_in_method_data(dst_ik, dst_ak, ik_vis, &ak_vis,
                                         method_data, ALL_KLASSES, THREAD);
        }
      }
    }
    last_len = len;
  }
}

void LazyAOT::collect_klasses_in_method_data(GrowableArray<InstanceKlass*>* dst_ik,
                                             GrowableArray<ArrayKlass*>* dst_ak,
                                             PtrHashSet<InstanceKlass*>* ik_vis,
                                             PtrHashSet<ArrayKlass*>* ak_vis,
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
      collect_klasses_by_inheritance(dst_ik, ik_vis, *iter, THREAD);
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

bool LazyAOT::sort_klasses_by_inheritance(GrowableArray<InstanceKlass*>* dst_ik,
                                          GrowableArray<ArrayKlass*>* dst_ak,
                                          GrowableArray<InstanceKlass*>* src,
                                          bool reverse_scan_src,
                                          TRAPS) {
  PtrHashSet<InstanceKlass*> visited;
  if (reverse_scan_src) {
    for (int i = src->length() - 1; i >= 0; --i) {
      collect_klasses_by_inheritance(dst_ik, &visited, src->at(i), THREAD);
    }
  } else {
    for (GrowableArrayIterator<InstanceKlass*> iter = src->begin();
                                               iter != src->end();
                                               ++iter) {
      collect_klasses_by_inheritance(dst_ik, &visited, *iter, THREAD);
    }
  }
  collect_klasses_in_constant_pool(dst_ik, &visited, THREAD);
  collect_klasses_in_method_data(dst_ik, dst_ak, &visited, THREAD);
  return true;
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
  oop loader_oop = cld->class_loader();

  // Skip the bootstrap loaders used by LambdaForm.
  // Each LambdaForm has its own bootstrap class loader.
  // @see ClassLoaderData::is_boot_class_loader_data()
  if (cld->is_the_null_class_loader_data()) return true;
  if (loader_oop == nullptr) return false;

  // Skip klasses like GeneratedMethodAccessor, GeneratedConstructorAccessor
  // and GeneratedSerializationConstructorAccessor.
  if (loader_oop->is_a(vmClasses::reflect_DelegatingClassLoader_klass())) {
    return false;
  }

  return true;
}

class KlassGetAllInstanceKlassesClosure: public KlassClosure {
  GrowableArray<InstanceKlass*>* _klasses;
  GrowableArray<Method*>* _methods_to_compile;
  GrowableArray<Method*>* _methods_not_compile;

public:
  KlassGetAllInstanceKlassesClosure(GrowableArray<InstanceKlass*>* klasses,
                                    GrowableArray<Method*>* methods_to_compile,
                                    GrowableArray<Method*>* methods_not_compile):
                                    _klasses(klasses),
                                    _methods_to_compile(methods_to_compile),
                                    _methods_not_compile(methods_not_compile) {}

  void do_klass(Klass* k) override {
    if (!k->is_instance_klass()) return;
    InstanceKlass* ik = InstanceKlass::cast(k);
    if (!ik->is_loaded()) return;

    if (PrintAllClassInfo) {
      ResourceMark rm;
      Symbol* class_name = ik->name();
      const char* class_name_c = class_name == nullptr ? nullptr
                               : class_name->as_C_string();
      tty->print_cr("        %s:%p", class_name_c, ik);
    }

    // Maybe we should add "if (!ik->is_initialized()) return;".
    if (!LazyAOT::can_be_compiled(ik, /* check_cld */ false)) return;

    bool should_append_klass = false;
    Array<Method*>* methods = ik->methods();
    int len = methods->length();
    for (int i = 0; i < len; i++) {
      Method* m = methods->at(i);

      bool should_compile = m->has_compiled_code();

      if (should_compile) {
        _methods_to_compile->append(m);
        should_append_klass = true;
      }

      if (m->is_rewrite_invokehandle()) {
        _methods_not_compile->append(m);
      }
    }

    if (should_append_klass) {
      _klasses->append(ik);
    }
  }
};

class CLDGetAllInstanceKlassesClosure: public CLDClosure {
  GrowableArray<ClassLoaderData*>* _loaders;
  GrowableArray<InstanceKlass*>* _klasses;
  GrowableArray<Method*>* _methods_to_compile;
  GrowableArray<Method*>* _methods_not_compile;

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
    if (LazyAOT::can_be_compiled(cld)) {
      if (_loaders != nullptr) _loaders->append(cld);
      KlassGetAllInstanceKlassesClosure cl(_klasses, _methods_to_compile, _methods_not_compile);
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
                                  _methods_not_compile(methods_not_compile) {}

  void do_cld(ClassLoaderData* cld) override { for_each(cld); }
};

void LazyAOT::collect_all_klasses_to_compile(GrowableArray<ClassLoaderData*>* all_loaders,
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
      ClassLoaderDataGraph::cld_do(&cl);
    }
    TraceTime tt("Sort klasses", TRACETIME_LOG(Info, jbooster, aot));
    sort_klasses_by_inheritance(all_sorted_klasses, array_klasses, klasses_to_compile,
                                /* reverse_scan_src */ true, CATCH);
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
    guarantee((bool)result.get_jboolean() == true, "sanity");
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
    guarantee((bool)result.get_jboolean() == true, "sanity");
  }
  return hash_set_h;
}

bool LazyAOT::compile_classes_by_graal(int session_id,
                                       const char* file_path,
                                       GrowableArray<InstanceKlass*>* klasses,
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

  TempNewSymbol compile_classes_name = SymbolTable::new_symbol("compileClasses");
  TempNewSymbol compile_classes_signature = SymbolTable::new_symbol("(ILjava/lang/String;Ljava/util/Set;)Z");
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

  TempNewSymbol compile_methods_name = SymbolTable::new_symbol("compileMethods");
  TempNewSymbol compile_methods_signature = SymbolTable::new_symbol("(ILjava/lang/String;Ljava/util/Set;Ljava/util/Set;Ljava/util/Set;)Z");
  JavaCalls::call_static(&result, ServerDataManager::get().main_klass(),
                         compile_methods_name,
                         compile_methods_signature,
                         &java_args, CHECK_false);
  return (bool) result.get_jboolean();
}
