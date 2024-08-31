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

#ifndef SHARE_JBOOSTER_NET_MESSAGE_INLINE_HPP
#define SHARE_JBOOSTER_NET_MESSAGE_INLINE_HPP

#include <string.h>

#include "jbooster/net/message.hpp"
#include "jbooster/net/messageBuffer.inline.hpp"
#include "jbooster/net/serialization.hpp"

inline void Message::set_overflow(uint32_t offset, uint32_t size) {
  guarantee(!has_overflow(), "handle the existing overflow first");
  assert(size > 0 && offset + size <= _buf.buf_size(), "sanity");
  _overflow_offset = offset;
  _overflow_size = size;
}

inline uint32_t Message::move_overflow() {
  assert(has_overflow(), "sanity");
  uint32_t size = _overflow_size;
  memmove(_buf.buf(), _buf.buf() + _overflow_offset, _overflow_size);
  _overflow_offset = _overflow_size = 0;
  return size;
}

inline int Message::serialize_inner() {
  return 0;
}

inline int Message::deserialize_inner() {
  return 0;
}

template <typename Arg, typename... Args>
inline int Message::serialize_inner(const Arg* const arg, const Args* const... args) {
  JB_RETURN(_buf.serialize_with_meta(arg));
  return serialize_inner(args...);
}

template <typename Arg, typename... Args>
inline int Message::deserialize_inner(Arg* const& arg, Args* const&... args) {
  JB_RETURN(_buf.deserialize_with_meta(arg));
  return deserialize_inner(args...);
}

template <typename... Args>
inline int Message::serialize(const Args* const... args) {
  return serialize_inner(args...);
}

template <typename... Args>
inline int Message::deserialize(Args* const&... args) {
  return deserialize_inner(args...);
}

inline void Message::serialize_meta() {
  _buf.set_cur_offset(0);
  _buf.serialize_no_meta(_meta.msg_size);
  _buf.serialize_no_meta(_meta.msg_type);
  assert(cur_buf_offset() == meta_size, "sanity");
}

inline void Message::deserialize_meta() {
  _buf.set_cur_offset(0);
  _buf.deserialize_ref_no_meta(_meta.msg_size);
  _buf.deserialize_ref_no_meta(_meta.msg_type);
  assert(cur_buf_offset() == meta_size, "sanity");
}

#endif // SHARE_JBOOSTER_NET_MESSAGE_INLINE_HPP
