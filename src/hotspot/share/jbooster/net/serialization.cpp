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

#include "classfile/classFileStream.hpp"
#include "classfile/classLoadInfo.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmClasses.hpp"
#include "jbooster/dataTransmissionUtils.hpp"
#include "jbooster/jbooster_globals.hpp"
#include "jbooster/net/messageBuffer.inline.hpp"
#include "jbooster/net/serialization.hpp"
#include "jbooster/net/serializationWrappers.hpp"
#include "jbooster/net/serverStream.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/utilities/debugUtils.inline.hpp"
#include "memory/resourceArea.hpp"
#include "oops/arrayKlass.hpp"
#include "oops/instanceKlass.hpp"
#include "prims/jvmtiClassFileReconstituter.hpp"
#include "runtime/handles.inline.hpp"
#include "utilities/stringUtils.hpp"

// ------------------ Serializer of C-style String (i.e., char*) -------------------

int SerializationImpl<const char*>::serialize(MessageBuffer& buf, const Arg& arg) {
  if (arg == nullptr) {
    return buf.serialize_no_meta(MessageConst::NULL_PTR);
  }
  uint32_t str_len = strlen(arg);
  JB_RETURN(buf.serialize_no_meta(str_len));
  return buf.serialize_memcpy(arg, str_len);
}

int SerializationImpl<const char*>::deserialize_ref(MessageBuffer& buf, Arg& arg) {
  char* str = nullptr;
  JB_RETURN(buf.deserialize_ref_no_meta<char*>(str));
  arg = str;
  return 0;
}

int SerializationImpl<char*>::serialize(MessageBuffer& buf, const Arg& arg) {
  const char* str = arg;
  return buf.serialize_no_meta<const char*>(arg);
}

int SerializationImpl<char*>::deserialize_ref(MessageBuffer& buf, Arg& arg) {
  guarantee(arg == nullptr, "do not pre-allocate the string");
  uint32_t str_len;
  JB_RETURN(buf.deserialize_ref_no_meta(str_len));
  if (str_len == MessageConst::NULL_PTR) {
    return 0;
  }
  char* s = NEW_RESOURCE_ARRAY(char, str_len + 1);
  JB_RETURN(buf.deserialize_memcpy(s, str_len));
  s[str_len] = '\0';
  arg = s;
  return 0;
}

// ----------------------------- Serializer for Symbol -----------------------------

int SerializationImpl<Symbol>::serialize(MessageBuffer& buf, const Symbol& arg) {
  JB_RETURN(buf.serialize_no_meta(arg.utf8_length()));
  return buf.serialize_memcpy(arg.base(), arg.utf8_length());
}

int SerializationImpl<Symbol>::deserialize_ptr(MessageBuffer& buf, Symbol*& arg_ptr) {
  int len;
  JB_RETURN(buf.deserialize_ref_no_meta(len));
  arg_ptr = SymbolTable::new_symbol(buf.cur_buf_ptr(), len);
  buf.skip_cur_offset((uint32_t) len);
  return 0;
};

// ------------------------- Serializer for InstanceKlass --------------------------
// We rebuild the InstanceKlass based on the following method:
// InstanceKlass* SystemDictionary::resolve_from_stream(
//         ClassFileStream* st, Symbol* class_name, Handle class_loader,
//         const ClassLoadInfo& cl_info, TRAPS)

int SerializationImpl<InstanceKlass>::serialize(MessageBuffer& buf, const InstanceKlass& arg) {
  assert(UseJBooster, "only for client klass");

  // client-side klass pointer
  JB_RETURN(buf.serialize_no_meta((uintptr_t) &arg));

  // [arg of resolve_from_stream] Symbol* class_name
  JB_RETURN(buf.serialize_with_meta(arg.name()));

  // [arg of resolve_from_stream] Handle class_loader
  ClassLoaderLocator cll(arg.class_loader_data());
  JB_RETURN(buf.serialize_no_meta(cll));

  // [arg of resolve_from_stream] const ClassLoadInfo& cl_info (ignored for now)
  JB_RETURN(buf.serialize_no_meta((int) 0x1c2d3e4f));

  // [arg of resolve_from_stream] ClassFileStream* st
  {
    JavaThread* THREAD = JavaThread::current();
    ResourceMark rm(THREAD);
    HandleMark hm(THREAD);

    char* cf_buf = nullptr;
    uint32_t cf_size = 0;

    bool should_send_class_file = !arg.class_loader_data()->is_boot_class_loader_data();
    if (should_send_class_file) {
      InstanceKlass* ik = const_cast<InstanceKlass*>(&arg);
      JvmtiClassFileReconstituter reconstituter(ik);
      if (reconstituter.get_error() == JVMTI_ERROR_NONE) {
        cf_buf = (char*) reconstituter.class_file_bytes();
        cf_size = (uint32_t) reconstituter.class_file_size();
      }
    }
    MemoryWrapper mw((void*) cf_buf, cf_size);
    JB_RETURN(buf.serialize_no_meta(mw));
  }

  return 0;
}

