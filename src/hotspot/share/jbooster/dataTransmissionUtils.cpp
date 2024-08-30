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
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "jbooster/dataTransmissionUtils.hpp"
#include "jbooster/net/serializationWrappers.inline.hpp"
#include "jbooster/net/serverStream.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/utilities/debugUtils.inline.hpp"
#include "oops/methodData.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/interfaceSupport.inline.hpp"

// -------------------------------- ClassLoaderKey ---------------------------------

ClassLoaderKey::ClassLoaderKey(const ClassLoaderKey& key):
        _loader_class_name(key._loader_class_name),
        _loader_name(key._loader_name),
        _first_loaded_class_name(key._first_loaded_class_name) {
  // change ref only for the server
  if (AsJBooster) {
    if (_loader_class_name != nullptr) {
      _loader_class_name->increment_refcount();
    }
    if (_loader_name != nullptr) {
      _loader_name->increment_refcount();
    }
    if (_first_loaded_class_name != nullptr) {
      _first_loaded_class_name->increment_refcount();
    }
  }
}

ClassLoaderKey::~ClassLoaderKey() {
  if (AsJBooster) {
    if (_loader_class_name != nullptr) {
      _loader_class_name->decrement_refcount();
    }
    if (_loader_name != nullptr) {
      _loader_name->decrement_refcount();
    }
    if (_first_loaded_class_name != nullptr) {
      _first_loaded_class_name->decrement_refcount();
    }
  }
}

ClassLoaderKey& ClassLoaderKey::operator = (const ClassLoaderKey& other) {
  if (AsJBooster) {
    if (_loader_class_name != nullptr) {
      _loader_class_name->decrement_refcount();
    }
    if (_loader_name != nullptr) {
      _loader_name->decrement_refcount();
    }
    if (_first_loaded_class_name != nullptr) {
      _first_loaded_class_name->decrement_refcount();
    }
  }

  _loader_class_name = other._loader_class_name;
  _loader_name = other._loader_name;
  _first_loaded_class_name = other._first_loaded_class_name;

  if (AsJBooster) {
    if (_loader_class_name != nullptr) {
      _loader_class_name->increment_refcount();
    }
    if (_loader_name != nullptr) {
      _loader_name->increment_refcount();
    }
    if (_first_loaded_class_name != nullptr) {
      _first_loaded_class_name->increment_refcount();
    }
  }
  return *this;
}

ClassLoaderKey ClassLoaderKey::boot_loader_key() {
  return ClassLoaderKey(nullptr, nullptr, nullptr);
}
bool ClassLoaderKey::is_boot_loader() const {
  return _loader_class_name == nullptr
      && _loader_name == nullptr
      && _first_loaded_class_name == nullptr;
}
ClassLoaderKey ClassLoaderKey::platform_loader_key() {
  return ClassLoaderKey(vmSymbols::jdk_internal_loader_ClassLoaders_PlatformClassLoader(), nullptr, nullptr);
}
bool ClassLoaderKey::is_platform_loader() const {
  return _loader_class_name == vmSymbols::jdk_internal_loader_ClassLoaders_PlatformClassLoader()
      && _loader_name == nullptr
      && _first_loaded_class_name == nullptr;
}

ClassLoaderKey ClassLoaderKey::key_of_class_loader_data(ClassLoaderData* cld) {
  if (cld->is_boot_class_loader_data()) {
    return boot_loader_key();
  } else if (cld->is_platform_class_loader_data()) {
    return platform_loader_key();
  }

  Klass* loader_class = cld->class_loader_klass();
  Symbol* loader_class_name = loader_class == nullptr ? nullptr : loader_class->name();
  Symbol* loader_name = cld->name();
  Klass* first_klass = cld->klasses();
  if (first_klass == nullptr) {
    return ClassLoaderKey(loader_class_name, loader_name, nullptr);
  }

  while (first_klass->next_link() != nullptr) {
    first_klass = first_klass->next_link();
  }
  Symbol* first_loaded_class_name = first_klass->name();
  return ClassLoaderKey(loader_class_name, loader_name, first_loaded_class_name);
}

// ------------------------------- ClassLoaderChain --------------------------------

