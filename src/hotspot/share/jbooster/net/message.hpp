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

#ifndef SHARE_JBOOSTER_NET_MESSAGE_HPP
#define SHARE_JBOOSTER_NET_MESSAGE_HPP

#include "jbooster/net/messageBuffer.hpp"
#include "jbooster/net/messageType.hpp"
#include "jbooster/net/netCommon.hpp"

class Message: public MessageConst {
  // see MessageConst for the message format
private:
  struct {
    uint32_t msg_size;
    uint16_t magic_num;
    MessageType msg_type;
  } _meta;

  MessageBuffer _buf;

  uint32_t _overflow_offset;
  uint32_t _overflow_size;

private:
  int serialize_inner();
  int deserialize_inner();
  template <typename Arg, typename... Args>
  int serialize_inner(const Arg* const arg, const Args* const... args);
  template <typename Arg, typename... Args>
  int deserialize_inner(Arg* const& arg, Args* const&... args);

public:
  Message(SerializationMode smode, CommunicationStream* stream = nullptr):
      _buf(smode, stream),
      _overflow_offset(0),
      _overflow_size(0) {}

  uint32_t msg_size() const { return _meta.msg_size; }
  void set_msg_size(uint32_t size) { _meta.msg_size = size; }
  void set_msg_size_based_on_cur_buf_offset() { set_msg_size(cur_buf_offset()); }
  void set_magic_num(uint16_t magic_num) { _meta.magic_num = magic_num; }
  MessageType msg_type() const { return _meta.msg_type; }
  void set_msg_type(MessageType type) { _meta.msg_type = type; }

  char* buf_beginning() const { return _buf.buf(); }
  uint32_t buf_size() const { return _buf.buf_size(); }

  uint32_t cur_buf_offset() { return _buf.cur_offset(); }
  void set_cur_buf_offset_after_meta() { _buf.set_cur_offset(meta_size); }

  bool has_overflow() { return _overflow_size > 0; }
  void set_overflow(uint32_t offset, uint32_t size);
  uint32_t move_overflow();

  void expand_buf_if_needed(uint32_t required_size, uint32_t copy_size) {
    _buf.expand_if_needed(required_size, copy_size);
  }

  uint32_t deserialize_size_only() { return *((uint32_t*)_buf.buf()); }
  uint16_t deserialize_magic_num_only() { return *((uint16_t*)(_buf.buf() + sizeof(_meta.msg_size))); }

  bool check_magic_num(uint16_t magic_num) { return magic_num == MessageConst::RPC_MAGIC; }

  template <typename... Args>
  int serialize(const Args* const... args);
  template <typename... Args>
  int deserialize(Args* const&... args);

  void serialize_meta();
  void deserialize_meta();
};

#endif // SHARE_JBOOSTER_NET_MESSAGE_HPP
