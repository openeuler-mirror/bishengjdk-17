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

#include "jbooster/net/messageBuffer.inline.hpp"

/**
 * Allocate an extra eight bytes in case the array object is not 8-byte aligned.
 */
char* MessageBuffer::alloc_buf_obj(uint32_t new_buf_size) {
  return NEW_C_HEAP_ARRAY(char, new_buf_size + sizeof(int64_t), mtJBooster);
}

void MessageBuffer::del_buf_obj(char* buf_obj) {
  FREE_C_HEAP_ARRAY(char, buf_obj);
}

/**
 * Make the beginning of buf be 8-byte aligned.
 */
char* MessageBuffer::calc_buf_start(char* buf_obj) {
  return buf_obj + calc_padding<int64_t>((uint32_t) reinterpret_cast<uintptr_t>(buf_obj));
}

MessageBuffer::MessageBuffer(SerializationMode smode, CommunicationStream* stream):
    _smode(smode),
    _buf_size(_default_buf_size),
    _buf_obj(alloc_buf_obj(_buf_size)),
    _buf(calc_buf_start(_buf_obj)),
    _cur_offset(0),
    _stream(stream) {}

MessageBuffer::~MessageBuffer() {
  del_buf_obj(_buf_obj);
}

/**
 * Round capacity to power of 2, at most limit.
 * Make sure that 0 < _buf_size < required_size <= 0x80000000.
 */
uint32_t MessageBuffer::calc_new_buf_size(uint32_t required_size) {
  guarantee(required_size <= 0x80000000, "Message size is too big");
  uint32_t v = required_size - 1;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1;
}

void MessageBuffer::expand_buf(uint32_t required_size, uint32_t copy_size) {
  char* old_buf_obj = _buf_obj;
  char* old_buf = _buf;

  uint32_t new_buf_size = calc_new_buf_size(required_size);
  char* new_buf_obj = alloc_buf_obj(new_buf_size);
  char* new_buf = calc_buf_start(new_buf_obj);
  guarantee((new_buf_obj + new_buf_size + sizeof(int64_t)) >= (new_buf + copy_size), "sanity");
  memcpy(new_buf, old_buf, copy_size);

  _buf_obj = new_buf_obj;
  _buf = new_buf;
  _buf_size = new_buf_size;
  del_buf_obj(old_buf_obj);

  assert(((int) (reinterpret_cast<uintptr_t>(_buf)) & 7) == 0, "8-byte aligned");
}
