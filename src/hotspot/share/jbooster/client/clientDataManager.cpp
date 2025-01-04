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

#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "jbooster/client/clientDaemonThread.hpp"
#include "jbooster/client/clientDataManager.hpp"
#include "jbooster/client/clientStartupSignal.hpp"
#include "jbooster/net/clientStream.hpp"
#include "jbooster/net/sslUtils.hpp"
#include "logging/log.hpp"
#include "runtime/arguments.hpp"
#include "runtime/globals_extension.hpp"
#include "runtime/java.hpp"
#include "runtime/timerTrace.hpp"
#include "utilities/formatBuffer.hpp"

ClientDataManager* ClientDataManager::_singleton = nullptr;

ClientDataManager::ClientDataManager() {
  _random_id = JBoosterManager::calc_random_id();
  _startup_end = false;
  _program_args = nullptr;
  _program_str_id = nullptr;
  _cache_dir_path = nullptr;

  _using_aot = false;
  _using_cds = false;
  _using_clr = false;

  _cache_clr_path = nullptr;
  _cache_cds_path = nullptr;
  _cache_aot_path = nullptr;

  _session_id = 0;
  _program_id = 0;
  _server_random_id = 0;
  _server_available = false;
}

static bool is_option_on(const char* flag_name, const char* options, const char* option) {
  const char* start = strstr(options, option);
  if (start == nullptr) return false;
  if (start == options) return true;
  if (*(start - 1) == '+') return true;
  if (*(start - 1) == '-') return false;
  vm_exit_during_initialization(err_msg("Failed to parse \"%s\" in \"%s\" of %s.", option, options, flag_name));
  return false;
}

void ClientDataManager::init_client_vm_options() {
  // manage boost packages
  if (!FLAG_IS_DEFAULT(BoostStopAtLevel) && !FLAG_IS_DEFAULT(UseBoostPackages)) {
    vm_exit_during_initialization("Either BoostStopAtLevel or UseBoostPackages can be set.");
  }

  if (!FLAG_IS_DEFAULT(UseBoostPackages)) {
    if (strcmp(UseBoostPackages, "all") == 0) {
      _boost_level.set_allow_clr(true);
      _boost_level.set_allow_cds(true);
      _boost_level.set_allow_aot(true);
      _boost_level.set_enable_aot_pgo(true);
    } else {
      _boost_level.set_allow_clr(is_option_on("UseBoostPackages", UseBoostPackages, "clr"));
      _boost_level.set_allow_cds(is_option_on("UseBoostPackages", UseBoostPackages, "cds"));
      _boost_level.set_allow_aot(is_option_on("UseBoostPackages", UseBoostPackages, "aot"));
      _boost_level.set_enable_aot_pgo(is_option_on("UseBoostPackages", UseBoostPackages, "pgo"));
      if (_boost_level.is_aot_pgo_enabled()) _boost_level.set_allow_aot(true);
    }
  } else {
    switch (BoostStopAtLevel) {
    case 4: _boost_level.set_enable_aot_pgo(true);
    case 3: _boost_level.set_allow_aot(true);
    case 2: _boost_level.set_allow_cds(true);
    case 1: _boost_level.set_allow_clr(true);
    case 0: break;
    default: break;
    }
  }

  if (FLAG_IS_DEFAULT(UseAggressiveCDS) || UseAggressiveCDS) {
    _boost_level.set_enable_cds_agg(true);
  }

  if (JBoosterStartupSignal != nullptr) {
    ClientStartupSignal::init_phase1();
  }
}

void ClientDataManager::init_const() {
  _cache_dir_path = JBoosterCachePath;
  _program_args = new JClientArguments(true);
  _program_str_id = JBoosterManager::calc_program_string_id(_program_args->program_name(),
                                                            _program_args->program_entry(),
                                                            _program_args->is_jar(),
                                                            _program_args->hash());
  _cache_clr_path = JBoosterManager::calc_cache_path(_cache_dir_path, _program_str_id, "clr.log");
  const char* cds_path_suffix = _boost_level.is_cds_agg_enabled() ? "cds-agg.jsa" : "cds-dy.jsa";
  _cache_cds_path = JBoosterManager::calc_cache_path(_cache_dir_path, _program_str_id, cds_path_suffix);
  const char* aot_path_suffix = _boost_level.is_aot_pgo_enabled() ? "aot-pgo.so" : "aot-static.so";
  _cache_aot_path = JBoosterManager::calc_cache_path(_cache_dir_path, _program_str_id, aot_path_suffix);
}

