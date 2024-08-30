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

#include "classfile/javaClasses.inline.hpp"
#include "classfile/vmClasses.hpp"
#include "classfile/vmSymbols.hpp"
#include "jbooster/jBoosterManager.hpp"
#include "jbooster/net/serverStream.hpp"
#include "jbooster/server/serverControlThread.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/server/serverMessageHandler.hpp"
#include "jbooster/utilities/concurrentHashMap.inline.hpp"
#include "jbooster/utilities/debugUtils.inline.hpp"
#include "logging/log.hpp"
#include "runtime/globals.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/os.hpp"
#include "runtime/thread.inline.hpp"

uint32_t ServerControlThread::_unused_shared_data_cleanup_timeout = -1; // set in java code
uint32_t ServerControlThread::_heartbeat_read_write_timeout = 4 * 1000;
uint32_t ServerControlThread::_session_no_ref_timeout = 16 * 1000;

ServerControlThread* ServerControlThread::_singleton = nullptr;

ServerControlThread::ClientSessionState::ClientSessionState(ServerStream* server_stream):
        _server_stream(server_stream),
        _heartbeat_failed_times(0) {}

ServerControlThread* ServerControlThread::start_thread(TRAPS) {
  JavaThread* new_thread = new JavaThread(&server_control_thread_entry);
  guarantee(new_thread != nullptr && new_thread->osthread() != nullptr, "sanity");
  guarantee(_singleton == nullptr, "sanity");
  _singleton = new ServerControlThread(new_thread);

  Handle string = java_lang_String::create_from_str("JBooster Serevr Control", CHECK_NULL);
  Handle thread_group(THREAD, Universe::main_thread_group());
  Handle thread_oop = JavaCalls::construct_new_instance(
                                 vmClasses::Thread_klass(),
                                 vmSymbols::threadgroup_string_void_signature(),
                                 thread_group,
                                 string,
                                 CHECK_NULL);

  Klass* group = vmClasses::ThreadGroup_klass();
  JavaValue result(T_VOID);
  JavaCalls::call_special(&result,
                        thread_group,
                        group,
                        vmSymbols::add_method_name(),
                        vmSymbols::thread_void_signature(),
                        thread_oop,
                        THREAD);

  JavaThread::start_internal_daemon(THREAD, new_thread, thread_oop, NormPriority);
  return _singleton;
}

void ServerControlThread::server_control_thread_entry(JavaThread* thread, TRAPS) {
  _singleton->control_loop(thread);
}

ServerControlThread::ServerControlThread(JavaThread* java_thread):
        _the_java_thread(java_thread),
        _client_daemons(),
        _sleep_lock(Mutex::leaf, "JBooster Server Control", true, Mutex::_safepoint_check_never) {
}

/**
 * This method is invoked in other thread.
 * So pay attention to the difference between THREAD and this.
 */
void ServerControlThread::add_client_daemon_connection(ServerStream* server_stream, TRAPS) {
  uint32_t session_id = server_stream->session_id();
  server_stream->set_current_thread(_the_java_thread);
  server_stream->set_read_write_timeout(_heartbeat_read_write_timeout);
  ClientSessionState state(server_stream);
  ClientSessionState* in_map = _client_daemons.put_if_absent(session_id, state, THREAD);
  if (in_map->server_stream() != server_stream) {
    server_stream->set_current_thread(THREAD);
    log_error(jbooster)("Duplicate client daemon connection, ignored: session_id=%u, stream_id=%u.",
                        session_id, server_stream->stream_id());
    return;
  }
}

/**
 * Send and receive the heartbeat.
 */
int ServerControlThread::send_heartbeat(ServerStream* server_stream) {
  int magic = 0xa1b2c3;
  JB_RETURN(server_stream->send_request(MessageType::Heartbeat, &magic));
  int num;
  JB_RETURN(server_stream->recv_response(&num));
  if (num != magic) JB_RETURN(JBErr::BAD_ARG_DATA);
  return 0;
}

/**
 * Try to delete a session data that meets:
 * - It has a daemon stream and the stream has been closed;
 * - It has no other stream.
 */
void ServerControlThread::try_remove_session_data(uint32_t session_id, ClientSessionState* state, TRAPS) {
  ServerStream* server_stream = state->server_stream();
  JClientSessionData* sd = server_stream->session_data();
  assert(sd->session_id() == session_id, "sanity");

  // the last server stream should be this one
  if (sd->ref_cnt().get() > 1) {
    return;
  }
  assert(sd->ref_cnt().get() == 1, "sanity");

  guarantee(server_stream->current_thread() == THREAD, "sanity");

  log_info(jbooster)("The client has exited, so its session data is cleared: session_id=%u, stream_id=%u.",
                     session_id, server_stream->stream_id());

  _client_daemons.remove(session_id, THREAD);
  state = nullptr;  // deleted in _client_daemons
  delete server_stream;
  assert(sd->ref_cnt().get() == 0, "sanity");
  ServerDataManager::get().try_remove_session(session_id, THREAD);
}

/**
 * Update the heart beat of the session data.
 * Delete the session data if the client has exited (or lost contact).
 */
