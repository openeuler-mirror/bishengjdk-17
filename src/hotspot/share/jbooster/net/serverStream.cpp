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

#include "compiler/compileTask.hpp"
#include "jbooster/net/rpcCompatibility.hpp"
#include "jbooster/net/serializationWrappers.hpp"
#include "jbooster/net/serverStream.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/utilities/fileUtils.hpp"
#include "runtime/thread.hpp"
#include "runtime/timerTrace.hpp"

ServerStream::ServerStream(int conn_fd):
                           CommunicationStream(Thread::current_or_null()),
                           _session_data(nullptr) {
  init_stream(conn_fd);
}

ServerStream::ServerStream(int conn_fd, Thread* thread):
                           CommunicationStream(thread),
                           _session_data(nullptr) {
  init_stream(conn_fd);
}

ServerStream::~ServerStream() {
  set_session_data(nullptr);
}

uint32_t ServerStream::session_id() {
  return session_data()->session_id();
}

void ServerStream::set_session_data(JClientSessionData* sd) {
  JClientSessionData* old_sd = _session_data;
  if (sd == old_sd) return;
  // Do not call sd->ref_cnt().inc() here as it has been inc when obtained.
  if (old_sd != nullptr) {
    old_sd->ref_cnt().dec_and_update_time();
  }
  _session_data = sd;
}

void ServerStream::handle_meta_request(uint32_t stream_id) {
  set_stream_id(stream_id);
  MessageType type;
  JB_TRY {
    JB_THROW(recv_request(type));
    switch (type) {
      case MessageType::ClientSessionMeta: {
        JB_THROW(sync_session_meta__server());
        close_stream();
        break;
      }
      case MessageType::ClientStreamMeta: {
        JB_THROW(sync_stream_meta__server());
        break;
      }
      default: {
        JB_THROW(JBErr::BAD_MSG_TYPE);
        break;
      }
      // Recheck JB_ERR because the JB_THROWs above are intercepted
      // by the switch statement.
      JB_THROW(JB_ERR);
    }
  } JB_TRY_END
  JB_CATCH(JBErr::INCOMPATIBLE_RPC) {
    const char* unsupport_reason = "RPC version";
    log_warning(jbooster)("The server does not support this client because the \"%s\" of the two are different! stream_id=%u.",
                          unsupport_reason, stream_id);
    send_request(MessageType::UnsupportedClient, &unsupport_reason);
    close_stream();
  } JB_CATCH_REST() {
    log_warning(jbooster, rpc)("Unhandled exception at ServerStream::handle_meta_request(): "
                               "error=%s(\"%s\"), first_msg_type=%s, stream_id=%u.",
                               JBErr::err_name(JB_ERR), JBErr::err_message(JB_ERR),
                               msg_type_name(type), stream_id);
    close_stream();
  } JB_CATCH_END;
}

int ServerStream::sync_session_meta__server() {
  ServerDataManager& sdm = ServerDataManager::get();

  RpcCompatibility comp;
  uint64_t client_random_id;
  JClientArguments program_args(false);    // on-stack allocation to prevent memory leakage
  JB_RETURN(parse_request(&comp, &client_random_id, &program_args));

  const char* unsupport_reason = nullptr;
  if (!program_args.check_compatibility_with_server(&unsupport_reason)) {
    log_warning(jbooster)("The server does not support this client because the \"%s\" of the two are different! stream_id=%u.",
                          unsupport_reason, stream_id());
    JB_RETURN(send_request(MessageType::UnsupportedClient, &unsupport_reason));
    JB_RETURN(JBErr::ABORT_CUR_PHRASE);
  }

  JClientSessionData* sd = sdm.create_session(client_random_id, &program_args, Thread::current());
  JClientProgramData* pd = sd->program_data();
  set_session_data(sd);

  uint64_t server_random_id = sdm.random_id();
  uint32_t session_id = sd->session_id();
  uint32_t program_id = pd->program_id();
  bool has_remote_clr = pd->clr_cache_state().is_cached();
  bool has_remote_cds = pd->cds_cache_state().is_cached();
  bool has_remote_aot = pd->aot_cache_state().is_cached();
  JB_RETURN(send_response(stream_id_addr(), &server_random_id, &session_id, &program_id,
                          &has_remote_clr, &has_remote_cds, &has_remote_aot));
  log_info(jbooster, rpc)("New client: session_id=%u, program_id=%u, "
                          "random_id=" UINT64_FORMAT_X ", program_name=%s, "
                          "has_clr=%s, has_cds=%s, has_aot=%s, "
                          "stream_id=%u.",
                          session_id, program_id,
                          client_random_id, program_args.program_name(),
                          BOOL_TO_STR(has_remote_clr),
                          BOOL_TO_STR(has_remote_cds),
                          BOOL_TO_STR(has_remote_aot),
                          stream_id());

  return handle_sync_requests(pd);
}

