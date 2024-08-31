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

#include "jbooster/net/message.inline.hpp"
#include "jbooster/net/messageBuffer.inline.hpp"
#include "jbooster/net/serializationWrappers.inline.hpp"
#include "jbooster/utilities/fileUtils.hpp"
#include "runtime/os.inline.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/growableArray.hpp"
#include "unittest.hpp"

static int try_catch_func(int i) {
  JB_RETURN(i);
  EXPECT_EQ(i, 0);
  return 0;
}

class CustomData1: public StackObj {
public:
  enum {
    V0, V1, V2
  };
  enum {
    M0, M1, M2, M3, M4, M5, M6
  };

private:
  int _test_mode;
  int _v1, _v2;

public:
  CustomData1(int test_mode, bool is_serialize): _test_mode(test_mode),
                                                 _v1(is_serialize ? V1 : V0),
                                                 _v2(is_serialize ? V2 : V0) {}

  int v1() { return _v1; }
  int v2() { return _v2; }

  int serialize(MessageBuffer& buf) const {
    buf.serialize_no_meta(_v1);
    buf.serialize_no_meta(_v2);
    return 0;
  }

  int deserialize(MessageBuffer& buf) {
    buf.deserialize_ref_no_meta(_v1);
    if (_test_mode == M0) {
      return 0;
    } else if (_test_mode == M1) {
      return JBErr::DESER_TERMINATION;
    }
    buf.deserialize_ref_no_meta(_v2);
    if (_test_mode == M2) {
      return 0;
    } else if (_test_mode == M3) {
      return JBErr::DESER_TERMINATION;
    } else {
      int v3;
      buf.deserialize_ref_no_meta(v3);
      if (_test_mode == M4) {
        return 0;
      } else if (_test_mode == M5) {
        return JBErr::DESER_TERMINATION;
      }
    }
    return JBErr::BAD_ARG_DATA;
  }
};

DECLARE_SERIALIZER_INTRUSIVE(CustomData1);

static void init_recv_message(Message& recv, Message& send) {
  recv.expand_buf_if_needed(send.cur_buf_offset(), 0);
  memcpy(recv.buf_beginning(), send.buf_beginning(), send.cur_buf_offset());
}

static void copy_message_buffer(MessageBuffer& to, MessageBuffer& from) {
  to.expand_if_needed(from.cur_offset(), 0);
  memcpy(to.buf(), from.buf(), from.cur_offset());
}

TEST(JBoosterNet, try_catch) {
  int i;
  for (i = 0; i < 9; ++i) {
    JB_TRY_BREAKABLE {
      if (i == 5 || i == 6) continue;
      JB_THROW(try_catch_func(i));
      EXPECT_EQ(i, 0);
    } JB_TRY_BREAKABLE_END
    JB_CATCH(3, 5, 7) {
      EXPECT_TRUE(JB_ERR == 3 || JB_ERR == 7);
    }
    JB_CATCH(2, 4, 6) {
      EXPECT_TRUE(JB_ERR == 2 || JB_ERR == 4);
    }
    JB_CATCH_REST() {
      EXPECT_TRUE(JB_ERR == 1 || JB_ERR == 8);
    } JB_CATCH_END;
  }
  EXPECT_EQ(i, 9);

  for (i = 0; i < 3; ++i) {
    JB_TRY {
      JB_THROW(try_catch_func(i));
      EXPECT_EQ(i, 0);
    } JB_TRY_END
    JB_CATCH_REST() {
      EXPECT_TRUE(JB_ERR == 1 || JB_ERR == 2);
    } JB_CATCH_END;
  }
  EXPECT_EQ(i, 3);
}

