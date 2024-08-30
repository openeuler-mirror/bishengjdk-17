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

#include <dirent.h>

#include "jbooster/utilities/fileUtils.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/os.inline.hpp"

FileUtils::ListDir::ListDir(const char* path): _path(path) {
  _cur = nullptr;
  _ds = os::opendir(path);
}

FileUtils::ListDir::~ListDir() {
  if (_ds != nullptr) os::closedir(_ds);
}

bool FileUtils::ListDir::next() {
  if (_ds == nullptr) return false;
  return (_cur = os::readdir(_ds)) != nullptr;
}

const char* FileUtils::ListDir::name() {
  assert(_cur != nullptr, "sanity");
  return _cur->d_name;
}

bool FileUtils::ListDir::is_file() {
  assert(_cur != nullptr, "sanity");
  return _cur->d_type == DT_REG;
}

bool FileUtils::ListDir::is_dir() {
  assert(_cur != nullptr, "sanity");
  return _cur->d_type == DT_DIR;
}
