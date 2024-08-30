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

#ifndef SHARE_JBOOSTER_NET_MESSAGEBUFFER_HPP
#define SHARE_JBOOSTER_NET_MESSAGEBUFFER_HPP

#include "jbooster/net/netCommon.hpp"
#include "memory/allocation.hpp"

class CommunicationStream;

class SerializationMode {
public:
  enum Value: uint8_t {
    BOTH,
    SERIALIZE,
    DESERIALIZE
  };

private:
  Value _mode;

public:
  SerializationMode(Value mode): _mode(mode) {}

  void set(Value mode) { _mode = mode; }

  bool operator == (const SerializationMode other) const {
    return _mode == other._mode;
  }

  bool can_serialize() const { return _mode != DESERIALIZE; }
  bool can_deserialize() const { return _mode != SERIALIZE; }

  void assert_can_serialize() const NOT_DEBUG_RETURN;
  void assert_can_deserialize() const NOT_DEBUG_RETURN;
};

class MessageBuffer final: public StackObj {
  friend class Message;

private:
  const uint32_t _default_buf_size = 4 * 1024; // 4k

  SerializationMode _smode;
  uint32_t _buf_size;
  char* _buf;
  uint32_t _cur_offset;
  CommunicationStream* const _stream;

private:
  static uint32_t calc_new_buf_size(uint32_t required_size);
  void expand_buf(uint32_t required_size, uint32_t copy_size);

public:
  MessageBuffer(SerializationMode smode, CommunicationStream* stream = nullptr);
  ~MessageBuffer();

  char* buf() const { return _buf; }
  uint32_t buf_size() const { return _buf_size; }

  uint32_t cur_offset() const { return _cur_offset; }
  void set_cur_offset(uint32_t offset) { _cur_offset = offset; }
  void skip_cur_offset(uint32_t offset) { _cur_offset += offset; }
  void reset_cur_offset() { _cur_offset = 0u; }

  char* cur_buf_ptr() const { return _buf + _cur_offset; }

  CommunicationStream* stream() const { return _stream; }

  void expand_if_needed(uint32_t required_size, uint32_t copy_size) {
    if (_buf_size < required_size) {
      expand_buf(required_size, copy_size);
    }
  }

  // serializers
  int serialize_memcpy(const void* from, uint32_t arg_size);
  template <typename Arg>
  int serialize_no_meta(const Arg& arg);
  template <typename Arg>
  int serialize_with_meta(const Arg* arg_ptr);

  // deserializers
  int deserialize_memcpy(void* to, uint32_t arg_size);
  template <typename Arg>
  int deserialize_ref_no_meta(Arg& arg);
  template <typename Arg>
  int deserialize_ptr_no_meta(Arg*& arg_ptr);
  template <typename Arg>
  int deserialize_with_meta(Arg* const& arg_ptr);
};

#endif // SHARE_JBOOSTER_NET_MESSAGEBUFFER_HPP
