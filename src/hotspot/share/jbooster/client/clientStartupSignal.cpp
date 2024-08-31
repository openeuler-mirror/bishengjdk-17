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

#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "jbooster/client/clientStartupSignal.hpp"
#include "jbooster/jBoosterManager.hpp"
#include "jbooster/utilities/debugUtils.inline.hpp"
#include "memory/oopFactory.hpp"
#include "oops/typeArrayOop.inline.hpp"
#include "runtime/arguments.hpp"
#include "runtime/atomic.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "utilities/stringUtils.hpp"

volatile bool ClientStartupSignal::_startup_class_found = false;

Symbol* ClientStartupSignal::_klass_name_sym = nullptr;

const char* ClientStartupSignal::_klass_name = nullptr;
const char* ClientStartupSignal::_method_name = nullptr;
const char* ClientStartupSignal::_method_signature = nullptr;

static void guarantee_good_format(bool good) {
  if (good) return;
  vm_exit_during_initialization(err_msg(
      "Invalid format: JBoosterStartupSignal=\"%s\".", JBoosterStartupSignal));
}

/**
 * JBoosterStartupSignal follow the format of Method::name_and_sig_as_C_string():
 *   "java.lang.String.matches(Ljava/lang/String;)Z"
 * This is also ok (but make sure that the method is not overloaded):
 *   "java.lang.String.matches"
 */
void ClientStartupSignal::init_x_flag() {
  char* s = StringUtils::copy_to_heap(JBoosterStartupSignal, mtJBooster);
  int len = strlen(s);
  int ke = 0; // klass end
  int lb = 0, rb = 0;
  guarantee_good_format(len >= 5 && s[0] != '.' && s[0] != '/' && s[0] != '(');
  for (int i = 0; i < len; ++i) {
    switch (s[i]) {
      case '.': s[i] = '/'; // no break here
      case '/': if (lb == 0) ke = i; break;
      case '(':
        guarantee_good_format(lb == 0);
        lb = i;
        break;
      case ')':
        guarantee_good_format(lb != 0 && rb == 0);
        rb = i;
        break;
      default: break;
    }
  }

  if (lb == 0) {
    _method_signature = nullptr;
  } else {
    _method_signature = StringUtils::copy_to_heap(s + lb, mtJBooster);
    s[lb] = '\0';
  }
  _method_name = StringUtils::copy_to_heap(s + ke + 1, mtJBooster);
  s[ke] = '\0';
  _klass_name = StringUtils::copy_to_heap(s, mtJBooster);

  StringUtils::free_from_heap(s);

  if (log_is_enabled(Trace, jbooster)) {
    log_trace(jbooster)("startup_method_klass=%s", _klass_name);
    log_trace(jbooster)("startup_method_method=%s", _method_name);
    log_trace(jbooster)("startup_method_signature=%s", _method_signature);
  }
}

void ClientStartupSignal::init_d_flag() {
  jint res = Arguments::init_jbooster_startup_signal_properties(_klass_name,
                                                                _method_name,
                                                                _method_signature);
  if (res != JNI_OK) {
    vm_exit_during_initialization("Failed to set -D flags for JBoosterStartupSignal");
  }
}

void ClientStartupSignal::init_phase1() {
  JBoosterManager::client_only();
  assert(JBoosterStartupSignal != nullptr, "sanity");
  init_x_flag();
  init_d_flag();
}

void ClientStartupSignal::init_phase2() {
  JBoosterManager::client_only();
  assert(JBoosterStartupSignal != nullptr, "sanity");
  _klass_name_sym = SymbolTable::new_symbol(_klass_name);
}

void ClientStartupSignal::free() {
  StringUtils::free_from_heap(_klass_name);
  StringUtils::free_from_heap(_method_name);
  StringUtils::free_from_heap(_method_signature);

  if (_klass_name_sym != nullptr) {
    _klass_name_sym->decrement_refcount();
    _klass_name_sym = nullptr;
  }
}

bool ClientStartupSignal::inject_startup_callback(unsigned char** data_ptr, int* data_len, TRAPS) {
  int len = *data_len;

  // arg1
  typeArrayOop bytes_oop = oopFactory::new_byteArray(len, CHECK_false);
  typeArrayHandle bytes_h(THREAD, bytes_oop);
  const jbyte* src = reinterpret_cast<const jbyte*>(*data_ptr);
  ArrayAccess<>::arraycopy_from_native(src, bytes_h(), typeArrayOopDesc::element_offset<jbyte>(0), len);

  // arg 2
  Handle method_name_h = java_lang_String::create_from_str(_method_name, CHECK_false);

  // arg 3
  Handle method_signature_h = _method_signature == nullptr
                            ? Handle()
                            : java_lang_String::create_from_str(_method_signature, CHECK_false);

  // wrap args
  JavaValue result(T_ARRAY);
  JavaCallArguments java_args;
  java_args.push_oop(bytes_h);
  java_args.push_oop(method_name_h);
  java_args.push_oop(method_signature_h);

  // callee klass
  TempNewSymbol ss_name = SymbolTable::new_symbol("jdk/jbooster/api/JBoosterStartupSignal");
  Klass* ss_k = SystemDictionary::resolve_or_null(
          ss_name,
          Handle(THREAD, SystemDictionary::java_platform_loader()),
          Handle(), CHECK_false);
  guarantee(ss_k != nullptr && ss_k->is_instance_klass(), "sanity");
  InstanceKlass* ss_ik = InstanceKlass::cast(ss_k);

  // call it
  TempNewSymbol inject_method_name = SymbolTable::new_symbol("injectStartupCallback");
  TempNewSymbol inject_method_signature = SymbolTable::new_symbol("([BLjava/lang/String;Ljava/lang/String;)[B");
  JavaCalls::call_static(&result,
                         ss_ik,
                         inject_method_name,
                         inject_method_signature,
                         &java_args, CHECK_false);
  typeArrayHandle res_bytes_h(THREAD, (typeArrayOop) result.get_oop());
  if (res_bytes_h.is_null()) return false;

  // copy java byte array to cpp
  unsigned char* res_bytes_base = reinterpret_cast<unsigned char*>(res_bytes_h->byte_at_addr(0));
  int res_len = res_bytes_h->length();

  unsigned char* res_bytes = NEW_RESOURCE_ARRAY(unsigned char, res_len);
  memcpy(res_bytes, res_bytes_base, res_len);

  *data_ptr = res_bytes;
  *data_len = res_len;
  return true;
}

bool ClientStartupSignal::try_inject_startup_callback(unsigned char** data_ptr, int* data_len, TRAPS) {
  bool success = false;
  if (Atomic::cmpxchg(&_startup_class_found, false, true) == /* prior value */ false) {
    success = inject_startup_callback(data_ptr, data_len, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      success = false;
    }
  }

  if (success) {
    log_info(jbooster, startup)("Successfully injected startup callback to: klass=%s, method=%s%s.",
                                ClientStartupSignal::klass_name(),
                                ClientStartupSignal::method_name(),
                                ClientStartupSignal::method_signature());
  } else {
    log_warning(jbooster, startup)("Failed to inject startup callback to: klass=%s, method=%s%s.",
                                   ClientStartupSignal::klass_name(),
                                   ClientStartupSignal::method_name(),
                                   ClientStartupSignal::method_signature());
    if (HAS_PENDING_EXCEPTION) {
      LogTarget(Warning, jbooster, startup) lt;
      DebugUtils::clear_java_exception_and_print_stack_trace(lt, THREAD);
    }
  }

  return success;
}