void ClassLoaderChain::parse_cld() const {
  if (!_chain.is_empty()) return; // already parsed

  JavaThread* THREAD = JavaThread::current();
  ClassLoaderData* cld = _cld;
  do {
    assert(cld != nullptr, "sanity");
    ClassLoaderKey key = ClassLoaderKey::key_of_class_loader_data(cld);
    _chain.append(ClassLoaderChain::Node(key, (uintptr_t) cld));
    if (cld->is_boot_class_loader_data()) break;
    Handle cur_h(THREAD, cld->class_loader());
    Handle parent_h(THREAD, java_lang_ClassLoader::parent(cur_h()));
    cld = ClassLoaderData::class_loader_data_or_null(parent_h());
    if (cld == nullptr) {
      // [JBOOSTER TODO] this is a temp solution
      cld = ClassLoaderData::the_null_class_loader_data();
    }
  } while (true);
}

int ClassLoaderChain::serialize(MessageBuffer& buf) const {
  parse_cld();
  JB_RETURN(buf.serialize_no_meta(_chain.length()));
  for (GrowableArrayIterator<Node> it = _chain.begin(); it != _chain.end(); ++it) {
    uintptr_t addr = (*it).client_cld_addr;
    ClassLoaderKey key = (*it).key;
    JB_RETURN(buf.serialize_no_meta(addr));
    JB_RETURN(buf.serialize_with_meta(key.loader_class_name()));
    JB_RETURN(buf.serialize_with_meta(key.loader_name()));
    JB_RETURN(buf.serialize_with_meta(key.first_loaded_class_name()));
  }
  return 0;
}

int ClassLoaderChain::deserialize(MessageBuffer& buf) {
  assert(_chain.is_empty(), "sanity");
  int size;
  JB_RETURN(buf.deserialize_ref_no_meta(size));
  for (int i = 0; i < size; ++i) {
    uintptr_t addr;
    Symbol* loader_class_name = nullptr;
    Symbol* loader_name = nullptr;
    Symbol* first_loaded_class_name = nullptr;
    JB_RETURN(buf.deserialize_ref_no_meta(addr));
    JB_RETURN(buf.deserialize_with_meta(loader_class_name));
    JB_RETURN(buf.deserialize_with_meta(loader_name));
    JB_RETURN(buf.deserialize_with_meta(first_loaded_class_name));
    _chain.append(ClassLoaderChain::Node(ClassLoaderKey(loader_class_name, loader_name, first_loaded_class_name), addr));
  }
  return 0;
}

// ------------------------------ ClassLoaderLocator -------------------------------

static ClassLoaderData* cld_of_cl_oop(oop class_loader_oop) {
  return ClassLoaderData::class_loader_data_or_null(class_loader_oop);
}

ClassLoaderLocator::ClassLoaderLocator(Handle class_loader_h):
                    ClassLoaderLocator(cld_of_cl_oop(class_loader_h())) {}

void ClassLoaderLocator::set_class_loader_data(Handle class_loader_h) {
  _class_loader_data = cld_of_cl_oop(class_loader_h());
}

int ClassLoaderLocator::serialize(MessageBuffer& buf) const {
  if (UseJBooster) {
    JB_RETURN(buf.serialize_no_meta((uintptr_t) _class_loader_data));
  } else if (AsJBooster) {
    if (_class_loader_data != nullptr) {
      Thread* THREAD = Thread::current();
      address client_addr = ((ServerStream*) buf.stream())->session_data()
              ->client_cld_address(_class_loader_data, THREAD);
      _client_cld_address = (uintptr_t) client_addr;
      guarantee(_client_cld_address != 0, "client CLD address not in server");
    } else {
      guarantee(_client_cld_address != 0, "client CLD address is not specified");
    }
    JB_RETURN(buf.serialize_no_meta(_client_cld_address));
  } else fatal("Not jbooster environment?");
  return 0;
}

int ClassLoaderLocator::deserialize(MessageBuffer& buf) {
  uintptr_t client_addr;
  JB_RETURN(buf.deserialize_ref_no_meta(client_addr));
  if (UseJBooster) {
    _class_loader_data = (ClassLoaderData*) client_addr;
  } else if (AsJBooster) {
    _client_cld_address = client_addr;
    recheck_class_loader_data(buf);
  } else fatal("Not jbooster environment?");
  return 0;
}