int ServerStream::handle_sync_requests(JClientProgramData* pd) {
  bool not_end = true;
  do {
    MessageType type;
    JB_RETURN(recv_request(type));
    switch (type) {
    case MessageType::GetClassLoaderResourceCache: {
      TraceTime tt("Send clr", TRACETIME_LOG(Info, jbooster));
      FileWrapper file(pd->clr_cache_state().file_path(), SerializationMode::SERIALIZE);
      JB_RETURN(file.send_file(this));
      break;
    }
    case MessageType::GetAggressiveCDSCache: {
      TraceTime tt("Send cds", TRACETIME_LOG(Info, jbooster));
      FileWrapper file(pd->cds_cache_state().file_path(), SerializationMode::SERIALIZE);
      JB_RETURN(file.send_file(this));
      break;
    }
    case MessageType::GetLazyAOTCache: {
      TraceTime tt("Send aot", TRACETIME_LOG(Info, jbooster));
      FileWrapper file(pd->aot_cache_state().file_path(), SerializationMode::SERIALIZE);
      JB_RETURN(file.send_file(this));
      break;
    }
    default: not_end = false; break;
    }
  } while (not_end);
  return 0;
}

int ServerStream::sync_stream_meta__server() {
  ServerDataManager& sdm = ServerDataManager::get();

  uint32_t session_id;
  uint64_t client_random_id, server_random_id;
  JB_RETURN(parse_request(&session_id, &client_random_id, &server_random_id));
  set_session_data(sdm.get_session(session_id, Thread::current()));

  if (_session_data == nullptr
      || client_random_id != _session_data->random_id()
      || server_random_id != sdm.random_id()) {
    // The server has been restarted, and this client was connected to the previous server instance.
    log_warning(jbooster, rpc)("Unrecognized session! Sync again. "
                               "obtained_session_id=%u, "
                               "obtained_client_random_id=" UINT64_FORMAT_X ", "
                               "obtained_server_random_id=" UINT64_FORMAT_X ", "
                               "stream_id=%u.",
                               session_id, client_random_id, server_random_id,
                               stream_id());
    JB_RETURN(resync_session_and_stream_meta__server());
    JB_RETURN(sync_stream_meta__server());
  } else {
    JB_RETURN(send_response(stream_id_addr()));
    log_trace(jbooster, rpc)("New ServerStream: session_id=%u, stream_id=%u.", session_id, stream_id());
  }
  return 0;
}

int ServerStream::resync_session_and_stream_meta__server() {
  JB_RETURN(send_request(MessageType::ClientSessionMetaAgain));
  MessageType type;
  JB_RETURN(recv_request(type));
  if (type != MessageType::ClientSessionMeta) {
    JB_RETURN(JBErr::BAD_MSG_TYPE);
  }
  JB_RETURN(sync_session_meta__server());
  return 0;
}

// ============================ ThreadServerStreamMark =============================

ThreadServerStreamMark::ThreadServerStreamMark(ServerStream* server_stream, bool should_delete):
                                               _server_stream(server_stream),
                                               _delete(should_delete),
                                               _thread(Thread::current_or_null()) {
  guarantee(_thread == _server_stream->current_thread(), "sanity");
}

ThreadServerStreamMark::ThreadServerStreamMark(ServerStream* server_stream, bool should_delete, TRAPS):
                                               _server_stream(server_stream),
                                               _delete(should_delete),
                                               _thread(THREAD) {
  guarantee(_thread == _server_stream->current_thread(), "sanity");
}

ThreadServerStreamMark::~ThreadServerStreamMark() {
  if (_delete && (_thread == _server_stream->current_thread())) {
    delete _server_stream;
    log_debug(jbooster, rpc)("A ServerStream is automatically deleted.");
  }
}
