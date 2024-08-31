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

#ifndef SHARE_JBOOSTER_NET_CLIENTSTREAM_HPP
#define SHARE_JBOOSTER_NET_CLIENTSTREAM_HPP

#include "jbooster/net/communicationStream.inline.hpp"

class ClientStream: public CommunicationStream {
private:
  const char* const _server_address;
  const char* const _server_port;
  const uint32_t _timeout_ms;

  bool _inform_before_close;

private:
  static int try_to_connect_once(int* res_fd, const char* address, const char* port, uint32_t timeout_ms);

  int request_cache_file(bool* use_it,
                         bool allowed_to_use,
                         bool local_cache_exists,
                         bool remote_cache_exists,
                         const char* file_path,
                         MessageType msg_type);

  int connect_to_server();

  int sync_session_meta__client(bool* use_clr, bool* use_cds, bool* use_aot);
  int sync_stream_meta__client();
  int resync_session_and_stream_meta__client();

public:
  ClientStream(const char* address, const char* port, uint32_t timeout_ms);
  ClientStream(const char* address, const char* port, uint32_t timeout_ms, Thread* thread);
  ~ClientStream();

  void set_inform_before_close(bool should) { _inform_before_close = should; }

  int connect_and_init_session(bool* use_clr, bool* use_cds, bool* use_aot);
  int connect_and_init_stream();
  void send_no_more_compilation_tasks();

  void log_or_crash(const char* file, int line, int err_code);
};

#define LOG_OR_CRASH() log_or_crash(__FILE__, __LINE__, JB_ERR)

#endif // SHARE_JBOOSTER_NET_CLIENTSTREAM_HPP
