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

#ifndef SHARE_JBOOSTER_CLIENT_CLIENTDATAMANAGER_HPP
#define SHARE_JBOOSTER_CLIENT_CLIENTDATAMANAGER_HPP

#include "jbooster/jBoosterManager.hpp"
#include "jbooster/jClientArguments.hpp"
#include "jbooster/utilities/fileUtils.hpp"

class ClientDataManager: public CHeapObj<mtJBooster> {
  friend class ClientStream;

  static ClientDataManager* _singleton;

  uint64_t _random_id;
  bool _startup_end;
  JClientArguments* _program_args;
  const char* _program_str_id;
  const char* _cache_dir_path;

  JClientBoostLevel _boost_level;

  bool _using_clr;
  bool _using_cds;
  bool _using_aot;

  const char* _cache_clr_path;
  const char* _cache_cds_path;
  const char* _cache_aot_path;

  uint32_t _session_id;
  uint32_t _program_id;
  uint64_t _server_random_id;
  bool _server_available;

private:
  NONCOPYABLE(ClientDataManager);

  ClientDataManager();

  void init_client_vm_options();
  void init_const();
  void init_client_duty();
  void init_client_duty_under_local_mode();
  jint init_clr_options();
  jint init_cds_options();
  jint init_aot_options();
  jint init_pgo_options();
  void print_init_state();

protected:
  void set_session_id(uint32_t sid) { _session_id = sid; }
  void set_program_id(uint32_t pid) { _program_id = pid; }
  void set_server_random_id(uint64_t id) { _server_random_id = id; }

public:
  static ClientDataManager& get() {
    JBoosterManager::client_only();
    assert(_singleton != nullptr, "sanity");
    return *_singleton;
  }

  static jint init_phase1();
  static void init_phase2(TRAPS);

  // Escape to the original path without jbooster if the server cannot be connected.
  static jint escape();

  uint64_t random_id() { return _random_id; }

  bool is_startup_end() { return _startup_end; }
  void set_startup_end() { _startup_end = true; }

  JClientArguments* program_args() { return _program_args; }
  const char* program_str_id() { return _program_str_id; }

  // $HOME/.jbooster/client
  const char* cache_dir_path() { return _cache_dir_path; }

  JClientBoostLevel& boost_level() { return _boost_level; }

  bool is_clr_being_used() { return _using_clr; }
  bool is_cds_being_used() { return _using_cds; }
  bool is_aot_being_used() { return _using_aot; }

  // <cache_dir>/client/cache-<parogram_str_id>-clr.log
  const char* cache_clr_path() { return _cache_clr_path; }
  // <cache_dir>/client/cache-<parogram_str_id>-cds-[dy|agg].jsa
  const char* cache_cds_path() { return _cache_cds_path; }
  // <cache_dir>/client/cache-<parogram_str_id>-aot-[static|pgo].so
  const char* cache_aot_path() { return _cache_aot_path; }

  uint32_t session_id() { return _session_id; }
  uint32_t program_id() { return _program_id; }

  uint64_t server_random_id()            { return _server_random_id; }

  bool is_server_available()             { return _server_available; }
  void set_server_available(bool avl)    { _server_available = avl; }
};

#endif // SHARE_JBOOSTER_CLIENT_CLIENTDATAMANAGER_HPP
