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

#include "jbooster/net/messageType.hpp"

#define REGISTER_MESSAGE_TYPE_STR(type_name, human_readable) #type_name,

static const char* msg_type_name_arr[] = {
  MESSAGE_TYPES(REGISTER_MESSAGE_TYPE_STR)
  "END_OF_MESSAGE_TYPE"
};

const char* msg_type_name(MessageType meg_type) {
  int mt = (int)meg_type;
  if (mt < 0) return "UNKNOWN_NEGATIVE";
  if (mt >= (int)(sizeof(msg_type_name_arr) / sizeof(char*))) return "UNKNOWN_POSITIVE";
  return msg_type_name_arr[(int)meg_type];
}

#undef REGISTER_MESSAGE_TYPE_STR