ClassLoaderData* ClassLoaderLocator::recheck_class_loader_data(MessageBuffer& buf) {
  Thread* THREAD = Thread::current();
  // _class_loader_data can be null if the mapping is not found.
  _class_loader_data = ((ServerStream*) buf.stream())->session_data()
          ->server_cld_address((address) _client_cld_address, THREAD);
  return _class_loader_data;
}

// --------------------------------- KlassLocator ----------------------------------

KlassLocator::KlassLocator(InstanceKlass* klass):
        _class_name(klass->name()),
        _client_klass((uintptr_t) klass),
        _fingerprint(klass->get_stored_fingerprint()),
        _class_loader_locator(klass->class_loader_data()) {
}

int KlassLocator::serialize(MessageBuffer& buf) const {
  JB_RETURN(buf.serialize_with_meta(_class_name));
  JB_RETURN(buf.serialize_no_meta(_client_klass));
  JB_RETURN(buf.serialize_no_meta(_fingerprint));
  JB_RETURN(buf.serialize_with_meta(&_class_loader_locator));
  return 0;
}

int KlassLocator::deserialize(MessageBuffer& buf) {
  JB_RETURN(buf.deserialize_with_meta(_class_name));
  JB_RETURN(buf.deserialize_ref_no_meta(_client_klass));
  JB_RETURN(buf.deserialize_ref_no_meta(_fingerprint));
  JB_RETURN(buf.deserialize_with_meta(&_class_loader_locator));
  return 0;
}

InstanceKlass* KlassLocator::try_to_get_ik(TRAPS) {
  ClassLoaderData* cld = _class_loader_locator.class_loader_data();
  if (cld == nullptr) {
    return nullptr;
  }
  Handle loader(THREAD, cld->class_loader());
  Klass* k = SystemDictionary::resolve_or_null(_class_name, loader, Handle(), THREAD);
  if (k == nullptr || HAS_PENDING_EXCEPTION) {
    if (cld->is_boot_class_loader_data() || cld->is_platform_class_loader_data()) {
      log_warning(jbooster, serialization)("The %s class \"%s\" not found.",
                  cld->is_boot_class_loader_data() ? "boot" : "platform",
                  _class_name->as_C_string());
      if (HAS_PENDING_EXCEPTION && !PENDING_EXCEPTION->is_a(vmClasses::ClassNotFoundException_klass())) {
        LogTarget(Warning, jbooster, serialization) lt;
        DebugUtils::clear_java_exception_and_print_stack_trace(lt, THREAD);
      }
    }
    if (HAS_PENDING_EXCEPTION) {
      CLEAR_PENDING_EXCEPTION;
    }
    return nullptr;
  }
  return InstanceKlass::cast(k);
}

// --------------------------------- MethodLocator ---------------------------------

int MethodLocator::serialize(MessageBuffer& buf) const {
  JB_RETURN(buf.serialize_no_meta((uintptr_t) _method->method_holder()));
  JB_RETURN(buf.serialize_with_meta(_method->name()));
  JB_RETURN(buf.serialize_with_meta(_method->signature()));
#ifdef ASSERT
  JB_RETURN(buf.serialize_with_meta(_method->method_holder()->name()));
#endif
  return 0;
}

