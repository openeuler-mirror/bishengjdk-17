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

#ifndef SHARE_JBOOSTER_NET_SERVERLISTENINGTHREAD_HPP
#define SHARE_JBOOSTER_NET_SERVERLISTENINGTHREAD_HPP

#include "runtime/thread.hpp"

class ServerListeningThread : public CHeapObj<mtJBooster> {
private:
  static ServerListeningThread* _singleton;

  JavaThread* const _the_java_thread;
  const char* const _address;
  const uint16_t _port;
  const uint32_t _timeout_ms;

  volatile uint32_t _stream_id_for_alloc;

  volatile bool _exit_flag;

private:
  static void server_listener_thread_entry(JavaThread* thread, TRAPS);

  ServerListeningThread(JavaThread* the_java_thread, const char* address, uint16_t port, uint32_t timeout_ms);

  uint32_t new_stream_id();

  void handle_new_connection(int conn_fd, TRAPS);

  int run_listener(TRAPS);

public:
  static ServerListeningThread* start_thread(const char* address,
                                             uint16_t port,
                                             uint32_t timeout_ms,
                                             TRAPS);

  ~ServerListeningThread();

  bool get_exit_flag() { return _exit_flag; }
  void set_exit_flag() { _exit_flag = true; }

  void handle_connection(int conn_fd);
};

#endif // SHARE_JBOOSTER_NET_SERVERLISTENINGTHREAD_HPP
