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

#ifndef SHARE_JBOOSTER_UTILITIES_FILEUTILS_HPP
#define SHARE_JBOOSTER_UTILITIES_FILEUTILS_HPP

#ifdef LINUX
#include <dirent.h>
#endif // LINUX

#include "memory/allocation.hpp"
#include "utilities/globalDefinitions.hpp"

// @see src/hotspot/share/memory/filemap.cpp
#ifndef O_BINARY       // if defined (Win32) use binary files.
#define O_BINARY 0     // otherwise do nothing.
#endif

class FileUtils: AllStatic {
public:
  static const char* separator();
  static char separator_char();
  static const char* home_path();
  static bool exists(const char* path);
  static bool is_file(const char* path);
  static bool is_dir(const char* path);
  static uint64_t modify_time(const char* path);
  static bool mkdir(const char* path);
  static bool mkdirs(const char* path);
  static bool rename(const char* path_from, const char* path_to);
  static bool move(const char* path_from, const char* path_to);
  static bool remove(const char* path);
  static bool is_same(const char* path1, const char* path2);
  static bool is_same(const char* path, const char* content, int64_t size);

  class ListDir: public StackObj {
    const char* _path;

#ifdef LINUX
    DIR* _ds;
    dirent* _cur;
#endif // LINUX

  public:
    ListDir(const char* path);
    ~ListDir();

    bool next();

    const char* name();
    bool is_file();
    bool is_dir();
  };
};

#endif // SHARE_JBOOSTER_UTILITIES_FILEUTILS_HPP