TEST(JBoosterNet, serializationn_basics) {
  char cache[1024];
  uint32_t cache_size;
  { MessageBuffer buf(SerializationMode::SERIALIZE);
    char    c1 = '6';
    int     i1 = 1234;
    int64_t l1 = 900000000000000ll;
    EXPECT_EQ(buf.serialize_no_meta(c1), 0);
    EXPECT_EQ(buf.cur_offset(), 1u);
    EXPECT_EQ(buf.serialize_no_meta(i1), 0);
    EXPECT_EQ(buf.cur_offset(), 5u);
    EXPECT_EQ(buf.serialize_no_meta(l1), 0);
    EXPECT_EQ(buf.cur_offset(), 13u);

    uint32_t u1 = 2468;
    const char* s1 = nullptr;
    const char* s2 = "hello";
    const char* s3 = "world!";
    EXPECT_EQ(buf.serialize_with_meta(&u1), 0);
    EXPECT_EQ(buf.cur_offset(), 21u);
    EXPECT_EQ(buf.serialize_with_meta(&s1), 0);
    EXPECT_EQ(buf.cur_offset(), 29u);
    EXPECT_EQ(buf.serialize_with_meta(&s2), 0);
    EXPECT_EQ(buf.cur_offset(), 42u);
    EXPECT_EQ(buf.serialize_with_meta(&s3), 0);
    EXPECT_EQ(buf.cur_offset(), 56u);

    cache_size = buf.cur_offset();
    memcpy(cache, buf.buf(), cache_size);
  }

  { MessageBuffer buf(SerializationMode::DESERIALIZE);
    buf.expand_if_needed(cache_size, 0);
    memcpy(buf.buf(), cache, cache_size);
    char    c1;
    int     i1;
    int64_t l1;
    EXPECT_EQ(buf.deserialize_ref_no_meta(c1), 0);
    EXPECT_EQ(buf.cur_offset(), 1u);
    EXPECT_EQ(buf.deserialize_ref_no_meta(i1), 0);
    EXPECT_EQ(buf.cur_offset(), 5u);
    EXPECT_EQ(buf.deserialize_ref_no_meta(l1), 0);
    EXPECT_EQ(buf.cur_offset(), 13u);
    EXPECT_EQ(c1, '6');
    EXPECT_EQ(i1, 1234);
    EXPECT_EQ(l1, 900000000000000ll);

    uint32_t u1;
    const char* s1 = nullptr;
    char s2[6];
    StringWrapper s3;
    EXPECT_EQ(buf.deserialize_with_meta(&u1), 0);
    EXPECT_EQ(buf.cur_offset(), 21u);
    EXPECT_EQ(buf.deserialize_with_meta(&s1), 0);
    EXPECT_EQ(buf.cur_offset(), 29u);
    EXPECT_EQ(buf.deserialize_with_meta(&s2), 0);
    EXPECT_EQ(buf.cur_offset(), 42u);
    EXPECT_EQ(buf.deserialize_with_meta(&s3), 0);
    EXPECT_EQ(buf.cur_offset(), 56u);
    EXPECT_EQ(u1, 2468u);
    EXPECT_STREQ(s1, nullptr);
    EXPECT_STREQ(s2, "hello");
    EXPECT_STREQ(s3.get_string(), "world!");
    EXPECT_EQ(s3.size(), 6u);
  }
}

