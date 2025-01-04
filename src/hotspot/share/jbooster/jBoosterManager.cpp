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

#include "jbooster/client/clientDataManager.hpp"
#include "jbooster/jBoosterManager.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "logging/log.hpp"
#include "runtime/arguments.hpp"
#include "runtime/java.hpp"
#include "runtime/os.hpp"
#include "utilities/formatBuffer.hpp"

#ifdef ASSERT
void JBoosterManager::client_only() {
  assert(UseJBooster, "client only");
}

void JBoosterManager::server_only() {
  assert(AsJBooster, "server only");
}
#endif // ASSERT

const char* JBoosterManager::calc_cache_dir_path(bool is_client) {
  const char* fs = FileUtils::separator();
  const char* user_home = FileUtils::home_path();
  int buf_len = strlen(user_home) + strlen(fs) * 2 + 9 /* ".jbooster" */ + 6 /* "client" or "server" */ + 1;
  char* buf = NEW_C_HEAP_ARRAY(char, buf_len, mtJBooster);
  int cnt = snprintf(buf, buf_len, "%s%s.jbooster%s%s", user_home, fs, fs, is_client ? "client" : "server");
  guarantee(buf_len == cnt + 1, "sanity");
  return buf;
}

const char* JBoosterManager::calc_program_string_id(const char* program_name,
                                                    const char* program_entry,
                                                    bool is_jar,
                                                    uint32_t program_hash) {
  // use simple class name (if it is a class name)
  const char* tmp = is_jar ? nullptr : strrchr(program_entry, '.');
  const char* short_entry = (tmp == nullptr ? program_entry : tmp + 1);

  int max_res_len = -1;
  char* res = nullptr;
  int snp_len = 0;
  if (program_name == nullptr) {
    // short_entry + '-' + program_hash + '\0'
    max_res_len = strlen(short_entry) + 1 + 8 + 1;
    res = NEW_C_HEAP_ARRAY(char, max_res_len, mtJBooster);
    snp_len = snprintf(res, max_res_len, "%s-%x", short_entry, program_hash);
  } else {
    // program_name + '-' + short_entry + '-' + program_hash + '\0'
    max_res_len = strlen(program_name) + 1 + strlen(short_entry) + 1 + 8 + 1;
    res = NEW_C_HEAP_ARRAY(char, max_res_len, mtJBooster);
    snp_len = snprintf(res, max_res_len, "%s-%s-%x", program_name, short_entry, program_hash);
  }
  guarantee(snp_len <= max_res_len, "sanity");
  log_info(jbooster)("Human-readable program string id: \"%s\".", res);
  return res;
}

const char* JBoosterManager::calc_cache_path(const char* cache_dir,
                                             const char* program_str_id,
                                             const char* cache_type) {
  const char* fs = FileUtils::separator();
  int buf_len = strlen(cache_dir) + strlen(fs) + 6 + strlen(program_str_id) + 1 + strlen(cache_type) + 1;
  char* buf = NEW_C_HEAP_ARRAY(char, buf_len, mtJBooster);
  int cnt = snprintf(buf, buf_len, "%s%scache-%s-%s", cache_dir, fs, program_str_id, cache_type);
  guarantee(buf_len == cnt + 1, "sanity");
  return buf;
}

/**
 * Returns "<cache-path>.tmp".
 * The returned tmp path string is allocated on the heap!
 */
const char* JBoosterManager::calc_tmp_cache_path(const char* cache_path) {
  int cache_path_len = strlen(cache_path);
  const int tmp_cache_path_len = cache_path_len + 5; // ".tmp"
  char* tmp_cache_path = NEW_C_HEAP_ARRAY(char, tmp_cache_path_len, mtJBooster);
  snprintf(tmp_cache_path, tmp_cache_path_len, "%s.tmp", cache_path);
  return tmp_cache_path;
}

uint64_t JBoosterManager::calc_random_id() {
  // nano_time + random_int, to make it as unique as possible.
  return (((uint64_t) os::javaTimeNanos()) >> 4) ^ (((uint64_t) os::random()) << (64 - 8 - 4));
}

/**
 * Try to create and open the tmp file atomically (we treat the tmp file
 * as a lock). The file will not be opened if it already exists.
 * Returns the file fd.
 */
int JBoosterManager::create_and_open_tmp_cache_file(const char* tmp_cache_path) {
  // Use O_CREAT & O_EXCL to make sure the opened file is created by the
  // current thread.
  return os::open(tmp_cache_path, O_BINARY | O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, 0600);
}

jint JBoosterManager::init_common() {
  if (FLAG_SET_CMDLINE(CalculateClassFingerprint, true) != JVMFlag::SUCCESS) {
    return JNI_EINVAL;
  }

  return JNI_OK;
}

jint JBoosterManager::init_phase1() {
  // NOTICE: In this phase Thread::current() is not initialized yet.
  assert(UseJBooster || AsJBooster, "sanity");
  if (UseJBooster && AsJBooster) {
    vm_exit_during_initialization("Both UseJBooster and AsJBooster are set?!");
  }

  jint jni_err = JNI_OK;
  jni_err = init_common();
  if (jni_err != JNI_OK) return jni_err;

  if (UseJBooster) {
    return ClientDataManager::init_phase1();
  } else if (AsJBooster) {
    return ServerDataManager::init_phase1();
  }
  return JNI_OK;
}

void JBoosterManager::init_phase2(TRAPS) {
  assert(UseJBooster == !AsJBooster, "sanity");

  if (UseJBooster) {
    ClientDataManager::init_phase2(CHECK);
  } else if (AsJBooster) {
    ServerDataManager::init_phase2(CHECK);
  }
}

void JBoosterManager::check_argument(JVMFlagsEnum flag) {
  if (JVMFlag::is_cmdline(flag)) {
    vm_exit_during_initialization(err_msg("Do not set VM option "
    "%s without UseJBooster enabled.", JVMFlag::flag_from_enum(flag)->name()));
  }
}

void JBoosterManager::check_arguments() {
  if (UseJBooster) return;

  check_argument(FLAG_MEMBER_ENUM(JBoosterAddress));
  check_argument(FLAG_MEMBER_ENUM(JBoosterPort));
  check_argument(FLAG_MEMBER_ENUM(JBoosterTimeout));
  check_argument(FLAG_MEMBER_ENUM(JBoosterExitIfUnsupported));
  check_argument(FLAG_MEMBER_ENUM(JBoosterCrashIfNoServer));
  check_argument(FLAG_MEMBER_ENUM(JBoosterProgramName));
  check_argument(FLAG_MEMBER_ENUM(JBoosterCachePath));
  check_argument(FLAG_MEMBER_ENUM(JBoosterLocalMode));
  check_argument(FLAG_MEMBER_ENUM(JBoosterStartupSignal));
  check_argument(FLAG_MEMBER_ENUM(JBoosterStartupMaxTime));
  check_argument(FLAG_MEMBER_ENUM(BoostStopAtLevel));
  check_argument(FLAG_MEMBER_ENUM(UseBoostPackages));
  check_argument(FLAG_MEMBER_ENUM(JBoosterClientStrictMatch));
  check_argument(FLAG_MEMBER_ENUM(PrintAllClassInfo));
  check_argument(FLAG_MEMBER_ENUM(CheckClassFileTimeStamp));
  check_argument(FLAG_MEMBER_ENUM(JBoosterServerSSLRootCerts));
}