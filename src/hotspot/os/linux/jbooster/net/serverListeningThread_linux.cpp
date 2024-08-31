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
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include "jbooster/net/errorCode.hpp"
#include "jbooster/net/serverListeningThread.hpp"
#include "logging/log.hpp"
#include "runtime/interfaceSupport.inline.hpp"

static int init_server_socket_opts(int socket_fd) {
  // enable reuse of address
  int val_reuse_address = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&val_reuse_address, sizeof(val_reuse_address)) < 0) {
    log_error(jbooster, rpc)("Failed to set socket opt SO_REUSEADDR: %s.", strerror(errno));
    return -1;
  }

  // enable keep alive
  int val_keep_alive = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&val_keep_alive, sizeof(val_keep_alive)) < 0) {
    log_error(jbooster, rpc)("Failed to set socket opt SO_KEEPALIVE: %s.", strerror(errno));
    return -1;
  }

  return 0;
}

static int init_accepted_socket_opts(int conn_fd, uint32_t timeout_ms) {
  // set receiving and sending timeout to `timeout_ms`
  timeval val_timeout = { timeout_ms / 1000 , (timeout_ms % 1000) * 1000 };

  if (setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&val_timeout, sizeof(val_timeout)) < 0) {
    log_error(jbooster, rpc)("Failed to set socket opt SO_RCVTIMEO: %s.", strerror(errno));
    return -1;
  }

  if (setsockopt(conn_fd, SOL_SOCKET, SO_SNDTIMEO, (void*)&val_timeout, sizeof(val_timeout)) < 0) {
    log_error(jbooster, rpc)("Failed to set socket opt SO_SNDTIMEO: %s.", strerror(errno));
    return -1;
  }

  return 0;
}

/**
 * Returns the socket file descriptor (or -1 if failed to open socket).
 */
static int bind_address(const char* address, uint16_t port) {
  // open socket
  int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);  // AF_INET for IPv4 only
  if (socket_fd < 0) {
    log_error(jbooster, rpc)("Failed to open a socket.");
    return socket_fd;
  }

  // configure socket
  if (init_server_socket_opts(socket_fd) < 0) {
    close(socket_fd);
    return -1;
  }

  // bind address and port
  sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);
  if (bind(socket_fd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    log_error(jbooster, rpc)("Failed to bind the socket.");
    close(socket_fd);
    return -1;
  }

  // listen
  if (listen(socket_fd, SOMAXCONN) < 0) {
    log_error(jbooster, rpc)("Failed to listen to the socket.");
    close(socket_fd);
    return -1;
  }

  return socket_fd;
}

/**
 * Keep listening for client requests.
 */
int ServerListeningThread::run_listener(TRAPS) {
  ThreadToNativeFromVM ttn(THREAD);
  int server_fd = bind_address(_address, _port);
  if (server_fd < 0) {
    log_error(jbooster, rpc)("Failed to bind address '%s:%d'.", _address, (int) _port);
    return EADDRINUSE;
  }

  pollfd pfd = {0};
  pfd.fd = server_fd;
  pfd.events = POLLIN;

  sockaddr_in acc_addr;
  socklen_t acc_addrlen = sizeof(acc_addr);

  log_info(jbooster, rpc)("The JBooster server is serving on %s:%d.", _address, (int) _port);
  while (!get_exit_flag()) {
    int pcode = poll(&pfd, 1, 100);
    if (pcode < 0) {
      if (errno == EINTR) continue;
      log_error(jbooster, rpc)("Failed to poll: %s.", strerror(errno));
      return errno;
    }
    else if (pcode == 0) continue; // timed out
    else if (pfd.revents != POLLIN) {
      log_error(jbooster, rpc)("Unexpected poll revent: %d.", pfd.revents);
      return JBErr::UNKNOWN;
    }

    while (!get_exit_flag()) {
      int conn_fd = accept(server_fd, (sockaddr*)&acc_addr, &acc_addrlen);
      if (conn_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          log_error(jbooster, rpc)("Failed to accept: %s.", strerror(errno));
        }
        break;
      }
      if (init_accepted_socket_opts(conn_fd, _timeout_ms) < 0) break;

      handle_new_connection(conn_fd, THREAD);
    }
  }

  close(server_fd);
  log_debug(jbooster, rpc)("The JBooster server listener thread stopped.");
  return 0;
}