TEST_VM(JBoosterNet, serializationn_string) {
  ResourceMark rm;
  MessageBuffer buf0(SerializationMode::SERIALIZE);
  const char* ss1 = "123";
  const char* ss2 = nullptr;
  char ss3[] = { '4', '5', '6', '\n' };
  EXPECT_EQ(buf0.serialize_with_meta(&ss1), 0);
  EXPECT_EQ(buf0.serialize_with_meta(&ss2), 0);
  EXPECT_EQ(buf0.serialize_with_meta(&ss3), 0);

  { MessageBuffer buf(SerializationMode::DESERIALIZE);
    copy_message_buffer(buf, buf0);
    char* s1 = nullptr;
    char* s2 = nullptr;
    const char* s3 = nullptr;
    EXPECT_EQ(buf.deserialize_with_meta(&s1), 0);
    EXPECT_EQ(buf.deserialize_with_meta(&s2), 0);
    EXPECT_EQ(buf.deserialize_with_meta(&s3), 0);
    EXPECT_STREQ(ss1, s1);
    EXPECT_EQ(ss2, s2);
    EXPECT_STREQ(ss3, s3);
  }

  { MessageBuffer buf(SerializationMode::DESERIALIZE);
    copy_message_buffer(buf, buf0);
    char s1[1];
    ASSERT_DEATH(buf.deserialize_with_meta(&s1), "");
  }

  { MessageBuffer buf(SerializationMode::DESERIALIZE);
    copy_message_buffer(buf, buf0);
    char s1[64];
    EXPECT_EQ(buf.deserialize_with_meta(&s1), 0);
    EXPECT_STREQ(ss1, s1);
    ASSERT_DEATH(buf.deserialize_with_meta(&s1), "");
  }

  { MessageBuffer buf(SerializationMode::DESERIALIZE);
    copy_message_buffer(buf, buf0);
    StringWrapper s1, s2, s3;
    EXPECT_EQ(buf.deserialize_with_meta(&s1), 0);
    EXPECT_EQ(buf.deserialize_with_meta(&s2), 0);
    EXPECT_EQ(buf.deserialize_with_meta(&s3), 0);
    EXPECT_STREQ(ss1, s1.get_string());
    EXPECT_EQ(ss2, s2.export_string());
    EXPECT_STREQ(ss3, s3.export_string());
  }

  buf0.reset_cur_offset();
  StringWrapper ss4(nullptr);
  StringWrapper ss5("");
  EXPECT_EQ(buf0.serialize_with_meta(&ss4), 0);
  EXPECT_EQ(buf0.serialize_with_meta(&ss5), 0);

  { MessageBuffer buf(SerializationMode::DESERIALIZE);
    copy_message_buffer(buf, buf0);
    const char* s1 = nullptr;
    char* s2 = nullptr;
    EXPECT_EQ(buf.deserialize_with_meta(&s1), 0);
    EXPECT_EQ(buf.deserialize_with_meta(&s2), 0);
    EXPECT_EQ(nullptr, s1);
    EXPECT_STREQ("", s2);
  }
}

TEST(JBoosterNet, serializationn_crash) {
  int err;
  MessageBuffer buf(SerializationMode::BOTH);
  const char* s = "hello";
  EXPECT_EQ(buf.serialize_with_meta(&s), 0);

  char s1[6];
  buf.reset_cur_offset();
  EXPECT_EQ(buf.deserialize_with_meta(&s1), 0);
  ASSERT_STREQ(s1, "hello");

  char s2[5];
  buf.reset_cur_offset();
  bool old = SuppressFatalErrorMessage;
  SuppressFatalErrorMessage = true;
  ASSERT_DEATH(buf.deserialize_with_meta(&s2), "");
  SuppressFatalErrorMessage = old;
}

TEST(JBoosterNet, serializationn_wrappers) {
  MessageBuffer buf(SerializationMode::BOTH);
  uint32_t mem_size = 16u * 1024;
  {
    StringWrapper s1("1");
    StringWrapper s2("22");
    StringWrapper s3("333");
    StringWrapper s4("4444");
    GrowableArray<StringWrapper*> ga(4, mtJBooster);
    ga.append(&s1);
    ga.append(&s2);
    ga.append(&s3);
    ga.append(&s4);
    ArrayWrapper<StringWrapper> aw(&ga);
    EXPECT_EQ(buf.serialize_with_meta(&aw), 0);
    EXPECT_EQ(buf.cur_offset(), 0u + (4 + 4) + (1 + 2 + 3 + 4 + 4 * (4 + 4)));

    char* mem = NEW_C_HEAP_ARRAY(char, mem_size, mtJBooster);
    memset(mem, 0x68, mem_size);
    MemoryWrapper mw(mem, mem_size);
    EXPECT_EQ(buf.serialize_with_meta(&mw), 0);
    FREE_C_HEAP_ARRAY(char, mem);
  }

  buf.reset_cur_offset();

  {
    ArrayWrapper<StringWrapper> aw(false);
    EXPECT_EQ(buf.deserialize_with_meta(&aw), 0);
    EXPECT_STREQ(aw.get(0)->get_string(), "1");
    EXPECT_STREQ(aw.get(1)->get_string(), "22");
    EXPECT_STREQ(aw.get(2)->get_string(), "333");
    EXPECT_STREQ(aw.get(3)->get_string(), "4444");
    StringWrapper* sws = aw.get_array<StringWrapper>();
    EXPECT_STREQ(sws[0].get_string(), "1");
    EXPECT_STREQ(sws[1].get_string(), "22");
    EXPECT_STREQ(sws[2].get_string(), "333");
    EXPECT_STREQ(sws[3].get_string(), "4444");

    MemoryWrapper mw;
    EXPECT_EQ(buf.deserialize_with_meta(&mw), 0);
    EXPECT_EQ(mw.size(), mem_size);
    const int* mem = (const int*) mw.get_memory();
    int size = (int) mw.size() / 4;
    for (int i = 0; i < size; ++i) {
      ASSERT_EQ(mem[i], 0x68686868);
    }
  }
}

