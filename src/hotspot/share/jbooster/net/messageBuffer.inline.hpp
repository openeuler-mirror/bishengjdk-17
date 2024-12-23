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

#ifndef SHARE_JBOOSTER_NET_MESSAGEBUFFER_INLINE_HPP
#define SHARE_JBOOSTER_NET_MESSAGEBUFFER_INLINE_HPP

#include "jbooster/net/messageBuffer.hpp"
#include "jbooster/net/serialization.hpp"
#include "logging/log.hpp"

#ifdef ASSERT
inline void SerializationMode::assert_can_serialize() const {
  assert(can_serialize(), "serialization only");
}

inline void SerializationMode::assert_can_deserialize() const {
  assert(can_deserialize(), "deserialization only");
}
#endif

template <typename Arg>
inline uint32_t MessageBuffer::calc_padding(uint32_t offset) {
  static_assert((sizeof(Arg) & (sizeof(Arg) - 1)) == 0, "Should be 1, 2, 4, 8!");
  static_assert(sizeof(Arg) <= 8,                       "Should be 1, 2, 4, 8!");
  return (-offset) & (sizeof(Arg) - 1);
}

template <typename Arg>
inline int MessageBuffer::serialize_base(Arg v) {
  static_assert(std::is_arithmetic<Arg>::value || std::is_enum<Arg>::value, "Base types or enums only!");
  _smode.assert_can_serialize();
  uint32_t arg_size = (uint32_t) sizeof(Arg);
  uint32_t padding = calc_padding<Arg>(_cur_offset);
  uint32_t nxt_offset = _cur_offset + padding + arg_size;
  expand_if_needed(nxt_offset, _cur_offset);
  *((Arg*) (_buf + _cur_offset + padding)) = v;
  _cur_offset = nxt_offset;
  return 0;
}

inline int MessageBuffer::serialize_memcpy(const void* from, uint32_t arg_size) {
  _smode.assert_can_serialize();
  assert(from != nullptr, "sanity");
  uint32_t nxt_offset = _cur_offset + arg_size;
  expand_if_needed(nxt_offset, _cur_offset);
  memcpy((void*) (_buf + _cur_offset), from, arg_size);
  _cur_offset = nxt_offset;
  return 0;
}

template <typename Arg>
inline int MessageBuffer::serialize_no_meta(const Arg& arg) {
  _smode.assert_can_serialize();
  return SerializationImpl<Arg>::serialize(*this, arg);
}

template <typename Arg>
inline int MessageBuffer::serialize_with_meta(const Arg* arg_ptr) {
  static_assert(MessageConst::arg_meta_size == sizeof(uint32_t), "Meta changed?");
  _smode.assert_can_serialize();
  if (arg_ptr == nullptr) {
    return serialize_no_meta(MessageConst::NULL_PTR);
  }
  const Arg& arg = *arg_ptr;
  skip_cur_offset(calc_padding<uint32_t >(_cur_offset));
  uint32_t meta_offset = _cur_offset;
  skip_cur_offset(MessageConst::arg_meta_size);
  expand_if_needed(_cur_offset, _cur_offset);
  JB_RETURN(serialize_no_meta(arg));

  // fill arg meta at last
  uint32_t arg_size = _cur_offset - meta_offset - MessageConst::arg_meta_size;
  *((uint32_t*) (_buf + meta_offset)) = arg_size;
  return 0;
}

template <typename Arg>
inline int MessageBuffer::deserialize_base(Arg& to) {
  static_assert(std::is_arithmetic<Arg>::value || std::is_enum<Arg>::value, "Base types or enums only!");
  _smode.assert_can_deserialize();
  uint32_t arg_size = (uint32_t) sizeof(Arg);
  uint32_t padding = calc_padding<Arg>(_cur_offset);
  uint32_t nxt_offset = _cur_offset + padding + arg_size;
  if (_buf_size < nxt_offset) {
    log_warning(jbooster, rpc)("The size to parse is longer than the msg size: "
                               "arg_size=%u, cur_offset=%u, nxt_offset=%u, buf_size=%u",
                               arg_size, _cur_offset, nxt_offset, _buf_size);
    return JBErr::BAD_MSG_DATA;
  }
  to = *((Arg*) (_buf + _cur_offset + padding));
  _cur_offset = nxt_offset;
  return 0;
}

inline int MessageBuffer::deserialize_memcpy(void* to, uint32_t arg_size) {
  _smode.assert_can_deserialize();
  assert(to != nullptr, "sanity");
  uint32_t nxt_offset = _cur_offset + arg_size;
  if (_buf_size < nxt_offset) {
    log_warning(jbooster, rpc)("The size to parse is longer than the msg size: "
                               "arg_size=%u, cur_offset=%u, nxt_offset=%u, buf_size=%u",
                               arg_size, _cur_offset, nxt_offset, _buf_size);
    return JBErr::BAD_MSG_DATA;
  }
  memcpy(to, (void*) (_buf + _cur_offset), arg_size);
  _cur_offset = nxt_offset;
  return 0;
}

template <typename Arg>
inline int MessageBuffer::deserialize_ref_no_meta(Arg& arg) {
  _smode.assert_can_deserialize();
  return SerializationImpl<Arg>::deserialize_ref(*this, arg);
}

template <typename Arg>
inline int MessageBuffer::deserialize_ptr_no_meta(Arg*& arg_ptr) {
  _smode.assert_can_deserialize();
  assert(arg_ptr == nullptr, "memory will be allocated by this deserializer");
  return SerializationImpl<Arg>::deserialize_ptr(*this, arg_ptr);
}

template <typename Arg>
inline int MessageBuffer::deserialize_with_meta(Arg* const& arg_ptr) {
  _smode.assert_can_deserialize();
  uint32_t arg_size;
  JB_RETURN(deserialize_ref_no_meta(arg_size));
  uint32_t arg_begin = _cur_offset;
  int jb_err;
  if (arg_ptr == nullptr) {
    if (arg_size == MessageConst::NULL_PTR) {
      return 0;
    }
    Arg*& nonconst_ptr = const_cast<Arg*&>(arg_ptr);
    jb_err = deserialize_ptr_no_meta(nonconst_ptr);
    if (jb_err != JBErr::DESER_TERMINATION) JB_RETURN(jb_err);
  } else {
    assert(arg_size != MessageConst::NULL_PTR, "nullptr cannot be assigned to \"Args* const&\"");
    jb_err = deserialize_ref_no_meta(*arg_ptr);
    if (jb_err != JBErr::DESER_TERMINATION) JB_RETURN(jb_err);
  }
  if (_cur_offset - arg_begin != arg_size) {
    if (jb_err == JBErr::DESER_TERMINATION && (_cur_offset - arg_begin < arg_size)) {
      _cur_offset = arg_begin + arg_size;
      return 0;
    }
    const char* type_name = DebugUtils::type_name<Arg>();
    log_warning(jbooster, rpc)("The arg size does not match the parsed size: "
                               "arg=%s, arg_size=%u, (cur_size - arg_begin)=%u.",
                               type_name, arg_size, _cur_offset - arg_begin);
    FREE_C_HEAP_ARRAY(char, type_name);
    return JBErr::BAD_ARG_SIZE;
  }
  return 0;
}

#endif // SHARE_JBOOSTER_NET_MESSAGEBUFFER_INLINE_HPP