static InstanceKlass* resolve_instance_klass(Symbol* class_name,
                                             ClassLoaderData* class_loader_data,
                                             Handle protection_domain,
                                             TRAPS) {
  Handle class_loader(THREAD, class_loader_data->class_loader());
  Klass* result = SystemDictionary::resolve_or_null(class_name, class_loader,
                                                    protection_domain, THREAD);
  if (HAS_PENDING_EXCEPTION) {
    log_warning(jbooster, serialization)("Failed to deserialize and resolve InstanceKlass \"%s\" "
                                         "(loader=\"%s:%p\"): see stack trace.",
                                         class_name->as_C_string(),
                                         class_loader_data->loader_name(),
                                         class_loader_data);
    LogTarget(Warning, jbooster, serialization) lt;
    DebugUtils::clear_java_exception_and_print_stack_trace(lt, THREAD);
    return nullptr;
  } else if (result == nullptr) {
    log_warning(jbooster, serialization)("Failed to deserialize InstanceKlass \"%s\" "
                                         "(loader=\"%s:%p\"): no class file found.",
                                         class_name->as_C_string(),
                                         class_loader_data->loader_name(),
                                         class_loader_data);
    return nullptr;
  }
  return InstanceKlass::cast(result);
}

static InstanceKlass* rebuild_instance_klass(Symbol* class_name,
                                             ClassLoaderData* class_loader_data,
                                             ClassLoadInfo& cl_info,
                                             u1* class_file_buf,
                                             int class_file_size,
                                             TRAPS) {
  Handle class_loader(THREAD, class_loader_data->class_loader());
  ClassFileStream st(class_file_buf, class_file_size, "__VM_JBoosterDeserialization__");
  InstanceKlass* result = SystemDictionary::resolve_from_stream(&st, class_name, class_loader,
                                                                cl_info, THREAD);
  if (result == nullptr || HAS_PENDING_EXCEPTION) {
    assert(HAS_PENDING_EXCEPTION, "sanity");
    Handle first_ex(THREAD, THREAD->pending_exception());
    if (PENDING_EXCEPTION->is_a(vmClasses::LinkageError_klass())) {
      // The class loader attempted duplicate class definition for the target class.
      Handle ex(THREAD, PENDING_EXCEPTION);
      const char* efile = ((ThreadShadow*)THREAD)->exception_file();
      int         eline = ((ThreadShadow*)THREAD)->exception_line();
      CLEAR_PENDING_EXCEPTION;
      Klass* found = SystemDictionary::find_instance_klass(class_name, class_loader, Handle());
      if (found != nullptr) {
        result = InstanceKlass::cast(found);
      }
    }
    // still null
    if (result == nullptr) {
      LogTarget(Warning, jbooster, serialization) lt;
      if (lt.is_enabled()) {
        lt.print("Failed to deserialize and rebuild InstanceKlass \"%s\" "
                 "(loader=\"%s:%p\"): see stack trace.",
                 class_name->as_C_string(),
                 class_loader_data->loader_name(),
                 class_loader_data);
        if (HAS_PENDING_EXCEPTION) {
          DebugUtils::clear_java_exception_and_print_stack_trace(lt, THREAD);
        } else {
          lt.print("(A LinkageError but not caused by duplicate definition)");
          LogStream ls(lt);
          java_lang_Throwable::print_stack_trace(first_ex, &ls);
        }
      }
    }
    CLEAR_PENDING_EXCEPTION;
  }
  return result;
}

