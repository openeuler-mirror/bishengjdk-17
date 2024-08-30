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

#include "classfile/javaClasses.hpp"
#include "jbooster/jClientVMFlags.hpp"
#include "jbooster/net/messageBuffer.inline.hpp"
#include "jbooster/net/serializationWrappers.hpp"
#include "logging/log.hpp"
#include "utilities/stringUtils.hpp"

template <typename T>
class FlagTypeHandler: public AllStatic {
public:
  static T constructor(T input) { return input; }
  static void destructor(T flag) { /* do nothing */ }

  static bool equals(T a, T b) { return a == b; }
  static uint32_t hash(T flag) { return primitive_hash<T>(flag); }

  static int serialize(MessageBuffer& buf, T flag) {
    return buf.serialize_no_meta(flag);
  }
  static int deserialize(MessageBuffer& buf, T& flag) {
    return buf.deserialize_ref_no_meta(flag);
  }

  static void print(outputStream* st, const char* name, T value) { fatal("please specialize it"); }
};

// ======================= template specialization of ccstr ========================

template <>
ccstr FlagTypeHandler<ccstr>::constructor(ccstr input) {
  return StringUtils::copy_to_heap(input, mtJBooster);
}

template <>
void FlagTypeHandler<ccstr>::destructor(ccstr flag) {
  StringUtils::free_from_heap(flag);
}

template <>
bool FlagTypeHandler<ccstr>::equals(ccstr a, ccstr b) {
  return StringUtils::compare(a, b) == 0;
}

template <>
uint32_t FlagTypeHandler<ccstr>::hash(ccstr flag) {
  return StringUtils::hash_code(flag);
}

template <>
int FlagTypeHandler<ccstr>::serialize(MessageBuffer& buf, ccstr flag) {
  return buf.serialize_with_meta(&flag);
}

template <>
int FlagTypeHandler<ccstr>::deserialize(MessageBuffer& buf, ccstr& flag) {
  StringWrapper sw;
  JB_RETURN(buf.deserialize_with_meta(&sw));
  flag = sw.export_string();
  return 0;
}

template <>
void FlagTypeHandler<ccstr>::print(outputStream* st, const char* name, ccstr value) {
  st->print_cr("    %s: %s", name, value);
}

// ==================== template specialization of other types =====================

template <>
void FlagTypeHandler<bool>::print(outputStream* st, const char* name, bool value) {
  st->print_cr("    %s: %s", name, BOOL_TO_STR(value));
}

template <>
void FlagTypeHandler<intx>::print(outputStream* st, const char* name, intx value) {
  st->print_cr("    %s: " INTX_FORMAT, name, value);
}

template <>
void FlagTypeHandler<size_t>::print(outputStream* st, const char* name, size_t value) {
  st->print_cr("    %s: " SIZE_FORMAT, name, value);
}

// ==================================== macros =====================================

JClientVMFlags::JClientVMFlags(bool is_client) {
  if (is_client) {
#define INIT_FLAG(type, flag) v_##flag = FlagTypeHandler<type>::constructor(flag);
    JCLIENT_VM_FLAGS(INIT_FLAG)
#undef INIT_FLAG

    _state = INITIALIZED_FOR_CLIENT;
  } else {
    _state = NOT_INITIALIZED_FOR_SEREVR;
  }
}

JClientVMFlags::~JClientVMFlags() {
  guarantee(_state != NOT_INITIALIZED_FOR_SEREVR, "sanity");
#define FREE_FLAG(type, flag) FlagTypeHandler<type>::destructor(v_##flag);
  JCLIENT_VM_FLAGS(FREE_FLAG)
#undef FREE_FLAG
}

bool JClientVMFlags::equals(JClientVMFlags* that, bool allow_clr, bool allow_cds, bool allow_aot) {
#define CMP_FLAG(type, flag) if (!FlagTypeHandler<type>::equals(this->v_##flag, that->v_##flag)) return false;
  if (allow_cds) {
    JCLIENT_CDS_VM_FLAGS(CMP_FLAG)
  }
  if (allow_aot) {
    JCLIENT_AOT_VM_FLAGS(CMP_FLAG)
  }
#undef CMP_FLAG

  return true;
}

uint32_t JClientVMFlags::hash(bool allow_clr, bool allow_cds, bool allow_aot) {
  uint32_t result = 1;
#define CALC_FLAG_HASH(type, flag) result = 31 * result + FlagTypeHandler<type>::hash(v_##flag);
  if (allow_cds) {
    JCLIENT_CDS_VM_FLAGS(CALC_FLAG_HASH)
  }
  if (allow_aot) {
    JCLIENT_AOT_VM_FLAGS(CALC_FLAG_HASH)
  }
#undef CALC_FLAG_HASH
  return result;
}

int JClientVMFlags::serialize(MessageBuffer& buf) const {
#define SERIALIZE_FLAG(type, flag) JB_RETURN(FlagTypeHandler<type>::serialize(buf, v_##flag));
  JCLIENT_VM_FLAGS(SERIALIZE_FLAG)
#undef SERIALIZE_FLAG
  return 0;
}

int JClientVMFlags::deserialize(MessageBuffer& buf) {
  guarantee(_state == NOT_INITIALIZED_FOR_SEREVR, "init twice?");

#define DESERIALIZE_FLAG(type, flag) JB_RETURN(FlagTypeHandler<type>::deserialize(buf, v_##flag));
  JCLIENT_VM_FLAGS(DESERIALIZE_FLAG)
#undef DESERIALIZE_FLAG

  _state = INITIALIZED_FOR_SEREVR;
  return 0;
}

void JClientVMFlags::print_flags(outputStream* st) {
  if (!log_is_enabled(Trace, jbooster)) return;
#define PRINT_FLAG(type, flag) FlagTypeHandler<type>::print(st, #flag, v_##flag);
    JCLIENT_VM_FLAGS(PRINT_FLAG)
#undef PRINT_FLAG
}