void ClientDataManager::init_client_duty() {
  if (JBoosterServerSSLRootCerts) {
    if (!SSLUtils::init_ssl_lib()) {
      vm_exit_during_initialization("Failed to load all functions from OpenSSL Dynamic Library.");
    }
    ClientStream::client_init_ssl_ctx(JBoosterServerSSLRootCerts);
  }
  // Connect to jbooster before initializing CDS, before loading java_lang_classes
  // and before starting the compiler threads.
  ClientStream client_stream(JBoosterAddress, JBoosterPort, JBoosterTimeout, nullptr);
  int rpc_err = client_stream.connect_and_init_session(&_using_clr, &_using_cds, &_using_aot);
  set_server_available(rpc_err == 0);
}

void ClientDataManager::init_client_duty_under_local_mode() {
  set_server_available(false);
  _using_clr = FileUtils::is_file(_cache_clr_path);
  _using_cds = FileUtils::is_file(_cache_cds_path);
  _using_aot = FileUtils::is_file(_cache_aot_path);
}

jint ClientDataManager::init_clr_options() {
  if (!_boost_level.is_clr_allowed()) return JNI_OK;

  if (FLAG_SET_CMDLINE(UseClassLoaderResourceCache, true) != JVMFlag::SUCCESS) {
    return JNI_EINVAL;
  }

  if (is_clr_being_used()) {
    if (FLAG_SET_CMDLINE(LoadClassLoaderResourceCacheFile, cache_clr_path()) != JVMFlag::SUCCESS) {
      return JNI_EINVAL;
    }
  } else if (is_server_available()) {
    if (FLAG_SET_CMDLINE(DumpClassLoaderResourceCacheFile, cache_clr_path()) != JVMFlag::SUCCESS) {
      return JNI_EINVAL;
    }
  }

  return JNI_OK;
}

jint ClientDataManager::init_cds_options() {
  if (!_boost_level.is_cds_allowed()) return JNI_OK;

  if (FLAG_IS_CMDLINE(SharedArchiveFile) || FLAG_IS_CMDLINE(ArchiveClassesAtExit) || FLAG_IS_CMDLINE(SharedArchiveConfigFile)) {
    vm_exit_during_initialization("Do not set CDS path manually whe using JBooster.");
  }

  if (FLAG_IS_CMDLINE(DynamicDumpSharedSpaces) || FLAG_IS_CMDLINE(DumpSharedSpaces) || FLAG_IS_CMDLINE(UseSharedSpaces)) {
    vm_exit_during_initialization("Do not set dump/load CDS manually whe using JBooster.");
  }

  if (is_cds_being_used()) {
    if (FLAG_SET_CMDLINE(SharedArchiveFile, cache_cds_path()) != JVMFlag::SUCCESS) {
      return JNI_EINVAL;
    }

    if (FLAG_SET_CMDLINE(RequireSharedSpaces, JBoosterExitIfUnsupported) != JVMFlag::SUCCESS) {
      return JNI_EINVAL;
    }
  } else if (is_server_available()) {
    // Dump data to the tmp file to prevent other processes from reading the
    // cache file that is not completely written.
    const char* cds_tmp_path = JBoosterManager::calc_tmp_cache_path(cache_cds_path());
    if (FLAG_SET_CMDLINE(ArchiveClassesAtExit, cds_tmp_path) != JVMFlag::SUCCESS) {
      return JNI_EINVAL;
    }
  }

  // It's OK to Use traditional Dynamic CDS if the user manually
  // set UseAggressiveCDS to false.
  if (FLAG_IS_DEFAULT(UseAggressiveCDS)) {
    if (FLAG_SET_CMDLINE(UseAggressiveCDS, true) != JVMFlag::SUCCESS) {
      return JNI_EINVAL;
    }
  }

  if (Arguments::init_agents_at_startup()) {
    if (FLAG_SET_CMDLINE(AllowArchivingWithJavaAgent, true) != JVMFlag::SUCCESS) {
      return JNI_EINVAL;
    }
  }

  return JNI_OK;
}

jint ClientDataManager::init_aot_options() {
  if (!_boost_level.is_aot_allowed()) return JNI_OK;
  if (FLAG_SET_CMDLINE(UseAOT, true) != JVMFlag::SUCCESS) {
    return JNI_EINVAL;
  }
  if (is_aot_being_used()) {
    if (FLAG_SET_CMDLINE(AOTLibrary, cache_aot_path()) != JVMFlag::SUCCESS) {
      return JNI_EINVAL;
    }
  }
  return JNI_OK;
}

jint ClientDataManager::init_pgo_options() {
  if (!_boost_level.is_aot_pgo_enabled()) return JNI_OK;
  if (FLAG_SET_CMDLINE(TypeProfileWidth, 8) != JVMFlag::SUCCESS) {
    return JNI_EINVAL;
  }
  return JNI_OK;
}

