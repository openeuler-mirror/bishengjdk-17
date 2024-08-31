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

#ifndef SHARE_JBOOSTER_CLIENT_CLIENTDAEMONTHREAD_HPP
#define SHARE_JBOOSTER_CLIENT_CLIENTDAEMONTHREAD_HPP

#include "jbooster/jBoosterManager.hpp"
#include "runtime/thread.hpp"

class ClientStream;

/**
 * Goals of this thread:
 * - Send keep-alive packages to the server;
 * - Send bytecode of the newly loaded klasses to the server;
 *
 * This thread is simple, so all static methods are used.
 */
class ClientDaemonThread: public AllStatic {
private:
  static JavaThread* _the_java_thread;

  static void client_daemon_thread_entry(JavaThread* thread, TRAPS) { daemon_run(thread); }

  static void daemon_run(TRAPS);
  static int daemon_loop(ClientStream* client_stream, TRAPS);

  static int handle_heartbeat(ClientStream* client_stream);

public:
  static void start_thread(TRAPS);
};

#endif // SHARE_JBOOSTER_CLIENT_CLIENTDAEMONTHREAD_HPP
