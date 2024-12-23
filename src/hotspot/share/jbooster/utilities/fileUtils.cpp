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

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "jbooster/net/messageBuffer.inline.hpp"
#include "jbooster/net/serializationWrappers.hpp"
#include "jbooster/utilities/fileUtils.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/os.inline.hpp"

static const char* get_home_path() {
  const char* res = ::getenv("HOME");
  if ((res == nullptr) || (*res == '\0')) {
    struct passwd* pwd = getpwuid(geteuid());
    if (pwd != nullptr) res = pwd->pw_dir;
  }
  guarantee(res != nullptr && *res != '\0', "failed to get home");
  return res;
}

const char* FileUtils::separator() {
  return os::file_separator();
}

char FileUtils::separator_char() {
  assert(strlen(separator()) == 1, "sanity");
  return separator()[0];
}

const char* FileUtils::home_path() {
  static const char* home_dir = get_home_path();
  return home_dir;
}

bool FileUtils::exists(const char* path) {
  struct stat st = {0};
  return os::stat(path, &st) == 0;
}

bool FileUtils::is_file(const char* path) {
  struct stat st = {0};
  if (os::stat(path, &st) != 0) return false;
  return S_ISREG(st.st_mode);
}

bool FileUtils::is_dir(const char* path) {
  struct stat st = {0};
  if (os::stat(path, &st) != 0) return false;
  return S_ISDIR(st.st_mode);
}

int64_t FileUtils::file_size(const char* path) {
  struct stat st = {0};
  if (os::stat(path, &st) != 0) return -1;
  // We don't care if it is a regular file.
  return st.st_size;
}

uint64_t FileUtils::modify_time(const char* path) {
  struct stat st = {0};
  if (os::stat(path, &st) != 0) return 0;
  return st.st_mtime;
}

bool FileUtils::mkdir(const char* path) {
  if (::mkdir(path, 0777) == OS_ERR) {
    if (errno == EEXIST) return false;
    else fatal("Cannot mkdir: %s. Err: %s", path, os::strerror(errno));
  }
  return true;
}

bool FileUtils::mkdirs(const char* path) {
  int len = strlen(path) + 1;
  // We don't use NEW_RESOURCE_ARRAY here as Thread::current() may
  // not be initialized yet.
  char* each_path = NEW_C_HEAP_ARRAY(char, len, mtJBooster);
  memcpy((void*)each_path, path, len);

  const char* sp = separator();
  int sp_len = strlen(sp);
  for (char* p = each_path + 1, *end = each_path + len; p < end; ++p) {
    if (strncmp(p, sp, sp_len) != 0) continue;
    *p = '\0';
    if (!exists(each_path)) mkdir(each_path);
    *p = *sp;
  }
  FREE_C_HEAP_ARRAY(char, each_path);
  return mkdir(path);
}

bool FileUtils::rename(const char* path_from, const char* path_to) {
  return ::rename(path_from, path_to) == 0;
}

bool FileUtils::move(const char* path_from, const char* path_to) {
  return rename(path_from, path_to);
}

bool FileUtils::remove(const char* path) {
  return ::remove(path) == 0;
}