void ClientDataManager::print_init_state() {
  log_info(jbooster)("Using boost packages:\n"
                     " - CLR: allowed_to_use=%s,\tis_being_used=%s\n"
                     " - CDS: allowed_to_use=%s,\tis_being_used=%s,\tis_aggressive_cds_enabled=%s\n"
                     " - AOT: allowed_to_use=%s,\tis_being_used=%s,\tis_pgo_aot_enabled=%s\n",
                     BOOL_TO_STR(_boost_level.is_clr_allowed()), BOOL_TO_STR(is_clr_being_used()),
                     BOOL_TO_STR(_boost_level.is_cds_allowed()), BOOL_TO_STR(is_cds_being_used()), BOOL_TO_STR(_boost_level.is_cds_agg_enabled()),
                     BOOL_TO_STR(_boost_level.is_aot_allowed()), BOOL_TO_STR(is_aot_being_used()), BOOL_TO_STR(_boost_level.is_aot_pgo_enabled()));
}

static void check_jbooster_port() {
  if (JBoosterPort == nullptr) {
    vm_exit_during_initialization("JBoosterPort is not set!");
  }

  int len = strlen(JBoosterPort);
  if (len > 1 && JBoosterPort[0] == '0') {
    vm_exit_during_initialization(
      err_msg("JBoosterPort \"%s\" should have no leading zero.", JBoosterPort)
    );
  }
  for (int i = 0; i < len; ++i) {
    if (!isdigit(JBoosterPort[i])) {
      vm_exit_during_initialization(
        err_msg("JBoosterPort \"%s\" should contain only digits.", JBoosterPort)
      );
    }
  }

  julong v;
  if (!Arguments::atojulong(JBoosterPort, &v)) {
    vm_exit_during_initialization(
      err_msg("JBoosterPort \"%s\" is not a unsigned integer.", JBoosterPort)
    );
  }
  uint uint_v = (uint) v;
  if (uint_v < 1024u || uint_v > 65535u) {
    vm_exit_during_initialization(
      err_msg("JBoosterPort \"%s\" is outside the allowed range [1024, 65535].", JBoosterPort)
    );
  }
}

static void check_cache_path() {
  if (JBoosterCachePath == nullptr) {
    FLAG_SET_DEFAULT(JBoosterCachePath, JBoosterManager::calc_cache_dir_path(true));
  }
  FileUtils::mkdirs(JBoosterCachePath);
}

jint ClientDataManager::init_phase1() {
  JBoosterManager::client_only();

  if (Arguments::java_command() == nullptr) {
    if (FLAG_SET_CMDLINE(UseJBooster, false) != JVMFlag::SUCCESS) {
      return JNI_EINVAL;
    }
    return JNI_OK;
  }

   if (JBoosterLocalMode) {
    if (JBoosterPort != nullptr) {
      vm_exit_during_initialization("Either JBoosterLocalMode or JBoosterPort can be set.");
    }

    if (JBoosterStartupSignal != nullptr) {
      log_warning(jbooster)("JBoosterStartupSignal will not be used under local mode.");
      if (FLAG_SET_CMDLINE(JBoosterStartupSignal, nullptr) != JVMFlag::SUCCESS) {
        return JNI_EINVAL;
      }
    }
  } else {
    check_jbooster_port();
  }
  check_cache_path();

  _singleton = new ClientDataManager();
  jint jni_err = JNI_OK;

  TraceTime tt("Init cache", TRACETIME_LOG(Info, jbooster));
  _singleton->init_client_vm_options();
  _singleton->init_const();
  if (!JBoosterLocalMode) {
    _singleton->init_client_duty();
  } else {
    _singleton->init_client_duty_under_local_mode();
  }

  jni_err = _singleton->init_clr_options();
  if (jni_err != JNI_OK) return jni_err;
  jni_err = _singleton->init_cds_options();
  if (jni_err != JNI_OK) return jni_err;
  jni_err = _singleton->init_aot_options();
  if (jni_err != JNI_OK) return jni_err;
  jni_err = _singleton->init_pgo_options();
  if (jni_err != JNI_OK) return jni_err;

  _singleton->print_init_state();
  if (!_singleton->is_server_available()) return escape();
  return JNI_OK;
}

void ClientDataManager::init_phase2(TRAPS) {
  JBoosterManager::client_only();
  if (JBoosterStartupSignal != nullptr) {
    ClientStartupSignal::init_phase2();
  }
  ClientDaemonThread::start_thread(CHECK);

  if (_singleton->_boost_level.is_clr_allowed()) {
    Klass* klass = SystemDictionary::resolve_or_fail(vmSymbols::java_net_ClassLoaderResourceCache(), true, CHECK);
    InstanceKlass::cast(klass)->initialize(CHECK);
  }
}

jint ClientDataManager::escape() {
  if (FLAG_SET_CMDLINE(UseJBooster, false) != JVMFlag::SUCCESS) {
    return JNI_EINVAL;
  }

  if (!JBoosterLocalMode) {
    log_error(jbooster)("Rolled back to the original path (UseJBooster=false), since the server is unavailable.");
  }
  return JNI_OK;
}
