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
#include "jbooster/jClientArguments.hpp"
#include "jbooster/net/communicationStream.hpp"
#include "jbooster/net/messageBuffer.inline.hpp"
#include "jbooster/net/serializationWrappers.hpp"
#include "jbooster/utilities/fileUtils.hpp"
#include "logging/log.hpp"
#include "logging/logStream.hpp"
#include "runtime/abstract_vm_version.hpp"
#include "runtime/arguments.hpp"
#include "utilities/stringUtils.hpp"

static JClientArguments::CpuArch calc_cpu() {
#ifdef X86
  return JClientArguments::CpuArch::CPU_X86;
#endif
#ifdef AARCH64
  return JClientArguments::CpuArch::CPU_AARCH64;
#endif
#ifdef ARM
  return JClientArguments::CpuArch::CPU_ARM;
#endif
  return JClientArguments::CpuArch::CPU_UNKOWN;
}

static uint32_t calc_new_hash(uint32_t old_hash, uint32_t ele_hash) {
  return 31 * old_hash + ele_hash;
}

/**
 * Returns the jar file name as the program name.
 * > This function is only for the client.
 * > A new C-heap string will be returned.
 */
const char* calc_program_entry_by_jar(const char* app_cp, int app_cp_len) {
  assert(strlen(FileUtils::separator()) == 1, "sanity");
  const char* file_start = strrchr(app_cp, FileUtils::separator()[0]);
  if (file_start == nullptr) file_start = app_cp;
  else ++file_start;
  int len = app_cp + app_cp_len - file_start;
  if (strncmp(file_start + len - 4, ".jar", 4) == 0) len -= 4;
  char* res = NEW_C_HEAP_ARRAY(char, len + 1, mtJBooster);
  memcpy(res, file_start, len);
  res[len] = '\0';
  return res;
}

/**
 * Return the main class full name.
 * > This function is only for the client.
 * > A new C-heap string will be returned.
 */
const char* calc_program_entry_by_class(const char* full_cmd, int full_cmd_len) {
  int len = full_cmd_len;
  const char* clazz_end = strchr(full_cmd, ' ');
  if (clazz_end != nullptr) len = clazz_end - full_cmd;
  char* res = NEW_C_HEAP_ARRAY(char, len + 1, mtJBooster);
  memcpy(res, full_cmd, len);
  res[len] = '\0';
  return res;
}

/**
 * Returns the hash of the classpath names.
 * > This function is only for the client.
 */
static uint32_t calc_classpath_name_hash(const char *app_cp, int app_cp_strlen) {
  return StringUtils::hash_code(app_cp, app_cp_strlen + 1);
}

/**
 * Returns the hash of the classpath timestamps.
 * > This function is only for the client.
 */
static uint32_t calc_classpath_timestamp_hash(const char *app_cp, int app_cp_strlen) {
  const int path_len = strlen(os::path_separator());
  uint32_t h = 0;
  char* tmp = StringUtils::copy_to_heap(app_cp, app_cp_strlen + 1, mtJBooster);
  char* p = tmp;
  while (p != nullptr) {
    char* nxt = strstr(p, os::path_separator());
    if (nxt != nullptr) {
      *nxt = '\0';
      nxt += path_len;
    }
    if (nxt - p != path_len) { // not empty path
      h = calc_new_hash(h, primitive_hash(FileUtils::modify_time(p)));
    }
    p = nxt;
  }
  StringUtils::free_from_heap(tmp);
  return h;
}

static uint32_t calc_agent_name_hash() {
  uint32_t h = 0;
  if (Arguments::init_agents_at_startup()) {
    for (AgentLibrary* agent = Arguments::agents(); agent != nullptr; agent = agent->next()) {
      h = calc_new_hash(h, StringUtils::hash_code(agent->name()));
    }
  }
  return h;
}

/**
 * Returns the java commands in the cmd line. Different from
 * Arguments::java_command(), the return value excludes the file path.
 * > This function is only for the client.
 * > A new C-heap string will be returned if it is not null.
 */
static const char* calc_java_commands_by_jar(const char* full_cmd, int app_cp_len) {
  if (full_cmd[app_cp_len] == '\0') return nullptr;
  return StringUtils::copy_to_heap(full_cmd + app_cp_len + 1, mtJBooster);
}

/**
 * Return the java commands in the cmd line. Different from
 * Arguments::java_command(), the return value excludes the class name.
 * > This function is only for the client.
 * > A new C-heap string will be returned if it is not null.
 */
