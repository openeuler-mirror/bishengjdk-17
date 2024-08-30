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

#ifndef SHARE_JBOOSTER_JBOOSTERMANAGER_HPP
#define SHARE_JBOOSTER_JBOOSTERMANAGER_HPP

#include "jbooster/jbooster_globals.hpp"
#include "memory/allocation.hpp"
#include "runtime/globals.hpp"
#include "utilities/exceptions.hpp"

/**
 * Manage some common parts about JBooster.
 * @see ClientDataManager
 * @see ServerDataManager
 */
class JBoosterManager: public AllStatic {
public:
  static const uint32_t _heartbeat_timeout = 4 * 60 * 1000; // 4 minutes

private:
  static jint init_common();

public:
  // Invoked in Threads::create_vm().
  static jint init_phase1();
  static void init_phase2(TRAPS);

  static void client_only() NOT_DEBUG_RETURN;
  static void server_only() NOT_DEBUG_RETURN;

  static uint64_t calc_random_id();

  static const char* calc_cache_dir_path(bool is_client);
  static const char* calc_program_string_id(const char* program_name,
                                            const char* program_entry,
                                            bool is_jar,
                                            uint32_t program_hash);
  static const char* calc_cache_path(const char* cache_dir,
                                     const char* program_str_id,
                                     const char* cache_type);
  static const char* calc_tmp_cache_path(const char* cache_path);

  static int create_and_open_tmp_cache_file(const char* tmp_cache_path);

  static uint32_t heartbeat_timeout() { return _heartbeat_timeout; }
};

#endif // SHARE_JBOOSTER_JBOOSTERMANAGER_HPP
