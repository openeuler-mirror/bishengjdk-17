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
#include "jbooster/client/clientDaemonThread.hpp"
#include "jbooster/client/clientMessageHandler.hpp"
#include "jbooster/net/clientStream.hpp"
#include "logging/log.hpp"
#include "runtime/globals.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/thread.inline.hpp"

JavaThread* ClientDaemonThread::_the_java_thread = nullptr;

void ClientDaemonThread::start_thread(TRAPS) {
  JavaThread* new_thread = new JavaThread(&client_daemon_thread_entry);
  JavaThread::vm_exit_on_osthread_failure(new_thread);
  guarantee(_the_java_thread == nullptr, "sanity");
  _the_java_thread = new_thread;

  Handle string = java_lang_String::create_from_str("JBooster Client Daemon", CATCH);
  Handle thread_group(THREAD, Universe::system_thread_group());
  Handle thread_oop = JavaCalls::construct_new_instance(
          vmClasses::Thread_klass(),
          vmSymbols::threadgroup_string_void_signature(),
          thread_group,
          string,
          CATCH);

  Klass* group = vmClasses::ThreadGroup_klass();
  JavaValue result(T_VOID);
  JavaCalls::call_special(&result,
                        thread_group,
                        group,
                        vmSymbols::add_method_name(),
                        vmSymbols::thread_void_signature(),
                        thread_oop,
                        THREAD);

  JavaThread::start_internal_daemon(THREAD, new_thread, thread_oop, MinPriority);
}

void ClientDaemonThread::daemon_run(TRAPS) {
  ThreadToNativeFromVM ttn(THREAD);

  ClientStream cs(JBoosterAddress, JBoosterPort, JBoosterManager::heartbeat_timeout(), THREAD);
  JB_TRY {
    JB_THROW(cs.connect_and_init_stream());
    JB_THROW(cs.send_request(MessageType::ClientDaemonTask));

    while (true) {
      JB_THROW(daemon_loop(&cs, THREAD));
    }
  } JB_TRY_END
  JB_CATCH_REST() {
    log_warning(jbooster)("Heartbeat timeout. The server seems to be dead.");
  } JB_CATCH_END;
}

int ClientDaemonThread::daemon_loop(ClientStream* client_stream, TRAPS) {
  MessageType type;
  JB_RETURN(client_stream->recv_request(type));
  switch (type) {
    case MessageType::Heartbeat:
      JB_RETURN(handle_heartbeat(client_stream));
      break;
    default:
      guarantee(false, "handle it");
  }
  return 0;
}

int ClientDaemonThread::handle_heartbeat(ClientStream* client_stream) {
  int magic;
  JB_RETURN(client_stream->parse_request(&magic));
  JB_RETURN(client_stream->send_response(&magic));
  return 0;
}
