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

#ifndef SHARE_JBOOSTER_NET_ERRORCODE_HPP
#define SHARE_JBOOSTER_NET_ERRORCODE_HPP

#include <errno.h>

#define JB_ERROR_CODES(f)                                                           \
  f(CONN_CLOSED,              "Connection has been closed"                        ) \
  f(CONN_CLOSED_BY_PEER,      "Connection is closed by the other end"             ) \
  f(BAD_MSG_SIZE,             "Unexpected size of the received message"           ) \
  f(BAD_MSG_TYPE,             "Unexpected message type of the received message"   ) \
  f(BAD_MSG_DATA,             "Unexpected payload data of the received message"   ) \
  f(BAD_ARG_SIZE,             "Unexpected size of the argument"                   ) \
  f(BAD_ARG_DATA,             "Unexpected payload data of the argument"           ) \
  f(INCOMPATIBLE_RPC,         "Incompatible RPC version"                          ) \
  f(DESER_TERMINATION,        "Deserialization terminated early (not an error)"   ) \
  f(ABORT_CUR_PHRASE,         "Abort current communication phrase (not an error)" ) \
  f(THREAD_EXCEPTION,         "Java exception from Thread::current()"             ) \
  f(UNKNOWN,                  "Unknown error"                                     ) \


#define REGISTER_JB_ERROR_CODE_OPPOSITE_IDS(err_name, human_readable) err_name,
#define REGISTER_JB_ERROR_CODES(err_name, human_readable) err_name = -(int)ErrCodeOpposite::err_name,

/**
 * Defines all the possible error codes for the jbooster network communication.
 * We define all the jbooster error codes as negative integers as C standard
 * already defines some positive errnos.
 *
 * @see static const char* errno_to_string (int e, bool short_text)
 */
class JBErr {
private:
  enum class ErrCodeOpposite: int {
    PLACE_HOLDER_OF_ZERO,
    JB_ERROR_CODES(REGISTER_JB_ERROR_CODE_OPPOSITE_IDS)
    PLACE_HOLDER_END
  };

  static const int _err_cnt = (int) ErrCodeOpposite::PLACE_HOLDER_END - 1;

public:
  enum: int {
    JB_ERROR_CODES(REGISTER_JB_ERROR_CODES)
    PLACE_HOLDER_END
  };

  static int err_cnt() { return _err_cnt; }

  static const char* err_name(int err_code);
  static const char* err_message(int err_code);
};

#undef REGISTER_JB_ERROR_CODE_OPPOSITE_IDS
#undef REGISTER_JB_ERROR_CODES

#endif // SHARE_JBOOSTER_NET_ERRORCODE_HPP
