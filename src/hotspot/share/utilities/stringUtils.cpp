/*
 * Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#include "precompiled.hpp"
#include "utilities/debug.hpp"
#include "utilities/stringUtils.hpp"
#if INCLUDE_JBOOSTER
#include "classfile/javaClasses.hpp"
#include "memory/allocation.hpp"
#include "oops/symbol.hpp"
#endif // INCLUDE_JBOOSTER

int StringUtils::replace_no_expand(char* string, const char* from, const char* to) {
  int replace_count = 0;
  size_t from_len = strlen(from);
  size_t to_len = strlen(to);
  assert(from_len >= to_len, "must not expand input");

  for (char* dst = string; *dst && (dst = strstr(dst, from)) != NULL;) {
    char* left_over = dst + from_len;
    memmove(dst, to, to_len);                       // does not copy trailing 0 of <to>
    dst += to_len;                                  // skip over the replacement.
    memmove(dst, left_over, strlen(left_over) + 1); // copies the trailing 0 of <left_over>
    ++ replace_count;
  }

  return replace_count;
}

double StringUtils::similarity(const char* str1, size_t len1, const char* str2, size_t len2) {
  assert(str1 != NULL && str2 != NULL, "sanity");

  // filter out zero-length strings else we will underflow on len-1 below
  if (len1 == 0 || len2 == 0) {
    return 0.0;
  }

  size_t total = len1 + len2;
  size_t hit = 0;

  for (size_t i = 0; i < len1 - 1; i++) {
    for (size_t j = 0; j < len2 - 1; j++) {
      if ((str1[i] == str2[j]) && (str1[i+1] == str2[j+1])) {
        ++hit;
        break;
      }
    }
  }

  return 2.0 * (double) hit / (double) total;
}

#if INCLUDE_JBOOSTER

uint32_t StringUtils::hash_code(const char* str) {
  if (str == nullptr) return 0u;
  return hash_code(str, strlen(str) + 1);
}

uint32_t StringUtils::hash_code(const char* str, int len) {
  assert(str != nullptr && str[len - 1] == '\0', "sanity");
  return java_lang_String::hash_code((const jbyte*) str, len - 1);
}

uint32_t StringUtils::hash_code(Symbol* sym) {
  if (sym == nullptr) return 0u;
  return java_lang_String::hash_code((const jbyte*) sym->base(), sym->utf8_length());
}

int StringUtils::compare(const char* str1, const char* str2) {
  if (str1 == nullptr) {
    return str2 == nullptr ? 0 : -1;
  }
  if (str2 == nullptr) {
    return str1 == nullptr ? 0 : +1;
  }
  return strcmp(str1, str2);
}

char* StringUtils::copy_to_resource(const char* str) {
  if (str == nullptr) return nullptr;
  return copy_to_resource(str, strlen(str) + 1);
}

char* StringUtils::copy_to_resource(const char* str, int len) {
  assert(str != nullptr && str[len - 1] == '\0', "sanity");
  return (char*) memcpy(NEW_RESOURCE_ARRAY(char, len), str, len);
}

char* StringUtils::copy_to_heap(const char* str, MEMFLAGS mt) {
  if (str == nullptr) return nullptr;
  return copy_to_heap(str, strlen(str) + 1, mt);
}

char* StringUtils::copy_to_heap(const char* str, int len, MEMFLAGS mt) {
  assert(str != nullptr && str[len - 1] == '\0', "sanity");
  return (char*) memcpy(NEW_C_HEAP_ARRAY(char, len, mt), str, len);
}

void StringUtils::free_from_heap(const char* str) {
  if (str == nullptr) return;
  FREE_C_HEAP_ARRAY(char, str);
}

const char* StringUtils::str(Symbol* sym) {
  return sym == nullptr ? "<null>" : sym->as_C_string();
}

#endif // INCLUDE_JBOOSTER
