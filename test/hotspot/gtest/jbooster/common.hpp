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

#ifndef GTEST_JBOOSTER_COMMON
#define GTEST_JBOOSTER_COMMON

#if INCLUDE_JBOOSTER

// @see src/hotspot/share/memory/filemap.cpp
#ifndef O_BINARY       // if defined (Win32) use binary files.
#define O_BINARY 0     // otherwise do nothing.
#endif

class TestUtils {
public:
  static bool is_same(const char* path1, const char* path2) {
    bool res = false;
    char* buf1 = nullptr;
    char* buf2 = nullptr;
    int fd1 = os::open(path1, O_BINARY | O_RDONLY, 0);
    int fd2 = os::open(path2, O_BINARY | O_RDONLY, 0);
    do {
      if (fd1 < 0 || fd2 < 0) break;
      int64_t size1 = os::lseek(fd1, 0, SEEK_END);
      int64_t size2 = os::lseek(fd2, 0, SEEK_END);
      if (size1 != size2) break;
      int64_t size = size1;
      os::lseek(fd1, 0, SEEK_SET);
      os::lseek(fd2, 0, SEEK_SET);
      // We don't use NEW_RESOURCE_ARRAY here as Thread::current() may
      // not be initialized yet.
      buf1 = NEW_C_HEAP_ARRAY(char, (size_t) size, mtJBooster);
      buf2 = NEW_C_HEAP_ARRAY(char, (size_t) size, mtJBooster);
      size1 = (int64_t) os::read(fd1, buf1, size);
      size2 = (int64_t) os::read(fd2, buf2, size);
      guarantee(size1 == size && size2 == size, "sanity");
      res = memcmp(buf1, buf2, size) == 0;
    } while (false);
    if (fd1 >= 0) os::close(fd1);
    if (fd2 >= 0) os::close(fd2);
    if (buf1 != nullptr) {
      FREE_C_HEAP_ARRAY(char, buf1);
    }
    if (buf2 != nullptr) {
      FREE_C_HEAP_ARRAY(char, buf2);
    }
    return res;
  }

  static bool is_same(const char* path, const char* content, int64_t size) {
    bool res = false;
    char* buf = nullptr;
    int fd = os::open(path, O_BINARY | O_RDONLY, 0);
    do {
      if (fd < 0) break;
      int64_t fsize = os::lseek(fd, 0, SEEK_END);
      if (fsize != size) break;
      os::lseek(fd, 0, SEEK_SET);
      // We don't use NEW_RESOURCE_ARRAY here as Thread::current() may
      // not be initialized yet.
      buf = NEW_C_HEAP_ARRAY(char, (size_t) size, mtJBooster);
      fsize = (int64_t) os::read(fd, buf, size);
      guarantee(fsize == size, "sanity");
      res = memcmp(content, buf, size) == 0;
    } while (false);
    if (fd >= 0) os::close(fd);
    if (buf != nullptr) {
      FREE_C_HEAP_ARRAY(char, buf);
    }
    return res;
  }
};

#endif // INCLUDE_JBOOSTER

#endif // GTEST_JBOOSTER_COMMON