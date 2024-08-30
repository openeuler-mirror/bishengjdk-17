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

#include <sys/socket.h>

#include "jbooster/net/communicationStream.inline.hpp"

bool CommunicationStream::set_read_write_timeout(uint32_t timeout_ms) {
  timeval val_timeout = { timeout_ms / 1000 , (timeout_ms % 1000) * 1000 };
  if (setsockopt(_conn_fd, SOL_SOCKET, SO_RCVTIMEO, (void*) &val_timeout, sizeof(val_timeout)) < 0) {
    return false;
  }
  if (setsockopt(_conn_fd, SOL_SOCKET, SO_SNDTIMEO, (void*) &val_timeout, sizeof(val_timeout)) < 0) {
    return false;
  }
  return true;
}
