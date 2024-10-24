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

#include "precompiled.hpp"
#include "utilities/macros.hpp"

#if INCLUDE_JBOOSTER

#include "classfile/symbolTable.hpp"
#include "jbooster/utilities/concurrentHashMap.inline.hpp"
#include "jbooster/utilities/debugUtils.inline.hpp"
#include "jbooster/utilities/fileUtils.hpp"
#include "jbooster/utilities/scalarHashMap.inline.hpp"
#include "runtime/os.inline.hpp"
#include "runtime/thread.hpp"
#include "unittest.hpp"
#include "utilities/exceptions.hpp"
#include "utilities/stringUtils.hpp"

class ATestClass {};

template <typename T>
static const char* get_type_name(T t) {
  EXPECT_TRUE(sizeof(t) >= 0);
  return DebugUtils::type_name<T>();
}

static void write_file(const char* file_path, const char* content) {
  int fd = os::open(file_path, O_BINARY | O_WRONLY | O_CREAT, 0666);
  ASSERT_TRUE(os::write(fd, content, strlen(content) + 1));
  os::close(fd);
}

TEST_VM(JBoosterUtil, string) {
  JavaThread* THREAD = JavaThread::current();
  ResourceMark rm(THREAD);
  EXPECT_LT(StringUtils::compare(nullptr, "") , 0);
  EXPECT_GT(StringUtils::compare("", nullptr) , 0);
  EXPECT_GT(StringUtils::compare("2", "1") , 0);
  EXPECT_STREQ(StringUtils::copy_to_resource("123") , "123");
  EXPECT_STREQ(StringUtils::copy_to_resource("1234", 5) , "1234");
  EXPECT_STREQ(StringUtils::copy_to_heap("123", mtJBooster) , "123");
  EXPECT_STREQ(StringUtils::copy_to_heap("1234", 5, mtJBooster) , "1234");
  EXPECT_STREQ(StringUtils::str(SymbolTable::new_symbol("456")), "456");
  EXPECT_STREQ(StringUtils::str(nullptr), "<null>");
}

TEST(JBoosterUtil, debug) {
  int a = 1;
  char b = 2;
  ATestClass c;
  // ignore the memory leak of the returned value
  EXPECT_STREQ("int", get_type_name(a));
  EXPECT_STREQ("char", get_type_name(b));
  EXPECT_STREQ("ATestClass", get_type_name(c));
}