static void create_test_file_for_file_wrapper(const char* file_name) {
  uint32_t mem_size = FileWrapper::MAX_SIZE_PER_TRANS * 2
                    + FileWrapper::MAX_SIZE_PER_TRANS / 2;
  char* mem = NEW_C_HEAP_ARRAY(char, mem_size, mtJBooster);
  memset(mem, 0xc4, mem_size);

  errno = 0;
  int fd = os::open(file_name, O_BINARY | O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, 0666);
  ASSERT_TRUE(fd >= 0);
  ASSERT_EQ(errno, 0);
  uint32_t left = mem_size;
  do {
    uint32_t write_size = (uint32_t) os::write(fd, mem + mem_size - left, left);
    left -= write_size;
  } while (left > 0);
  os::close(fd);
  FREE_C_HEAP_ARRAY(char, mem);
}

TEST(JBoosterNet, serializationn_file_wrapper) {
  MessageBuffer buf(SerializationMode::BOTH);
  const char* file_name1 = "tmp_jbooster_net_file_wrapper.tmp1";
  const char* file_name2 = "tmp_jbooster_net_file_wrapper.tmp2";
  create_test_file_for_file_wrapper(file_name1);

  {
    FileWrapper fw(file_name1, SerializationMode::SERIALIZE);
    int times = 0;
    while (!fw.is_file_all_handled()) {
      EXPECT_EQ(buf.serialize_with_meta(&fw), 0);
      ++times;
    }
    EXPECT_EQ(times, 3);
  }

  buf.reset_cur_offset();

  {
    FileWrapper fw(file_name2, SerializationMode::DESERIALIZE);
    int times = 0;
    while (!fw.is_file_all_handled()) {
      EXPECT_EQ(buf.deserialize_with_meta(&fw), 0);
      ++times;
    }
    EXPECT_EQ(times, 3);
  }
  EXPECT_TRUE(FileUtils::is_same(file_name1, file_name2));
  FileUtils::remove(file_name1);
  FileUtils::remove(file_name2);
}

TEST(JBoosterNet, expansion_of_message_buffer) {
  MessageBuffer buf(SerializationMode::SERIALIZE);
  ASSERT_EQ(buf.buf_size(), 4096u);
  uint64_t v1 = 4096;
  for (int i = 0; i < 4096; ++i) {
    buf.serialize_no_meta(v1);
  }
  EXPECT_EQ(buf.cur_offset(), 32768u);
  EXPECT_EQ(buf.buf_size(), 32768u);
  char v2 = 'a';
  buf.serialize_no_meta(v2);
  EXPECT_EQ(buf.cur_offset(), 32769u);
  EXPECT_EQ(buf.buf_size(), 65536u);
}

TEST(JBoosterNet, message_sending_and_receiving) {
  Message send(SerializationMode::SERIALIZE);
  Message recv(SerializationMode::DESERIALIZE);

  const int arr_size = 66u;

  int i1 = 1;
  char c1 = '2';
  const char* s1 = "hello";
  const char* ns1 = nullptr;
  uintptr_t p1 = 3;
  GrowableArray<int> ga(arr_size, mtJBooster);
  for (int i = 0; i < arr_size; ++i) {
    ga.append(i);
  }
  ArrayWrapper<int> aw1(&ga);
  send.serialize(&i1, &c1, &s1, &ns1, &p1, &aw1);
  init_recv_message(recv, send);

  int i2 = 0;
  char c2 = '0';
  char s2[6];
  const char* ns2 = "should be null";
  uintptr_t p2 = 0;
  ArrayWrapper<int> aw2(false);
  recv.deserialize(&i2, &c2, &s2, &ns2, &p2, &aw2);

  EXPECT_EQ(i2, i1);
  EXPECT_EQ(c2, c1);
  EXPECT_STREQ(s2, s1);
  EXPECT_EQ(ns2, ns1);
  EXPECT_EQ(p2, p1);
  EXPECT_EQ(aw2.size(), arr_size);
  for (int i = 0; i < arr_size; ++ i) {
    ASSERT_EQ(*aw2.get(i), i);
  }
}