void ServerControlThread::update_session_state(uint32_t session_id, ClientSessionState* state, TRAPS) {
  ServerStream* server_stream = state->server_stream();

  if (server_stream->is_stream_closed()) {
    try_remove_session_data(session_id, state, THREAD);
    return;
  }

  JB_TRY {
    JB_THROW(send_heartbeat(server_stream));
  } JB_TRY_END
  JB_CATCH(JBErr::CONN_CLOSED_BY_PEER) {
    try_remove_session_data(session_id, state, THREAD);
    return;
  } JB_CATCH_REST() {
    int times = state->inc_heartbeat_failed_times();
    log_warning(jbooster)("Heartbeat sync failed: error=%s(\"%s\"), "
                          "failed_times=%d, session_id=%u, stream_id=%u.",
                          JBErr::err_name(JB_ERR), JBErr::err_message(JB_ERR),
                          state->heartbeat_failed_times(),
                          session_id, server_stream->stream_id());
    if (times >= 4) {
      try_remove_session_data(session_id, state, THREAD);
    }
    return;
  } JB_CATCH_END;

  state->reset_heartbeat_failed_times();
}

/**
 * Update the states of all sessions by heart beat.
 */
void ServerControlThread::update_session_states(TRAPS) {
  GrowableArray<ServerControlThread::ClientSessionState*> sessions_with_daemon;
  auto scan_func = [&sessions_with_daemon] (ServerControlThread::SessionStateMap::KVNode* p) -> bool {
    assert(p->key() == p->value().server_stream()->session_id(), "sanity");
    sessions_with_daemon.append(&p->value());
    return true;
  };
  _client_daemons.for_each(scan_func, THREAD);
  for (GrowableArrayIterator<ClientSessionState*> iter = sessions_with_daemon.begin();
                                                  iter != sessions_with_daemon.end();
                                                  ++iter) {
    // Don't do the following logic in the scan_func above, because that's in the
    // critical section, where the map is locked. We should avoid locking the map
    // for too long.
    update_session_state((*iter)->server_stream()->session_id(), *iter, THREAD);
  }
}

/**
 * Delete session datas with closed daemon stream.
 * Most unused sessions are cleared in update_session_state(). This function is
 * mainly used to clear some special cases.
 * So it doesn't have to be invoked very often.
 */
void ServerControlThread::clear_unused_sessions(jlong current_time, TRAPS) {
  auto eval_func = [current_time] (ServerDataManager::JClientSessionDataMap::KVNode* p) -> bool {
    JClientSessionData* sd = p->value();
    if (sd->ref_cnt().no_ref_time(current_time) < (jlong) session_no_ref_timeout()) {
      return false;
    }
    return sd->ref_cnt().get() == 0;
  };

  auto del_func = [&keys = _client_daemons, THREAD] (ServerDataManager::JClientSessionDataMap::KVNode* p) {
    uint32_t session_id = p->key();
    // collect sessions without daemon stream
    guarantee(!keys.contains(session_id, THREAD), "ref cnt mismatch");

    log_info(jbooster)("A session data is deleted due to no daemon stream: "
                        "session_id=%u.", session_id);
    // The real deletion of session data will be done in JClientSessionDataMapEvents::on_del().
  };

  ServerDataManager& sdm = ServerDataManager::get();
  sdm.sessions()->bulk_remove_if(eval_func, del_func, THREAD);
}

/**
 * Delete program datas with no sessions for a long time.
 */
void ServerControlThread::clear_unused_programs(jlong current_time, TRAPS) {
  ResourceMark rm;
  GrowableArray<uint32_t> deleted_program_ids;

  auto eval_func = [current_time] (ServerDataManager::JClientProgramDataMap::KVNode* p) -> bool {
    JClientProgramData* pd = p->value();
    if (pd->ref_cnt().no_ref_time(current_time) < (jlong) unused_shared_data_cleanup_timeout()) {
      return false;
    }
    return pd->ref_cnt().get() == 0;
  };

  auto del_func = [&deleted_program_ids] (ServerDataManager::JClientProgramDataMap::KVNode* p) {
    deleted_program_ids.append(p->value()->program_id());
    log_info(jbooster)("A program data is deleted due to no sessions for a long time: "
                       "program_id=%u.", p->value()->program_id());
    // The real deletion of program data will be done in JClientProgramDataMapEvents::on_del().
  };

  ServerDataManager& sdm = ServerDataManager::get();
  sdm.programs()->bulk_remove_if(eval_func, del_func, THREAD);

  // Call the java methods after the critical region of bulk_remove_if().
  for (GrowableArrayIterator<uint32_t> iter = deleted_program_ids.begin();
                                       iter != deleted_program_ids.end(); ++iter) {
    ServerDataManager::get().remove_java_side_program_data(*iter, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      LogTarget(Error, jbooster) lt;
      DebugUtils::clear_java_exception_and_print_stack_trace(lt, THREAD);
    }
  }
}

/**
 * Main loop of ServerControlThread.
 */
void ServerControlThread::control_loop(TRAPS) {
  ThreadToNativeFromVM ttn(THREAD);
  jlong program_last_check_time = os::javaTimeMillis();

  while (true) {
    { MonitorLocker locker(&_sleep_lock, Mutex::_no_safepoint_check_flag);
      locker.wait(JBoosterManager::heartbeat_timeout() / 4);
    }
    jlong current_time = os::javaTimeMillis();
    ResourceMark rm(THREAD);

    update_session_states(THREAD);

    if (current_time - program_last_check_time > (jlong) (unused_shared_data_cleanup_timeout() >> 1)) {
      program_last_check_time = current_time;

      clear_unused_sessions(current_time, THREAD);
      clear_unused_programs(current_time, THREAD);
    }
  }
}