int SerializationImpl<InstanceKlass>::deserialize_ptr(MessageBuffer& buf, InstanceKlass*& arg_ptr) {
  assert(AsJBooster, "only called on the server");
  JavaThread* THREAD = JavaThread::current();
  ResourceMark rm(THREAD);

  // client-side InstanceKlass pointer
  uintptr_t client_klass;
  JB_RETURN(buf.deserialize_ref_no_meta(client_klass));

  // [arg of resolve_from_stream] Symbol* class_name
  Symbol* class_name = nullptr;
  JB_RETURN(buf.deserialize_with_meta(class_name));
  TempNewSymbol class_name_wrapper = class_name;

  // [arg of resolve_from_stream] Handle class_loader
  ClassLoaderLocator cll;
  JB_RETURN(buf.deserialize_ref_no_meta(cll));
  if (cll.class_loader_data() == nullptr) {
    log_warning(jbooster, serialization)("Failed to deserialize InstanceKlass \"%s\": "
                "cannot deserialize class loader " INTPTR_FORMAT ".",
                class_name->as_C_string(), cll.client_cld_address());
    return JBErr::DESER_TERMINATION;
  }
  ClassLoaderData* class_loader_data = cll.class_loader_data();

  // [arg of resolve_from_stream] const ClassLoadInfo& cl_info (ignored for now)
  int placeholder_num;
  JB_RETURN(buf.deserialize_ref_no_meta(placeholder_num));
  if (placeholder_num != 0x1c2d3e4f) {
    log_warning(jbooster, serialization)("Failed to deserialize klass \"%s\": "
                "wrong placeholder_num: %d.",
                class_name->as_C_string(), placeholder_num);
    return JBErr::BAD_ARG_DATA;
  }
  Handle protection_domain;
  ClassLoadInfo cl_info(protection_domain);

  // [arg of resolve_from_stream] ClassFileStream* st
  MemoryWrapper mw;
  JB_RETURN(buf.deserialize_ref_no_meta(mw));

  InstanceKlass* res = nullptr;
  if (mw.is_null()) {
    res = resolve_instance_klass(class_name, class_loader_data,
                                 cl_info.protection_domain(), THREAD);
  } else {
    res = rebuild_instance_klass(class_name, class_loader_data, cl_info,
                                 (u1*) mw.get_memory(), (int) mw.size(), THREAD);
  }
  if (res != nullptr) {
    JClientSessionData* session_data = ((ServerStream*) buf.stream())->session_data();
    session_data->add_klass_address((address) client_klass, (address) res, THREAD);
    arg_ptr = res;
  }
  return 0;
}

// --------------------------- Serializer for ArrayKlass ---------------------------
// We rebuild the klass by:
// SystemDictionary::resolve_or_null(Symbol* class_name, Handle class_loader,
//                                   Handle protection_domain, TRAPS)

int SerializationImpl<ArrayKlass>::serialize(MessageBuffer& buf, const ArrayKlass& arg) {
  assert(UseJBooster, "only for client klass");

  // client-side klass pointer
  JB_RETURN(buf.serialize_no_meta((uintptr_t) &arg));

  // class_name
  JB_RETURN(buf.serialize_with_meta(arg.name()));

  // class loader
  ClassLoaderLocator cll(arg.class_loader_data());
  JB_RETURN(buf.serialize_no_meta(cll));

  return 0;
}

int SerializationImpl<ArrayKlass>::deserialize_ptr(MessageBuffer& buf, ArrayKlass*& arg_ptr) {
  assert(AsJBooster, "only called on the server");
  JavaThread* THREAD = JavaThread::current();
  ResourceMark rm(THREAD);

  // client-side ArrayKlass pointer
  uintptr_t client_klass;
  JB_RETURN(buf.deserialize_ref_no_meta(client_klass));

  // class_name
  Symbol* class_name = nullptr;
  JB_RETURN(buf.deserialize_with_meta(class_name));
  TempNewSymbol class_name_wrapper = class_name;

  // class loader
  ClassLoaderLocator cll;
  JB_RETURN(buf.deserialize_ref_no_meta(cll));
  if (cll.class_loader_data() == nullptr) {
    log_warning(jbooster, serialization)("Failed to deserialize ArrayKlass \"%s\": "
                "cannot deserialize class loader " INTPTR_FORMAT ".",
                class_name->as_C_string(), cll.client_cld_address());
    return JBErr::DESER_TERMINATION;
  }
  Handle class_loader(THREAD, cll.class_loader_data()->class_loader());

  Klass* res = SystemDictionary::resolve_or_null(class_name, class_loader, Handle(), THREAD);
  if (res == nullptr || HAS_PENDING_EXCEPTION) {
    log_warning(jbooster, serialization)("Failed to deserialize ArrayKlass \"%s\" (loader={%s}): see stack trace.",
                class_name->as_C_string(), cll.class_loader_data()->loader_name());
    LogTarget(Warning, jbooster, serialization) lt;
    DebugUtils::clear_java_exception_and_print_stack_trace(lt, THREAD);
  } else {
    JClientSessionData* session_data = ((ServerStream*) buf.stream())->session_data();
    session_data->add_klass_address((address) client_klass, (address) res, THREAD);
    arg_ptr = ArrayKlass::cast(res);
  }
  return 0;
}