const char* calc_java_commands_by_class(const char* full_cmd, int full_cmd_len) {
  const char* start = strchr(full_cmd, ' ');
  if (start == nullptr) start = full_cmd + full_cmd_len;
  else ++start;
  return StringUtils::copy_to_heap(start, mtJBooster);
}

JClientArguments::JClientArguments(bool is_client) {
  if (is_client) {
    init_for_client();
    _state = INITIALIZED_FOR_CLIENT;
  } else {
    _state = NOT_INITIALIZED_FOR_SEREVR;
  }
}

JClientArguments::~JClientArguments() {
  if (_state == OBJECT_MOVED) return;  // let the new object to free the members

  if (_state == INITIALIZED_FOR_CLIENT) {
    // [FOR EACH ARG]
    // _internal_jvm_info is shared. Do not free it.
    // _program_name is shared. Do not free it.
    // _program_entry is shared. Do not free it.
    // _java_commands is shared. Do not free it.
    delete _related_flags;
  } else if (_state == INITIALIZED_FOR_SEREVR) {
    // [FOR EACH ARG]
    StringUtils::free_from_heap(_internal_jvm_info);
    StringUtils::free_from_heap(_program_name);
    StringUtils::free_from_heap(_program_entry);
    StringUtils::free_from_heap(_java_commands);
    delete _related_flags;
  } else {
    guarantee(_state == NOT_INITIALIZED_FOR_SEREVR, "sanity");
    // All the members are not initialized (may be random value).
  }
}

void JClientArguments::init_for_client() {
  const char* full_cmd = Arguments::java_command();
  const char* app_cp = Arguments::get_appclasspath();
  int full_cmd_len = strlen(full_cmd);
  int app_cp_len = strlen(app_cp);

  // The classpath is ignored when using "-jar" and in this case the
  // app classpath is the jar file path.
  bool is_jar = strncmp(full_cmd, app_cp, app_cp_len) == 0;

  // [FOR EACH ARG]
  _cpu_arch = calc_cpu();
  _jvm_version = Abstract_VM_Version::jvm_version();
  _internal_jvm_info = Abstract_VM_Version::internal_vm_info_string();
  _program_name = StringUtils::copy_to_heap(JBoosterProgramName, mtJBooster);
  _program_entry = is_jar ? calc_program_entry_by_jar(app_cp, app_cp_len)
                          : calc_program_entry_by_class(full_cmd, full_cmd_len);
  _is_jar = is_jar;
  _classpath_name_hash = calc_classpath_name_hash(app_cp, app_cp_len);
  _classpath_timestamp_hash = calc_classpath_timestamp_hash(app_cp, app_cp_len);
  _agent_name_hash = calc_agent_name_hash();
  if (JBoosterClientStrictMatch) {
    _java_commands = is_jar ? calc_java_commands_by_jar(full_cmd, app_cp_len)
                            : calc_java_commands_by_class(full_cmd, full_cmd_len);
  } else {
    _java_commands = StringUtils::copy_to_heap("<ignored>", mtJBooster);
  }
  _jbooster_allow_clr = ClientDataManager::get().is_clr_allowed();
  _jbooster_allow_cds = ClientDataManager::get().is_cds_allowed();
  _jbooster_allow_aot = ClientDataManager::get().is_aot_allowed();
  _jbooster_allow_pgo = ClientDataManager::get().is_pgo_allowed();
  _related_flags = new JClientVMFlags(true);

  _hash = calc_hash();
}

uint32_t JClientArguments::calc_hash() {
  uint32_t result = 1;

  // [FOR EACH ARG]
  result = calc_new_hash(result, primitive_hash(_cpu_arch));
  result = calc_new_hash(result, primitive_hash(_jvm_version));
  result = calc_new_hash(result, StringUtils::hash_code(_internal_jvm_info));
  result = calc_new_hash(result, StringUtils::hash_code(_program_name));
  result = calc_new_hash(result, StringUtils::hash_code(_program_entry));
  result = calc_new_hash(result, _is_jar);
  result = calc_new_hash(result, _classpath_name_hash);
  result = calc_new_hash(result, _classpath_timestamp_hash);
  result = calc_new_hash(result, _agent_name_hash);
  result = calc_new_hash(result, StringUtils::hash_code(_java_commands));
  result = calc_new_hash(result, primitive_hash(_jbooster_allow_clr));
  result = calc_new_hash(result, primitive_hash(_jbooster_allow_cds));
  result = calc_new_hash(result, primitive_hash(_jbooster_allow_aot));
  result = calc_new_hash(result, primitive_hash(_jbooster_allow_pgo));
  result = calc_new_hash(result, _related_flags->hash(_jbooster_allow_clr, _jbooster_allow_cds, _jbooster_allow_aot));

  return result;
}

