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
#include "jbooster/net/serverListeningThread.hpp"
#include "jbooster/net/serverStream.hpp"
#include "jbooster/net/sslUtils.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/server/serverMessageHandler.hpp"
#include "logging/log.hpp"
#include "runtime/atomic.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/os.inline.hpp"
#include "runtime/thread.inline.hpp"

ServerListeningThread* ServerListeningThread::_singleton = nullptr;
SSL_CTX* ServerListeningThread::_server_ssl_ctx = nullptr;

/**
 * This function is called in the main thread.
 */
ServerListeningThread* ServerListeningThread::start_thread(const char* address,
                                                           uint16_t port,
                                                           uint32_t timeout_ms,
                                                           const char* ssl_key,
                                                           const char* ssl_cert,
                                                           TRAPS) {
  if ((ssl_key == nullptr && ssl_cert != nullptr) || (ssl_key != nullptr && ssl_cert == nullptr)) {
    log_error(jbooster, rpc)("You must include --ssl-cert and --ssl-key together.");
    vm_exit(1);
  }
  if (ssl_key != nullptr && ssl_cert != nullptr) {
    if (!SSLUtils::init_ssl_lib()) {
      log_error(jbooster, rpc)("Failed to load all functions from OpenSSL Dynamic Library.");
      vm_exit(1);
    }
    server_init_ssl_ctx(ssl_key, ssl_cert);
  }

  JavaThread* new_thread = new JavaThread(&server_listener_thread_entry);
  guarantee(new_thread != nullptr && new_thread->osthread() != nullptr, "sanity");
  guarantee(_singleton == nullptr, "sanity");
  _singleton = new ServerListeningThread(new_thread, address, port, timeout_ms);

  Handle name = java_lang_String::create_from_str("JBooster Listening Thread", CHECK_NULL);
  Handle thread_group(THREAD, Universe::main_thread_group());
  Handle thread_oop = JavaCalls::construct_new_instance(
                                 vmClasses::Thread_klass(),
                                 vmSymbols::threadgroup_string_void_signature(),
                                 thread_group,
                                 name,
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

  JavaThread::start_internal_daemon(THREAD, new_thread, thread_oop, NearMaxPriority);
  return _singleton;
}

void ServerListeningThread::server_init_ssl_ctx(const char* ssl_key, const char* ssl_cert) {
  SSLUtils::openssl_init_ssl();

  _server_ssl_ctx = SSLUtils::ssl_ctx_new(SSLUtils::sslv23_server_method());
  if (_server_ssl_ctx == nullptr) {
    log_error(jbooster, rpc)("Failed to create SSL context");
    vm_exit(1);
  }

  SSLUtils::ssl_ctx_set_options(_server_ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2);

  const int security_level = 2;
  SSLUtils::ssl_ctx_set_security_level(_server_ssl_ctx, security_level);

  const char* error_description = nullptr;
  if (SSLUtils::ssl_ctx_use_certificate_file(_server_ssl_ctx, ssl_cert, SSL_FILETYPE_PEM) != 1) {
    error_description = "Failed to add certificate file to SSL context.";
    SSLUtils::handle_ssl_ctx_error(error_description);
  }
  if (SSLUtils::ssl_ctx_use_privatekey_file(_server_ssl_ctx, ssl_key, SSL_FILETYPE_PEM) != 1) {
    error_description = "Failed to add private key file to SSL context.";
    SSLUtils::handle_ssl_ctx_error(error_description);
  }
  if (SSLUtils::ssl_ctx_check_private_key(_server_ssl_ctx) != 1) {
    error_description = "Private key doesn't match certificate.";
    SSLUtils::handle_ssl_ctx_error(error_description);
  }
}

void ServerListeningThread::server_listener_thread_entry(JavaThread* thread, TRAPS) {
  JB_TRY {
    JB_THROW(_singleton->run_listener(thread));
  } JB_TRY_END
  JB_CATCH_REST() {
    log_error(jbooster, rpc)("ServerListeningThread failed to listen on the port: error=%s(\"%s\").",
                             JBErr::err_name(JB_ERR), JBErr::err_message(JB_ERR));
    vm_exit(1);
  } JB_CATCH_END;
  vm_exit(0);
}

ServerListeningThread::ServerListeningThread(JavaThread* the_java_thread,
                                             const char* address,
                                             uint16_t port,
                                             uint32_t timeout_ms):
        _the_java_thread(the_java_thread),
        _address(address),
        _port(port),
        _timeout_ms(timeout_ms),
        _stream_id_for_alloc(0),
        _exit_flag(false) {
}

ServerListeningThread::~ServerListeningThread() {
  log_debug(jbooster, rpc)("The JBooster server listener thread is destroyed.");
}

uint32_t ServerListeningThread::new_stream_id() {
  return Atomic::add(&_stream_id_for_alloc, 1U);
}

/**
 * Add the new connection to the connection thread pool.
 */
void ServerListeningThread::handle_new_connection(int conn_fd, SSL* ssl, TRAPS) {
  ThreadInVMfromNative tiv(THREAD);
  ResourceMark rm(THREAD);
  HandleMark hm(THREAD);

  JavaValue result(T_BOOLEAN);
  JavaCallArguments args;
  args.push_int(conn_fd);
  args.push_long((jlong)(uintptr_t) ssl);
  JavaCalls::call_static(&result, ServerDataManager::get().main_klass(),
                         vmSymbols::receiveConnection_name(),
                         vmSymbols::receiveConnection_signature(),
                         &args, CATCH);
  if (!result.get_jboolean()) {
    log_warning(jbooster, rpc)("Failed to handle the new connection as the thread pool is full.");
    SSLUtils::shutdown_and_free_ssl(ssl);
    os::close(conn_fd);
  }
}

/**
 * Handle the connection obtained from the connection thread pool.
 *
 * This function is called in another java thread (not in ServerListeningThread thread).
 * So do not use `this` here.
 */
void ServerListeningThread::handle_connection(int conn_fd, long ssl) {
  JavaThread* THREAD = JavaThread::current();
  ThreadToNativeFromVM ttn(THREAD);

  ServerStream* server_stream = new ServerStream(conn_fd, (SSL*)(uintptr_t) ssl, THREAD);
  ThreadServerStreamMark tssm(server_stream, true, THREAD);
  server_stream->handle_meta_request(new_stream_id());
  if (server_stream->is_stream_closed()) return;

  ServerMessageHandler msg_handler(server_stream);
  JB_TRY {
    JB_THROW(msg_handler.handle_tasks_from_client(THREAD));
  } JB_TRY_END
  JB_CATCH_REST() {
    log_warning(jbooster, rpc)("Unhandled exception at ServerListeningThread::handle_connection(): "
                               "error=%s(\"%s\"), session_id=%u, stream_id=%u.",
                               JBErr::err_name(JB_ERR), JBErr::err_message(JB_ERR),
                               server_stream->session_id(), server_stream->stream_id());
  } JB_CATCH_END;
}
