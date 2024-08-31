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

#include "jbooster/client/clientDataManager.hpp"
#include "jbooster/net/clientStream.hpp"
#include "jbooster/net/rpcCompatibility.hpp"
#include "jbooster/net/serializationWrappers.hpp"
#include "jbooster/utilities/fileUtils.hpp"
#include "runtime/java.hpp"
#include "runtime/thread.hpp"

ClientStream::ClientStream(const char* address, const char* port, uint32_t timeout_ms):
        CommunicationStream(Thread::current_or_null()),
        _server_address(address),
        _server_port(port),
        _timeout_ms(timeout_ms),
        _inform_before_close(true) {}

ClientStream::ClientStream(const char* address, const char* port, uint32_t timeout_ms, Thread* thread):
        CommunicationStream(thread),
        _server_address(address),
        _server_port(port),
        _timeout_ms(timeout_ms),
        _inform_before_close(true) {}

ClientStream::~ClientStream() {
  if (!can_close_safely() && _inform_before_close) {
    send_no_more_compilation_tasks();
  }
}

int ClientStream::connect_to_server() {
  close_stream();
  const int retries = 3;
  int last_err = 0;
  for (int i = 0; i < retries; ++i) {
    JB_TRY_BREAKABLE {
      int conn_fd;
      JB_THROW(try_to_connect_once(&conn_fd, _server_address, _server_port, _timeout_ms));
      assert(conn_fd >= 0 && errno == 0, "sanity");
      init_stream(conn_fd);
      return 0;
    } JB_TRY_BREAKABLE_END
    JB_CATCH(ECONNREFUSED, EBADF) {
      last_err = JB_ERR;
    } JB_CATCH_REST() {
      last_err = JB_ERR;
      break;
    } JB_CATCH_END;
  }

  if (last_err == ECONNREFUSED || last_err == EBADF) {
    constexpr const char* fmt = "Failed to connect to the JBooster server! Retried %d times.";
    if (JBoosterCrashIfNoServer) {
      fatal(fmt, retries);
    } else {
      log_error(jbooster, rpc)(fmt, retries);
    }
  } else {
    constexpr const char* fmt = "Unexpected exception when connecting to the JBooster server: error=%s(\"%s\").";
    if (JBoosterCrashIfNoServer) {
      fatal(fmt, JBErr::err_name(last_err), JBErr::err_message(last_err));
    } else {
      log_error(jbooster, rpc)(fmt, JBErr::err_name(last_err), JBErr::err_message(last_err));
    }
  }

  return last_err;
}

int ClientStream::request_cache_file(bool* use_it,
                                     bool allowed_to_use,
                                     bool local_cache_exists,
                                     bool remote_cache_exists,
                                     const char* file_path,
                                     MessageType msg_type) {
  if (!allowed_to_use) {
    *use_it = false;
  } else if (local_cache_exists) {
    *use_it = true;
  } else if (remote_cache_exists) {
    FileWrapper file(file_path, SerializationMode::DESERIALIZE);
    if (file.is_tmp_file_already_exists()) {
      *use_it = file.wait_for_file_deserialization();
    } else {
      JB_RETURN(send_request(msg_type));
      JB_RETURN(file.recv_file(this));
      *use_it = !file.is_null();
    }
  } else {
    *use_it = false;
  }
  return 0;
}

int ClientStream::sync_session_meta__client(bool* has_remote_clr, bool* has_remote_cds, bool* has_remote_aot) {
  ClientDataManager& cdm = ClientDataManager::get();

  RpcCompatibility comp;
  uint64_t client_random_id = cdm.random_id();
  JClientArguments* program_args = cdm.program_args();
  JB_RETURN(send_request(MessageType::ClientSessionMeta, &comp, &client_random_id, program_args));

  uint64_t server_random_id;
  uint32_t session_id, program_id;

  JB_TRY {
    JB_THROW(recv_response(stream_id_addr(), &server_random_id, &session_id, &program_id,
                           has_remote_clr, has_remote_cds, has_remote_aot));
  } JB_TRY_END
  JB_CATCH(JBErr::BAD_MSG_TYPE) {
    if (recv_msg_type() == MessageType::UnsupportedClient) {
      char unsupport_reason[64];
      parse_request(&unsupport_reason);
      log_error(jbooster, rpc)("The server does not support this client because the \"%s\" of the two are different!",
                              unsupport_reason);
    }
    return JB_ERR;
  } JB_CATCH_REST() {
    return JB_ERR;
  } JB_CATCH_END;

  cdm.set_server_random_id(server_random_id);
  cdm.set_session_id(session_id);
  cdm.set_program_id(program_id);
  log_info(jbooster, rpc)("Client meta: session_id=%u, program_id=%u, server_random_id=" UINT64_FORMAT_X ".",
                          session_id, program_id, server_random_id);
  return 0;
}