TEST(JBoosterUtil, file) {
  const char prefix[] = "gtest-jbooster-tmp";
  const int prefix_len = sizeof(prefix) - 1;

  FileUtils::mkdir("gtest-jbooster-tmp1");
  FileUtils::mkdir("gtest-jbooster-tmp2");
  FileUtils::mkdir("gtest-jbooster-tmp3");

  EXPECT_TRUE(FileUtils::exists("gtest-jbooster-tmp2"));
  FileUtils::remove("gtest-jbooster-tmp2");
  EXPECT_FALSE(FileUtils::exists("gtest-jbooster-tmp2"));

  EXPECT_TRUE(FileUtils::exists("gtest-jbooster-tmp3"));
  EXPECT_FALSE(FileUtils::exists("gtest-jbooster-tmp4"));
  FileUtils::move("gtest-jbooster-tmp3", "gtest-jbooster-tmp4");
  EXPECT_TRUE(FileUtils::exists("gtest-jbooster-tmp4"));
  EXPECT_FALSE(FileUtils::exists("gtest-jbooster-tmp3"));

  write_file("gtest-jbooster-tmp5", "12345");
  write_file("gtest-jbooster-tmp6", "12345");
  EXPECT_TRUE(FileUtils::is_same("gtest-jbooster-tmp5", "gtest-jbooster-tmp6"));
  write_file("gtest-jbooster-tmp6", "123456");
  EXPECT_FALSE(FileUtils::is_same("gtest-jbooster-tmp5", "gtest-jbooster-tmp6"));

  EXPECT_TRUE(FileUtils::is_same("gtest-jbooster-tmp5", "12345", 6));
  EXPECT_FALSE(FileUtils::is_same("gtest-jbooster-tmp5", "12346", 6));
  EXPECT_FALSE(FileUtils::is_same("gtest-jbooster-tmp5", "123456", 7));

  EXPECT_FALSE(FileUtils::is_file("gtest-jbooster-tmp4"));
  EXPECT_TRUE(FileUtils::is_dir("gtest-jbooster-tmp4"));
  EXPECT_TRUE(FileUtils::is_file("gtest-jbooster-tmp5"));
  EXPECT_FALSE(FileUtils::is_dir("gtest-jbooster-tmp5"));

  FileUtils::ListDir ls(".");
  int file_cnt = 0;
  int dir_cnt = 0;
  while (ls.next()) {
    if (strncmp(ls.name(), prefix, prefix_len) != 0) {
      continue;
    }
    EXPECT_EQ((int) strlen(ls.name()), prefix_len + 1);
    const char last = ls.name()[prefix_len];
    if (ls.is_dir()) {
      EXPECT_TRUE(last == '1' || last == '4');
      ++dir_cnt;
    }
    if (ls.is_file()) {
      EXPECT_TRUE(last == '5' || last == '6');
      ++file_cnt;
    }
  }
  EXPECT_EQ(dir_cnt, 2);
  EXPECT_EQ(file_cnt, 2);
  EXPECT_TRUE(FileUtils::remove("gtest-jbooster-tmp1"));
  EXPECT_TRUE(FileUtils::remove("gtest-jbooster-tmp4"));
  EXPECT_TRUE(FileUtils::remove("gtest-jbooster-tmp5"));
  EXPECT_TRUE(FileUtils::remove("gtest-jbooster-tmp6"));
  EXPECT_FALSE(FileUtils::remove("gtest-jbooster-tmp2"));
  EXPECT_FALSE(FileUtils::remove("gtest-jbooster-tmp7"));

#ifdef LINUX
  EXPECT_EQ(FileUtils::separator_char(), '/');
#else
  ASSERT_TRUE(false);
#endif // LINUX
}

TEST_VM(JBoosterUtil, concurrent_map) {
  JavaThread* THREAD = JavaThread::current();
  ConcurrentHashMap<int, char, mtJBooster> map;
  EXPECT_FALSE(map.contains(1, THREAD));
  EXPECT_EQ(map.get(1, THREAD), nullptr);
  char v2 = '2';
  char v3 = '3';
  EXPECT_EQ(*map.put_if_absent(1, v2, THREAD), '2');
  EXPECT_EQ(*map.put_if_absent(1, v3, THREAD), '2');
  EXPECT_TRUE(map.contains(1, THREAD));
  EXPECT_EQ(*map.get(1, THREAD), '2');
}

TEST(JBoosterUtil, scalar_map) {
  ScalarHashSet<int, mtJBooster> set;
  EXPECT_EQ(set.size(), 0);
  EXPECT_FALSE(set.has(1));
  EXPECT_TRUE(set.add(1));
  EXPECT_FALSE(set.add(1));
  EXPECT_TRUE(set.has(1));
  EXPECT_EQ(set.size(), 1);
  int final_size = 100000;
  int offset = 1000;
  for (int i = 1; i < final_size; ++i) {
    EXPECT_TRUE(set.add(i + offset));
  }
  ASSERT_EQ(set.size(), final_size);
  for (int i = 1; i < final_size; ++i) {
    EXPECT_TRUE(set.has(i + offset));
  }
  EXPECT_FALSE(set.has(final_size + offset));
  set.clear();
  EXPECT_EQ(set.size(), 0);
  EXPECT_FALSE(set.has(1));
  EXPECT_FALSE(set.has(final_size / 2 + offset));
}

#endif // INCLUDE_JBOOSTER
