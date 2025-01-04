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

#include <netdb.h>        // for addrinfo
#include <netinet/tcp.h>  // for TCP_NODELAY
#include <string.h>       // for memset()
#include <sys/socket.h>

#include "jbooster/net/clientStream.hpp"
#include "jbooster/net/errorCode.hpp"
#include "jbooster/net/sslUtils.hpp"
#include "logging/log.hpp"
#include "runtime/java.hpp"
#include "runtime/os.hpp"

#define LOG_INNER(socket_fd, address, port, err_code, format, args...)                                \
  log_error(jbooster, rpc)(format ": socket=%d, address=\"%s:%s\", error=%s(\"%s\").",                \
                           ##args, socket_fd, address, port,                                          \
                           JBErr::err_name(err_code), JBErr::err_message(err_code));                  \

static int resolve_address(addrinfo** res_addr, const char* address, const char* port) {
  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags     = 0;
  hints.ai_family    = AF_INET;     // IPv4 only
  hints.ai_socktype  = SOCK_STREAM;
  hints.ai_protocol  = 0;           // any protocol

  int gai_err = getaddrinfo(address, port, &hints, res_addr);
  if (gai_err != 0) {
    if (gai_err == EAI_SYSTEM) {
      LOG_INNER(-1, address, port, errno, "Failed to get addrinfo");
    } else {
      log_error(jbooster, rpc)("Failed to get addrinfo: address=\"%s:%s\", gai_err=\"%s\".",
                              address, port, gai_strerror(gai_err));
    }
    return -1;
  }
  return 0;
}

/**
 * Initializes the options of the client socket.
 */
static int init_socket_opts(int socket_fd, const char* address, const char* port, uint32_t timeout_ms) {
#define TRY_SETSOCKOPT(socket_fd, level, optname, optval)                             \
  if (setsockopt(socket_fd, level, optname, (void*)&optval, sizeof(optval)) < 0) {    \
    LOG_INNER(socket_fd, address, port, errno, "Failed to set socket opt " #optname); \
    return -1;                                                                        \
  }

  // enable keep alive
  int val_keep_alive = 1;
  TRY_SETSOCKOPT(socket_fd, SOL_SOCKET, SO_KEEPALIVE, val_keep_alive);

  // set linger after close() or shutdown() to 2 seconds
  linger val_linger = { 1, 2 };
  TRY_SETSOCKOPT(socket_fd, SOL_SOCKET, SO_LINGER, val_linger);

  // set receiving and sending timeout to `timeout_ms`
  timeval val_timeout = { timeout_ms / 1000 , (timeout_ms % 1000) * 1000 };
  TRY_SETSOCKOPT(socket_fd, SOL_SOCKET, SO_RCVTIMEO, val_timeout);
  TRY_SETSOCKOPT(socket_fd, SOL_SOCKET, SO_SNDTIMEO, val_timeout);

  // disable Nagle to reduce latency
  int val_no_delay = 1;
  TRY_SETSOCKOPT(socket_fd, IPPROTO_TCP, TCP_NODELAY, val_no_delay);

  return 0;
#undef TRY_SETSOCKOPT
}

int ClientStream::try_to_connect_once(int* res_fd, const char* address, const char* port, uint32_t timeout_ms) {
  errno = 0;

  int res_err = 0;
  int conn_fd = -1;
  // addrinfo res of getaddrinfo()
  addrinfo* res_addr = nullptr;

  do {
    if (resolve_address(&res_addr, address, port) < 0) {
      res_err = errno;
      break;
    }

    // open socket
    addrinfo* addr_p;
    for (addr_p = res_addr; addr_p; addr_p = addr_p->ai_next) {
      conn_fd = socket(addr_p->ai_family, addr_p->ai_socktype, addr_p->ai_protocol);
      if (conn_fd >= 0) break;
    }
    if (conn_fd < 0) {
      res_err = errno;
      LOG_INNER(conn_fd, address, port, res_err, "Failed to create the socket");
      break;
    }

    // configure socket
    if (init_socket_opts(conn_fd, address, port, timeout_ms) < 0) {
      res_err = errno;
      break;
    }

    // connect socket
    if (connect(conn_fd, addr_p->ai_addr, addr_p->ai_addrlen) < 0) {
      res_err = errno;
      LOG_INNER(conn_fd, address, port, res_err, "Failed to build the connection");
      break;
    }

    // success
    assert(errno == 0, "why errno=%s", JBErr::err_name(errno));
    freeaddrinfo(res_addr);
    *res_fd = conn_fd;
    return 0;
  } while (false);

  // fail
  errno = 0;
  if (res_addr != nullptr) freeaddrinfo(res_addr);
  if (conn_fd >= 0) close(conn_fd);
  if (res_err == 0) res_err = JBErr::UNKNOWN;
  *res_fd = -1;
  return res_err;
}

int ClientStream::try_to_ssl_connect(SSL** res_ssl, int conn_fd) {
  errno = 0;

  SSL* ssl = nullptr;
  do {
    SSL* ssl = SSLUtils::ssl_new(_client_ssl_ctx);
    if (ssl == nullptr) {
      log_error(jbooster, rpc)("Failed to get SSL.");
      break;
    }

    int ret = 0;
    if (ret = SSLUtils::ssl_set_fd(ssl, conn_fd) != 1) {
      SSLUtils::handle_ssl_error(ssl, ret, "Failed to set SSL file descriptor");
      break;
    }

    if (ret = SSLUtils::ssl_connect(ssl) != 1) {
      SSLUtils::handle_ssl_error(ssl, ret, "Failed to build SSL connection");
      break;
    }

    if (!verify_cert(ssl)) {
      break;
    }

    // success
    assert(errno == 0, "why errno=%s", JBErr::err_name(errno));
    *res_ssl = ssl;
    return 0;
  } while (false);

  // fail
  int res_err = JBErr::BAD_SSL;
  SSLUtils::shutdown_and_free_ssl(ssl);
  return res_err;
}

bool ClientStream::verify_cert(SSL* ssl) {
  X509* cert = SSLUtils::ssl_get_peer_certificate(ssl);
  if ((cert == nullptr)) {
    log_error(jbooster, rpc)("Server cert unspecified.");
    return false;
  }
  int res = SSLUtils::ssl_get_verify_result(ssl);
  if (res != X509_V_OK) {
    log_error(jbooster, rpc)("Failed to verify server cert.");
    return false;
  }
  return true;
}
