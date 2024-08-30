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

#ifndef SHARE_JBOOSTER_SERVER_SERVERCONTROLTHREAD_HPP
#define SHARE_JBOOSTER_SERVER_SERVERCONTROLTHREAD_HPP

#include "jbooster/utilities/concurrentHashMap.hpp"
#include "runtime/mutex.hpp"
#include "runtime/thread.hpp"

class ServerStream;

/**
 * Goals of this thread:
 * - Sync heartbeat with the clients;
 * - Manage the client sessions and client programs (clear out-of-date data);
 */
class ServerControlThread : public CHeapObj<mtJBooster> {
public:
  static uint32_t _unused_shared_data_cleanup_timeout;
  static uint32_t _heartbeat_read_write_timeout;
  static uint32_t _session_no_ref_timeout;

  class ClientSessionState: public StackObj {
    ServerStream* _server_stream;
    int _heartbeat_failed_times;

  public:
    ClientSessionState(ServerStream* server_stream);

    ServerStream* server_stream() { return _server_stream; }

    int heartbeat_failed_times() { return _heartbeat_failed_times; }
    int inc_heartbeat_failed_times() { return ++_heartbeat_failed_times; }
    int reset_heartbeat_failed_times() { return _heartbeat_failed_times = 0; }
  };

  // key: session id
  using SessionStateMap = ConcurrentHashMap<uint32_t, ClientSessionState, mtJBooster>;

private:
  static ServerControlThread* _singleton;

  JavaThread* const _the_java_thread;
  SessionStateMap _client_daemons;
  Monitor _sleep_lock;

private:
  static void server_control_thread_entry(JavaThread* thread, TRAPS);

  ServerControlThread(JavaThread* java_thread);

  int send_heartbeat(ServerStream* server_stream);
  void try_remove_session_data(uint32_t session_id, ClientSessionState* state, TRAPS);
  void update_session_state(uint32_t session_id, ClientSessionState* state, TRAPS);
  void update_session_states(TRAPS);
  void clear_unused_sessions(jlong current_time, TRAPS);
  void clear_unused_programs(jlong current_time, TRAPS);
  void control_loop(TRAPS);

public:
  static ServerControlThread* start_thread(TRAPS);

  static uint32_t unused_shared_data_cleanup_timeout() { return _unused_shared_data_cleanup_timeout; }
  static void set_unused_shared_data_cleanup_timeout(uint32_t timeout) {
    _unused_shared_data_cleanup_timeout = timeout;
  }

  static uint32_t heartbeat_read_write_timeout() { return _heartbeat_read_write_timeout; }
  static void set_heartbeat_read_write_timeout(uint32_t timeout) {
    _heartbeat_read_write_timeout = timeout;
  }

  static uint32_t session_no_ref_timeout() { return _session_no_ref_timeout; }
  static void set_session_no_ref_timeout(uint32_t timeout) {
    _session_no_ref_timeout = timeout;
  }

  void add_client_daemon_connection(ServerStream* server_stream, TRAPS);
};

#endif // SHARE_JBOOSTER_SERVER_SERVERCONTROLTHREAD_HPP
