/*
 * Copyright (c) 2020, 2024, Huawei Technologies Co., Ltd. All rights reserved.
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

#include "precompiled.hpp"
#include "utilities/macros.hpp"

#if INCLUDE_JBOOSTER

#include "common.hpp"

#include "jbooster/jClientArguments.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/utilities/concurrentHashMap.inline.hpp"
#include "jbooster/utilities/debugUtils.inline.hpp"
#include "utilities/stringUtils.hpp"

#include "unittest.hpp"

struct Gtest_JBoosterServer {
  static JClientArguments* create_mock_JClientArguments(uint32_t id) {
    JClientArguments* mock = new JClientArguments((DebugUtils*) nullptr);

    // [FOR EACH ARG]
    mock->_cpu_arch                 = JClientArguments::CpuArch::CPU_UNKOWN;
    mock->_jvm_version              = id;
    mock->_internal_jvm_info        = StringUtils::copy_to_heap("internaljvminfo", mtJBooster);
    mock->_program_name             = StringUtils::copy_to_heap("programname", mtJBooster);
    mock->_program_entry            = StringUtils::copy_to_heap("programentry", mtJBooster);
    mock->_is_jar                   = true;
    mock->_classpath_name_hash      = 1u;
    mock->_classpath_timestamp_hash = 2u;
    mock->_agent_name_hash          = 3u;
    mock->_cds_file_size            = 456;
    mock->_java_commands            = StringUtils::copy_to_heap("javacommands", mtJBooster);
    mock->_related_flags            = new JClientVMFlags(true);

    mock->_hash = mock->calc_hash();
    mock->_state = JClientArguments::INITIALIZED_FOR_SEREVR;

    return mock;
  }

  static JClientProgramData* create_mock_JClientProgramData(uint32_t program_id, JClientArguments* program_args) {
    JClientProgramData* mock = new JClientProgramData((DebugUtils*) nullptr);

    mock->_program_id = program_id;
    mock->_program_args = program_args->move();
    mock->_ref_cnt.inc();
    mock->_program_str_id = StringUtils::copy_to_heap("programstrid", mtJBooster);
    mock->clr_cache_state().init(StringUtils::copy_to_heap("gtestclr", mtJBooster));
    mock->dy_cds_cache_state().init(StringUtils::copy_to_heap("gtestdycds", mtJBooster));
    mock->agg_cds_cache_state().init(StringUtils::copy_to_heap("gtestaggcds", mtJBooster));
    mock->aot_static_cache_state().init(StringUtils::copy_to_heap("gtestaotstatic", mtJBooster));
    mock->aot_pgo_cache_state().init(StringUtils::copy_to_heap("gtestaotpgo", mtJBooster));

    return mock;
  }
};

TEST_VM(JBoosterServer, program_data_ref_cnt) {
  JavaThread* THREAD = JavaThread::current();
  ServerDataManager::JClientProgramDataMap programs;
  JClientArguments* program_args1 = Gtest_JBoosterServer::create_mock_JClientArguments(11);
  JClientProgramData* new_pd1 = Gtest_JBoosterServer::create_mock_JClientProgramData(1u, program_args1);
  delete program_args1;
  JClientProgramData** res1 = programs.put_if_absent(new_pd1->program_args(), new_pd1, THREAD);
  EXPECT_NE(res1, (JClientProgramData**) nullptr);
  EXPECT_EQ(*res1, new_pd1);
  EXPECT_EQ(new_pd1->ref_cnt().get(), 1);

  JClientArguments* program_args2 = Gtest_JBoosterServer::create_mock_JClientArguments(11);
  JClientProgramData* new_pd2 = Gtest_JBoosterServer::create_mock_JClientProgramData(2u, program_args2);
  delete program_args2;
  JClientProgramData** res2 = programs.put_if_absent(new_pd2->program_args(), new_pd2, THREAD);
  EXPECT_NE(res2, (JClientProgramData**) nullptr);
  EXPECT_EQ(*res2, new_pd1);
  EXPECT_EQ(new_pd1->ref_cnt().get(), 2);
  EXPECT_EQ(new_pd2->ref_cnt().get(), 1);

  new_pd2->ref_cnt().dec_and_update_time();
  delete new_pd2;

  new_pd1->ref_cnt().dec_and_update_time();
  new_pd1->ref_cnt().dec_and_update_time();
}

#endif // INCLUDE_JBOOSTER