int ClientStream::sync_stream_meta__client() {
  ClientDataManager& cdm = ClientDataManager::get();

  uint32_t session_id = cdm.session_id();
  uint64_t client_random_id = cdm.random_id();
  uint64_t server_random_id = cdm.server_random_id();
  JB_RETURN(send_request(MessageType::ClientStreamMeta, &session_id, &client_random_id, &server_random_id));
  JB_RETURN(recv_response(stream_id_addr()));
  log_trace(jbooster, rpc)("New ClientStream: session_id=%u, stream_id=%u.", session_id, stream_id());
  return 0;
}

int ClientStream::resync_session_and_stream_meta__client() {
  bool has_remote_clr, has_remote_cds, has_remote_aot;  // unused here
  JB_RETURN(sync_session_meta__client(&has_remote_clr, &has_remote_cds, &has_remote_aot));
  return 0;
}

int ClientStream::connect_and_init_session(bool* use_clr, bool* use_cds, bool* use_aot) {
  _inform_before_close = false;
  ClientDataManager& cdm = ClientDataManager::get();

  JB_TRY {
    JB_THROW(connect_to_server());
    bool has_remote_clr, has_remote_cds, has_remote_aot;
    JB_THROW(sync_session_meta__client(&has_remote_clr, &has_remote_cds, &has_remote_aot));

    JB_THROW(request_cache_file(use_clr,
                                cdm.is_clr_allowed(),
                                FileUtils::is_file(cdm.cache_clr_path()),
                                has_remote_clr,
                                cdm.cache_clr_path(),
                                MessageType::GetClassLoaderResourceCache));

    JB_THROW(request_cache_file(use_cds,
                                cdm.is_cds_allowed(),
                                FileUtils::is_file(cdm.cache_cds_path()),
                                has_remote_cds,
                                cdm.cache_cds_path(),
                                MessageType::GetAggressiveCDSCache));

    JB_THROW(request_cache_file(use_aot,
                                cdm.is_aot_allowed(),
                                FileUtils::is_file(cdm.cache_aot_path()),
                                has_remote_aot,
                                cdm.cache_aot_path(),
                                MessageType::GetLazyAOTCache));

    JB_THROW(send_request(MessageType::EndOfCurrentPhase));
  } JB_TRY_END
  JB_CATCH_REST() {
    if (JBoosterExitIfUnsupported && JB_ERR == JBErr::BAD_MSG_TYPE
        && recv_msg_type() == MessageType::UnsupportedClient) {
      vm_exit_during_initialization("The JBooster server does not support this client.");
    }
    LOG_OR_CRASH();

    if (!*use_clr) *use_clr = FileUtils::is_file(cdm.cache_clr_path());
    if (!*use_cds) *use_cds = FileUtils::is_file(cdm.cache_cds_path());
    if (!*use_aot) *use_aot = FileUtils::is_file(cdm.cache_aot_path());

    return JB_ERR;
  } JB_CATCH_END;
  return 0;
}

int ClientStream::connect_and_init_stream() {
  JB_TRY {
    JB_THROW(connect_to_server());
    JB_THROW(sync_stream_meta__client());
  } JB_TRY_END
  JB_CATCH(JBErr::BAD_MSG_TYPE) {
    if (msg_recv().msg_type() == MessageType::ClientSessionMetaAgain) {
      JB_TRY {
        JB_THROW(resync_session_and_stream_meta__client());
        JB_THROW(sync_stream_meta__client());
      } JB_TRY_END
      JB_CATCH_REST() {
        LOG_OR_CRASH();
        return JB_ERR;
      } JB_CATCH_END;
      return 0;
    }
    LOG_OR_CRASH();
    return JB_ERR;
  } JB_CATCH_REST() {
    LOG_OR_CRASH();
    return JB_ERR;
  } JB_CATCH_END;
  return 0;
}

void ClientStream::send_no_more_compilation_tasks() {
  if (is_stream_closed()) return;
  JB_TRY {
    bool no_more = true;
    JB_THROW(send_request(MessageType::NoMoreRequests, &no_more));
    set_can_close_safely();
  } JB_TRY_END
  JB_CATCH_REST() {
    log_warning(jbooster, rpc)("Exception [%s] at ClientStream::send_no_more_compilation_tasks(). stream_id=%u.",
                               JBErr::err_name(JB_ERR), stream_id());
  } JB_CATCH_END;
}

void ClientStream::log_or_crash(const char* file, int line, int err_code) {
  constexpr const char* fmt = "Unhandled exception found at %s:%d: error=%s(\"%s\"), stream_id=%u.";
  if (JBoosterCrashIfNoServer) {
    fatal(fmt, file, line,
          JBErr::err_name(err_code), JBErr::err_message(err_code),
          stream_id());
  } else {
    log_error(jbooster, rpc)(fmt, file, line,
                             JBErr::err_name(err_code), JBErr::err_message(err_code),
                             stream_id());
  }
}
