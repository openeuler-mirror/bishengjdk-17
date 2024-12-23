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

#ifndef SHARE_JBOOSTER_NET_COMMUNICATIONSTREAM_INLINE_HPP
#define SHARE_JBOOSTER_NET_COMMUNICATIONSTREAM_INLINE_HPP

#include "jbooster/net/communicationStream.hpp"
#include "jbooster/net/message.inline.hpp"
#include "logging/log.hpp"

inline bool CommunicationStream::check_received_message_type(MessageType expected) {
  if (expected != _msg_recv.msg_type()) {
    log_warning(jbooster, rpc)("Failed to receive the message as wrong message type: "
                               "expected=%s, received=%s. stream_id=%u.",
                               msg_type_name(expected),
                               msg_type_name(_msg_recv.msg_type()),
                               stream_id());
    return false;
  }
  return true;
}

inline bool CommunicationStream::check_received_message_size() {
  if (_msg_recv.msg_size() != _msg_recv.cur_buf_offset()) {
    log_warning(jbooster, rpc)("Failed to parse the message as the msg_size mismatch: "
                               "msg_size=%u, parsed_size=%u. stream_id=%u.",
                               _msg_recv.msg_size(),
                               _msg_recv.cur_buf_offset(),
                               stream_id());
    return false;
  }
  return true;
}

/**
 * Send the mesaage with the message type and all the arguments.
 */
template <typename... Args>
inline int CommunicationStream::send_request(MessageType type, const Args* const... args) {
  _msg_send.set_msg_type(type);
  _msg_send.set_magic_num(MessageConst::RPC_MAGIC);
  _msg_send.set_cur_buf_offset_after_meta();
  JB_RETURN(_msg_send.serialize(args...));
  _msg_send.set_msg_size_based_on_cur_buf_offset();
  return send_message();
}

/**
 * Receive a message.
 * The received message type is set to `type`.
 */
inline int CommunicationStream::recv_request(MessageType& type) {
  int err_code = recv_message();
  type = _msg_recv.msg_type();
  return err_code;
}

/**
 * Receive and parse a message.
 * The received message type must be the same as `type`.
 */
template <typename... Args>
inline int CommunicationStream::recv_request(MessageType type, Args* const&... args) {
  JB_RETURN(recv_message());
  if (!check_received_message_type(type)) {
    return JBErr::BAD_MSG_TYPE;
  }
  return parse_request(args...);
}

/**
 * Parse the received message.
 * Invoke recv_request() before parse_request() to identify the message type.
 */
template <typename... Args>
inline int CommunicationStream::parse_request(Args* const&... args) {
  assert(_msg_recv.cur_buf_offset() == Message::meta_size, "address mismatch");
  JB_RETURN(_msg_recv.deserialize(args...));
  if (!check_received_message_size()) {
    return JBErr::BAD_MSG_DATA;
  }
  return 0;
}

/**
 * Send the response using the message type of the last received message.
 */
template <typename... Args>
inline int CommunicationStream::send_response(const Args* const... args) {
  return send_request(_msg_recv.msg_type(), args...);
}


/**
 * Receive the response using the message type of the last sent message.
 */
template <typename... Args>
inline int CommunicationStream::recv_response(Args* const&... args) {
  JB_RETURN(recv_message());
  if (!check_received_message_type(_msg_send.msg_type())) {
    return JBErr::BAD_MSG_TYPE;
  }
  return parse_request(args...);
}

#endif // SHARE_JBOOSTER_NET_COMMUNICATIONSTREAM_INLINE_HPP