bool JClientArguments::equals(const JClientArguments* that) const {
  assert(that != nullptr, "sanity");

  // [FOR EACH ARG]
  if (this->_cpu_arch                 != that->_cpu_arch          ) return false;
  if (this->_jvm_version              != that->_jvm_version       ) return false;
  if (StringUtils::compare(this->_internal_jvm_info, that->_internal_jvm_info) != 0) return false;
  if (StringUtils::compare(this->_program_name, that->_program_name) != 0) return false;
  if (StringUtils::compare(this->_program_entry, that->_program_entry) != 0) return false;
  if (this->_is_jar                   != that->_is_jar) return false;
  if (this->_classpath_name_hash      != that->_classpath_name_hash) return false;
  if (this->_classpath_timestamp_hash != that->_classpath_timestamp_hash) return false;
  if (this->_agent_name_hash          != that->_agent_name_hash) return false;
  if (StringUtils::compare(this->_java_commands, that->_java_commands) != 0) return false;
  if (this->_jbooster_allow_clr       != that->_jbooster_allow_clr) return false;
  if (this->_jbooster_allow_cds       != that->_jbooster_allow_cds) return false;
  if (this->_jbooster_allow_aot       != that->_jbooster_allow_aot) return false;
  if (this->_jbooster_allow_pgo       != that->_jbooster_allow_pgo) return false;
  if (!this->_related_flags->equals(that->_related_flags,
                                    _jbooster_allow_clr,
                                    _jbooster_allow_cds,
                                    _jbooster_allow_aot)) return false;
  return true;
}

void JClientArguments::print_args(outputStream* st) const {
  // [FOR EACH ARG]
  st->print_cr("  hash:                     %x",      _hash);
  st->print_cr("  cpu_arch:                 %s",      cpu_arch_str(_cpu_arch));
  st->print_cr("  jvm_version:              %u",      _jvm_version);
  st->print_cr("  jvm_info:                 \"%s\"",  _internal_jvm_info);
  st->print_cr("  program_name:             %s",      _program_name);
  st->print_cr("  program_entry:            %s",      _program_entry);
  st->print_cr("  is_jar:                   %s",      BOOL_TO_STR(_is_jar));
  st->print_cr("  classpath_name_hash:      %x",      _classpath_name_hash);
  st->print_cr("  classpath_timestamp_hash: %x",      _classpath_timestamp_hash);
  st->print_cr("  agent_name_hash:          %x",      _agent_name_hash);
  st->print_cr("  java_commands:            \"%s\"",  _java_commands);
  st->print_cr("  allow_clr:                %s",      BOOL_TO_STR(_jbooster_allow_clr));
  st->print_cr("  allow_cds:                %s",      BOOL_TO_STR(_jbooster_allow_cds));
  st->print_cr("  allow_aot:                %s",      BOOL_TO_STR(_jbooster_allow_aot));
  st->print_cr("  allow_pgo:                %s",      BOOL_TO_STR(_jbooster_allow_pgo));
  st->print_cr("  vm_flags:");
  st->print_cr("    hash: %u", _related_flags->hash(_jbooster_allow_clr,
                                                     _jbooster_allow_cds,
                                                     _jbooster_allow_aot));
  _related_flags->print_flags(st);
}

JClientArguments* JClientArguments::move() {
  JClientArguments* new_obj = new JClientArguments(*this);  // shallow copy!
  _state = OBJECT_MOVED;
  return new_obj;
}