int MethodLocator::deserialize(MessageBuffer& buf) {
  Thread* THREAD = Thread::current();

  uintptr_t holder_ptr;
  JB_RETURN(buf.deserialize_ref_no_meta(holder_ptr));
  InstanceKlass* holder = (InstanceKlass*) ((ServerStream*) buf.stream())
          ->session_data()
          ->server_klass_address((address) holder_ptr, THREAD);

  Symbol* client_name = nullptr;
  Symbol* client_signature = nullptr;
  JB_RETURN(buf.deserialize_with_meta(client_name));
  JB_RETURN(buf.deserialize_with_meta(client_signature));
  TempNewSymbol client_name_wrapper = client_name;
  TempNewSymbol client_signature_wrapper = client_signature;

#ifdef ASSERT
  Symbol* klass_name = nullptr;
  JB_RETURN(buf.deserialize_with_meta(klass_name));
  TempNewSymbol klass_name_wrapper = klass_name;
#endif // ASSERT

  if (holder != nullptr) {
    assert(klass_name == holder->name(), "sanity");
    _method = holder->find_method(client_name, client_signature);
    if (_method == nullptr) {
      ResourceMark rm(THREAD);
      log_debug(jbooster, serialization)("Method \"%s%s\" not found in \"%s\".",
                client_name->as_C_string(),
                client_signature->as_C_string(),
                holder->internal_name());
    }
  } else {
    ResourceMark rm(THREAD);
#ifdef ASSERT
    log_debug(jbooster, serialization)("The holder of \"%s%s\" is null (should be \"%s\").",
              client_name->as_C_string(),
              client_signature->as_C_string(),
              klass_name->as_C_string());
#else
    log_debug(jbooster, serialization)("The holder of \"%s%s\" is null.",
              client_name->as_C_string(), client_signature->as_C_string());
#endif // ASSERT
    _method = nullptr;
  }
  return 0;
}

// ----------------------------- ProfileDataCollector ------------------------------

ProfileDataCollector::ProfileDataCollector(uint32_t size, InstanceKlass** klass_array_base):
        _klass_size(size),
        _klass_array_base(klass_array_base) {}

int ProfileDataCollector::serialize(MessageBuffer& buf) const {
  for (uint32_t i = 0; i < klass_size(); i++) {
    Array<Method*>* methods = klass_at(i)->methods();
    uint32_t cnt = 0;
    for (int j= 0; j < methods->length(); j++) {
      MethodData* method_data = methods->at(j)->method_data();
      if (method_data != nullptr && method_data->is_mature()) {
        ++cnt;
      }
    }
    JB_RETURN(buf.serialize_no_meta(cnt));
    for (int j= 0; j < methods->length(); j++) {
      Method* method = methods->at(j);
      MethodData* method_data = method->method_data();
      if (method_data != nullptr && method_data->is_mature()) {
        Symbol* name = method->name();
        Symbol* signature = method->signature();
        JB_RETURN(buf.serialize_with_meta(name));
        JB_RETURN(buf.serialize_with_meta(signature));
        MemoryWrapper md_mem(method_data, method_data->size_in_bytes());
        JB_RETURN(buf.serialize_no_meta(md_mem));
      }
    }
  }
  return 0;
}

int ProfileDataCollector::deserialize(MessageBuffer& buf) {
  JavaThread* THREAD = JavaThread::current();
  ResourceMark rm(THREAD);
  JClientSessionData* session_data = ((ServerStream*) buf.stream())->session_data();

  for (uint32_t i = 0; i < klass_size(); i++) {
    ThreadInVMfromNative tiv(THREAD);
    InstanceKlass* klass = klass_at(i);

    int cnt;
    JB_RETURN(buf.deserialize_ref_no_meta(cnt));
    for (int j = 0; j < cnt; ++j) {
      Symbol* name = nullptr;
      Symbol* signature = nullptr;
      JB_RETURN(buf.deserialize_with_meta(name));
      JB_RETURN(buf.deserialize_with_meta(signature));
      TempNewSymbol name_wrapper = name;
      TempNewSymbol signature_wrapper = signature;
      MemoryWrapper md_mem;
      JB_RETURN(buf.deserialize_ref_no_meta(md_mem));
      guarantee(name != nullptr, "sanity");
      guarantee(signature != nullptr, "sanity");
      guarantee(!md_mem.is_null(), "sanity");

      Method* method = klass->find_method(name, signature);
      if (method == nullptr) {
        log_warning(jbooster, serialization)("Method not found in klass: method=%s%s, klass=%s",
                                             name->as_C_string(),
                                             signature->as_C_string(),
                                             klass->internal_name());
        continue;
      }
      MethodData* method_data = MethodData::create_instance_for_jbooster(method,
                                (int) md_mem.size(), (char*) md_mem.get_memory(), CATCH);
      session_data->add_method_data((address) method, (address) method_data, THREAD);
    }
  }
  return 0;
}

InstanceKlass* ProfileDataCollector::klass_at(uint32_t index) const {
  guarantee(index < _klass_size,"sanity");
  return _klass_array_base[index];
}
