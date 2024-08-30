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

#include "jbooster/net/communicationStream.inline.hpp"
#include "runtime/os.inline.hpp"
#ifdef ASSERT
#include "runtime/thread.inline.hpp"
#endif // ASSERT

void CommunicationStream::handle_net_err(int comm_size, bool is_recv) {
  if (comm_size > 0) return;
  const char* rw = is_recv ? "recv" : "send";
  if (comm_size == 0) {
    set_errno(JBErr::CONN_CLOSED_BY_PEER);
    log_debug(jbooster, rpc)("Failed to %s as the connection is closed by peer. stream_id=%u.",
                             rw, stream_id());
    close_stream();
    return;
  }
  // comm_size < 0
  if (is_stream_closed()) {
    set_errno(JBErr::CONN_CLOSED);
    log_debug(jbooster, rpc)("Failed to %s as the connection has been closed. stream_id=%u.",
                             rw, stream_id());
    return;
  }
  int err = errno;
  errno = 0;
  set_errno(err);
  log_debug(jbooster, rpc)("Failed to %s: error=%s(\"%s\"), stream_id=%u.",
                           rw, JBErr::err_name(err), JBErr::err_message(err), stream_id());
  // We don't give special treatment to EAGAIN (yet).
  close_stream();
  return;
}

uint32_t CommunicationStream::read_once_from_stream(char* buf, uint32_t size) {
    int read_size = os::recv(_conn_fd, buf, (size_t) size, 0);
    if (read_size <= 0) {
      handle_net_err(read_size, true);
      return 0;
    }
    return (uint32_t) read_size;
}

uint32_t CommunicationStream::read_all_from_stream(char* buf, uint32_t size) {
  uint32_t total_read_size = 0;
  while (total_read_size < size) {
    int read_size = os::recv(_conn_fd, buf + total_read_size, (size_t) (size - total_read_size), 0);
    if (read_size <= 0) {
      handle_net_err(read_size, true);
      break;
    }
    total_read_size += read_size;
  }
  return total_read_size;
}

uint32_t CommunicationStream::write_once_to_stream(char* buf, uint32_t size) {
  int written_size = os::send(_conn_fd, buf, size, 0);
  if (written_size <= 0) {
    handle_net_err(written_size, false);
    return 0;
  }
  return (uint32_t) written_size;
}

uint32_t CommunicationStream::write_all_to_stream(char* buf, uint32_t size) {
  uint32_t total_written_size = 0;
  while (total_written_size < size) {
    int written_size = os::send(_conn_fd, buf + total_written_size, (size_t) (size - total_written_size), 0);
    if (written_size <= 0) {
      handle_net_err(written_size, false);
      break;
    }
    total_written_size += written_size;
  }
  return total_written_size;
}

void CommunicationStream::close_stream() {
  if (_conn_fd >= 0) {
    log_trace(jbooster, rpc)("Connection closed. stream_id=%u.", stream_id());
    os::close(_conn_fd);
    _conn_fd = -1;
  }
}

#ifdef ASSERT
void CommunicationStream::assert_current_thread() {
  assert(Thread::current_or_null() == _cur_thread, "cur_thread=%p, stream_thread=%p",
         Thread::current_or_null(), _cur_thread);
}

void CommunicationStream::assert_in_native() {
  if (_cur_thread != nullptr && _cur_thread->is_Java_thread()) {
    assert(_cur_thread->as_Java_thread()->thread_state() == _thread_in_native, "may affect safepoint");
  }
}
#endif // ASSERT

int CommunicationStream::recv_message() {
  assert_current_thread();
  assert_in_native();

  Message& msg = _msg_recv;
  // read once (or memmove from the overflowed buffer) to get message size
  uint32_t read_size, msg_size;
  if (msg.has_overflow()) {
    read_size = msg.move_overflow();
    if (read_size < sizeof(msg_size)) {
      read_size += read_once_from_stream(msg.buf_beginning() + read_size, msg.buf_size() - read_size);
    }
  } else {
    read_size = read_once_from_stream(msg.buf_beginning(), msg.buf_size());
  }

  if (read_size < sizeof(msg_size)) {
    if (!is_stream_closed()) {
      log_warning(jbooster, rpc)("Failed to read the size of the message (read_size=%d). stream_id=%u.",
                                 read_size, stream_id());
    }
    return return_errno_or_flag(JBErr::BAD_MSG_SIZE);
  }

  msg_size = msg.deserialize_size_only();
  if (read_size > msg_size) {         // read too much
    msg.set_overflow(msg_size, read_size - msg_size);
  } else if (read_size < msg_size) {  // read the rest then
    uint32_t msg_left_size = msg_size - read_size;
    msg.expand_buf_if_needed(msg_size, read_size);
    uint32_t sz = read_all_from_stream(msg.buf_beginning() + read_size, msg_left_size);
    if (sz < msg_left_size) {
      log_warning(jbooster, rpc)("Failed to read the rest message: read_size=%d, msg_left_size=%d. stream_id=%u.",
                                 sz, msg_left_size, stream_id());
      return return_errno_or_flag(JBErr::BAD_MSG_SIZE);
    }
  }

  msg.deserialize_meta();
  log_debug(jbooster, rpc)("Recv %s, size=%u, thread=%p, stream_id=%u.",
                           msg_type_name(msg.msg_type()), msg.msg_size(),
                           Thread::current_or_null(), stream_id());
  return 0;
}

int CommunicationStream::send_message() {
  assert_current_thread();
  assert_in_native();

  Message& msg = _msg_send;
  msg.serialize_meta();
  uint32_t sz = write_all_to_stream(msg.buf_beginning(), msg.msg_size());
  if (sz != msg.msg_size()) {
    log_warning(jbooster, rpc)("Failed to send the full message: write_size=%d, msg_size=%d. stream_id=%u.",
                               sz, msg.msg_size(), stream_id());
    return return_errno_or_flag(JBErr::BAD_MSG_SIZE);
  }
  log_debug(jbooster, rpc)("Send %s, size=%u, thread=%p, stream_id=%u.",
                           msg_type_name(msg.msg_type()), msg.msg_size(),
                           Thread::current_or_null(), stream_id());
  return 0;
}