int JClientArguments::serialize(MessageBuffer& buf) const {
  // [FOR EACH ARG]
  JB_RETURN(buf.serialize_no_meta(_cpu_arch));
  JB_RETURN(buf.serialize_no_meta(_jvm_version));
  JB_RETURN(buf.serialize_with_meta(&_internal_jvm_info));
  JB_RETURN(buf.serialize_with_meta(&_program_name));
  JB_RETURN(buf.serialize_with_meta(&_program_entry));
  JB_RETURN(buf.serialize_no_meta(_is_jar));
  JB_RETURN(buf.serialize_no_meta(_classpath_name_hash));
  JB_RETURN(buf.serialize_no_meta(_classpath_timestamp_hash));
  JB_RETURN(buf.serialize_no_meta(_agent_name_hash));
  JB_RETURN(buf.serialize_with_meta(&_java_commands));
  JB_RETURN(buf.serialize_no_meta(_jbooster_allow_clr));
  JB_RETURN(buf.serialize_no_meta(_jbooster_allow_cds));
  JB_RETURN(buf.serialize_no_meta(_jbooster_allow_aot));
  JB_RETURN(buf.serialize_no_meta(_jbooster_allow_pgo));
  JB_RETURN(buf.serialize_with_meta(_related_flags));

  JB_RETURN(buf.serialize_no_meta(_hash));

  return 0;
}

int JClientArguments::deserialize(MessageBuffer& buf) {
  guarantee(_state == NOT_INITIALIZED_FOR_SEREVR, "init twice?");

  // [FOR EACH ARG]

  JB_RETURN(buf.deserialize_ref_no_meta(_cpu_arch));
  JB_RETURN(buf.deserialize_ref_no_meta(_jvm_version));

  StringWrapper sw_internal_jvm_info;
  JB_RETURN(buf.deserialize_with_meta(&sw_internal_jvm_info));
  _internal_jvm_info = sw_internal_jvm_info.export_string();

  StringWrapper sw_program_name;
  JB_RETURN(buf.deserialize_with_meta(&sw_program_name));
  _program_name = sw_program_name.export_string();

  StringWrapper sw_program_entry;
  JB_RETURN(buf.deserialize_with_meta(&sw_program_entry));
  _program_entry = sw_program_entry.export_string();

  JB_RETURN(buf.deserialize_ref_no_meta(_is_jar));

  JB_RETURN(buf.deserialize_ref_no_meta(_classpath_name_hash));

  JB_RETURN(buf.deserialize_ref_no_meta(_classpath_timestamp_hash));

  JB_RETURN(buf.deserialize_ref_no_meta(_agent_name_hash));

  StringWrapper sw_java_commands;
  JB_RETURN(buf.deserialize_with_meta(&sw_java_commands));
  _java_commands = sw_java_commands.export_string();

  JB_RETURN(buf.deserialize_ref_no_meta(_jbooster_allow_clr));
  JB_RETURN(buf.deserialize_ref_no_meta(_jbooster_allow_cds));
  JB_RETURN(buf.deserialize_ref_no_meta(_jbooster_allow_aot));
  JB_RETURN(buf.deserialize_ref_no_meta(_jbooster_allow_pgo));

  _related_flags = new JClientVMFlags(false);
  JB_RETURN(buf.deserialize_with_meta(_related_flags));

  // end of for each arg

  JB_RETURN(buf.deserialize_ref_no_meta(_hash));
  guarantee(_hash == calc_hash(), "sanity");
  _state = INITIALIZED_FOR_SEREVR;
  LogTarget(Info, jbooster) lt;
  if (lt.is_enabled()) {
    ResourceMark rm;
    LogStream ls(lt);
    ls.print_cr("Received JClientArguments, stream_id=%u:", buf.stream()->stream_id());
    print_args(&ls);
  }
  return 0;
}

/**
 * The arg "reason" will only be set when the return value of this function is false.
 * The value of "reason" should be literal string so you don't have to free it.
 */
bool JClientArguments::check_compatibility_with_server(const char** reason) {
  const char* different_item = nullptr;
  if (_cpu_arch != calc_cpu()) {
    different_item = "CPU arch";
  } else if (_jvm_version != Abstract_VM_Version::jvm_version()) {
    different_item = "JVM version";
  } else if (StringUtils::compare(_internal_jvm_info, Abstract_VM_Version::internal_vm_info_string()) != 0) {
    different_item = "JVM info";
  } else if (_related_flags->get_UseG1GC() != true) {
    different_item = "G1 GC";
  } else {
    return true;
  }

  assert(different_item != nullptr, "sanity");
  *reason = different_item;
  return false;
}

const char* JClientArguments::cpu_arch_str(CpuArch cpu_arch) {
  switch (cpu_arch) {
    case CpuArch::CPU_UNKOWN:  return "unknown";
    case CpuArch::CPU_X86:     return "x86";
    case CpuArch::CPU_ARM:     return "arm";
    case CpuArch::CPU_AARCH64: return "aarch64";
    default: break;
  }
  return "error";
}
