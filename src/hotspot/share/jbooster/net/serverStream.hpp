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

#ifndef SHARE_JBOOSTER_NET_SERVERSTREAM_HPP
#define SHARE_JBOOSTER_NET_SERVERSTREAM_HPP

#include "jbooster/net/communicationStream.inline.hpp"
#include "memory/allocation.hpp"

class JClientSessionData;
class JClientProgramData;

class ServerStream: public CommunicationStream {
  JClientSessionData* _session_data;

private:
  void set_session_data(JClientSessionData* sd);

  int sync_session_meta__server();
  int sync_stream_meta__server();
  int resync_session_and_stream_meta__server();

  int handle_sync_requests(JClientProgramData* pd, bool enable_cds_agg, bool enable_aot_pgo);

public:
  ServerStream(int conn_fd, SSL* ssl);
  ServerStream(int conn_fd, SSL* ssl, Thread* thread);
  ~ServerStream();

  void handle_meta_request(uint32_t stream_id);

  JClientSessionData* session_data() { return _session_data; }
  uint32_t session_id();
}; // ServerStream

/**
 * Use RAII to auto deleting closed server stream.
 * The server stream will be deleted only if it's handled by current thread.
 */
class ThreadServerStreamMark: public StackObj {
  ServerStream* const _server_stream;
  const bool _delete;
  Thread* const _thread;

public:
  ThreadServerStreamMark(ServerStream* server_stream, bool should_delete);
  ThreadServerStreamMark(ServerStream* server_stream, bool should_delete, TRAPS);
  ~ThreadServerStreamMark();
};

#endif // SHARE_JBOOSTER_NET_SERVERSTREAM_HPP
