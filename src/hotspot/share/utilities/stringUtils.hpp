/*
 * Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_UTILITIES_STRINGUTILS_HPP
#define SHARE_UTILITIES_STRINGUTILS_HPP

#include "memory/allocation.hpp"

#if INCLUDE_JBOOSTER
class Symbol;
#endif // INCLUDE_JBOOSTER

class StringUtils : AllStatic {
public:
  // Replace the substring <from> with another string <to>. <to> must be
  // no longer than <from>. The input string is modified in-place.
  //
  // Replacement is done in a single pass left-to-right. So replace_no_expand("aaa", "aa", "a")
  // will result in "aa", not "a".
  //
  // Returns the count of substrings that have been replaced.
  static int replace_no_expand(char* string, const char* from, const char* to);

  // Compute string similarity based on Dice's coefficient
  static double similarity(const char* str1, size_t len1, const char* str2, size_t len2);

#if INCLUDE_JBOOSTER
  // A null-pointer-supported version of strcmp.
  static int compare(const char* str1, const char* str2);

  // Return 0 if str is null.
  static uint32_t hash_code(const char* str);
  static uint32_t hash_code(const char* str, int len);
  static uint32_t hash_code(Symbol* sym);

  // Do nothing and return null if the str is null.
  static char* copy_to_resource(const char* str);
  static char* copy_to_resource(const char* str, int len);

  // Do nothing and return null if the str is null.
  static char* copy_to_heap(const char* str, MEMFLAGS mt);
  static char* copy_to_heap(const char* str, int len, MEMFLAGS mt);

  // Do nothing if the str is null.
  static void free_from_heap(const char* str);

  // Return "<null>" if sym is null.
  static const char* str(Symbol* sym);
#endif // INCLUDE_JBOOSTER
};

#endif // SHARE_UTILITIES_STRINGUTILS_HPP