TEST(JBoosterNet, deserialization_exceptions) {
  Message send(SerializationMode::SERIALIZE);

  {
    CustomData1 d(-1, true);
    EXPECT_EQ(send.serialize(&d), 0);
  }

  { Message recv(SerializationMode::DESERIALIZE);
    init_recv_message(recv, send);
    CustomData1 d(CustomData1::M0, false);
    EXPECT_EQ(recv.deserialize(&d), JBErr::BAD_ARG_SIZE);
    EXPECT_EQ(recv.cur_buf_offset(), MessageConst::arg_meta_size + sizeof(int));
    EXPECT_EQ(d.v1(), CustomData1::V1);
    EXPECT_EQ(d.v2(), CustomData1::V0);
  }

  { Message recv(SerializationMode::DESERIALIZE);
    init_recv_message(recv, send);
    CustomData1 d(CustomData1::M1, false);
    EXPECT_EQ(recv.deserialize(&d), 0);
    EXPECT_EQ(recv.cur_buf_offset(), MessageConst::arg_meta_size + 2 * sizeof(int));
    EXPECT_EQ(d.v1(), CustomData1::V1);
    EXPECT_EQ(d.v2(), CustomData1::V0);
  }

  { Message recv(SerializationMode::DESERIALIZE);
    init_recv_message(recv, send);
    CustomData1 d(CustomData1::M2, false);
    EXPECT_EQ(recv.deserialize(&d), 0);
    EXPECT_EQ(recv.cur_buf_offset(), MessageConst::arg_meta_size + 2 * sizeof(int));
    EXPECT_EQ(d.v1(), CustomData1::V1);
    EXPECT_EQ(d.v2(), CustomData1::V2);
  }

  { Message recv(SerializationMode::DESERIALIZE);
    init_recv_message(recv, send);
    CustomData1 d(CustomData1::M3, false);
    EXPECT_EQ(recv.deserialize(&d), 0);
    EXPECT_EQ(recv.cur_buf_offset(), MessageConst::arg_meta_size + 2 * sizeof(int));
    EXPECT_EQ(d.v1(), CustomData1::V1);
    EXPECT_EQ(d.v2(), CustomData1::V2);
  }

  { Message recv(SerializationMode::DESERIALIZE);
    init_recv_message(recv, send);
    CustomData1 d(CustomData1::M4, false);
    EXPECT_EQ(recv.deserialize(&d), JBErr::BAD_ARG_SIZE);
    EXPECT_EQ(recv.cur_buf_offset(), MessageConst::arg_meta_size + 3 * sizeof(int));
    EXPECT_EQ(d.v1(), CustomData1::V1);
    EXPECT_EQ(d.v2(), CustomData1::V2);
  }

  { Message recv(SerializationMode::DESERIALIZE);
    init_recv_message(recv, send);
    CustomData1 d(CustomData1::M5, false);
    EXPECT_EQ(recv.deserialize(&d), JBErr::BAD_ARG_SIZE);
    EXPECT_EQ(recv.cur_buf_offset(), MessageConst::arg_meta_size + 3 * sizeof(int));
    EXPECT_EQ(d.v1(), CustomData1::V1);
    EXPECT_EQ(d.v2(), CustomData1::V2);
  }

  { Message recv(SerializationMode::DESERIALIZE);
    init_recv_message(recv, send);
    CustomData1 d(CustomData1::M6, false);
    EXPECT_EQ(recv.deserialize(&d), JBErr::BAD_ARG_DATA);
    EXPECT_EQ(recv.cur_buf_offset(), MessageConst::arg_meta_size + 3 * sizeof(int));
    EXPECT_EQ(d.v1(), CustomData1::V1);
    EXPECT_EQ(d.v2(), CustomData1::V2);
  }
}

#endif // INCLUDE_JBOOSTER
