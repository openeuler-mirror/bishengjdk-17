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

#ifndef SHARE_JBOOSTER_JCLIENTARGUMENTS_HPP
#define SHARE_JBOOSTER_JCLIENTARGUMENTS_HPP

#include "jbooster/jClientVMFlags.hpp"
#include "jbooster/net/serialization.hpp"

class JClientBoostLevel {
  bool _allow_clr;
  bool _allow_cds;
  bool _allow_aot;
  bool _enable_aot_pgo;
  bool _enable_cds_agg;

public:
  JClientBoostLevel(): _allow_clr(false), _allow_cds(false), _allow_aot(false), _enable_aot_pgo(false), _enable_cds_agg(false) {}
  JClientBoostLevel(const JClientBoostLevel& level) = default;

  bool is_clr_allowed() const { return _allow_clr; }
  bool is_cds_allowed() const { return _allow_cds; }
  bool is_aot_allowed() const { return _allow_aot; }
  bool is_aot_pgo_enabled() const {return _enable_aot_pgo; }
  bool is_cds_agg_enabled() const {return _enable_cds_agg; }

  void set_allow_clr(bool allow_clr) { _allow_clr = allow_clr; }
  void set_allow_cds(bool allow_cds) { _allow_cds = allow_cds; }
  void set_allow_aot(bool allow_aot) { _allow_aot = allow_aot; }
  void set_enable_aot_pgo(bool enable_aot_pgo) { _enable_aot_pgo = enable_aot_pgo; }
  void set_enable_cds_agg(bool enable_cds_agg) { _enable_cds_agg = enable_cds_agg; }

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);
};

DECLARE_SERIALIZER_INTRUSIVE(JClientBoostLevel);

/**
 * Arguments that identify a program.
 */
class JClientArguments final: public CHeapObj<mtJBooster> {
  // @see test/hotspot/gtest/jbooster/test_server.cpp
  friend class Gtest_JBoosterServer;

  enum InitState {
    NOT_INITIALIZED_FOR_SEREVR,
    INITIALIZED_FOR_SEREVR,
    INITIALIZED_FOR_CLIENT,
    OBJECT_MOVED   // all members are shallowly copied to another object
  };

public:
  enum class CpuArch {
    CPU_UNKOWN,
    CPU_X86,
    CPU_ARM,
    CPU_AARCH64
  };

private:
  InitState _state;
  uint32_t _hash;

  // ======== parameters used to identify a program ========
  // All places with the comment "[FOR EACH ARG]" need to
  // be updated if you want to add a new argument.
  // [FOR EACH ARG]
  CpuArch     _cpu_arch;
  uint32_t    _jvm_version;
  const char* _internal_jvm_info;
  const char* _program_name;
  const char* _program_entry;
  bool        _is_jar;
  uint32_t    _classpath_name_hash;
  uint32_t    _classpath_timestamp_hash;
  uint32_t    _agent_name_hash;
  int64_t     _cds_file_size;
  const char* _java_commands;
  JClientVMFlags* _related_flags;
  // ========================= end =========================

private:
  void init_for_client();
  uint32_t calc_hash();

public:
  JClientArguments(bool is_client);
  JClientArguments(DebugUtils* ignored) {} // for GTEST only
  ~JClientArguments();

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  // [FOR EACH ARG]
  CpuArch         cpu_arch()                  const { return _cpu_arch; }
  uint32_t        jvm_version()               const { return _jvm_version; }
  const char*     internal_jvm_info()         const { return _internal_jvm_info; }
  const char*     program_name()              const { return _program_name; }
  const char*     program_entry()             const { return _program_entry; }
  bool            is_jar()                    const { return _is_jar; }
  uint32_t        classpath_name_hash()       const { return _classpath_name_hash; }
  uint32_t        classpath_timestamp_hash()  const { return _classpath_timestamp_hash; }
  uint32_t        agent_name_hash()           const { return _agent_name_hash; }
  const char*     java_commands()             const { return _java_commands; }
  JClientVMFlags* related_flags()             const { return _related_flags; }

  bool equals(const JClientArguments* that) const;
  uint32_t hash() const { return _hash; }
  void print_args(outputStream* st) const;

  JClientArguments* move();

  // This method is only used on the server.
  bool check_compatibility_with_server(const char** reason);

  static const char* cpu_arch_str(CpuArch cpu_arch);
};

DECLARE_SERIALIZER_INTRUSIVE(JClientArguments);

#endif // SHARE_JBOOSTER_JCLIENTARGUMENTS_HPP
