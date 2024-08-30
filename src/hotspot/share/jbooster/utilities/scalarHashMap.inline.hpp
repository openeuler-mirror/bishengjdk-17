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

#ifndef SHARE_JBOOSTER_UTILITIES_SCALARHASHMAP_INLINE_HPP
#define SHARE_JBOOSTER_UTILITIES_SCALARHASHMAP_INLINE_HPP

#include "jbooster/utilities/scalarHashMap.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/hashtable.inline.hpp"

template <typename K, typename V, MEMFLAGS F>
inline ScalarHashMap<K, V, F>::ScalarHashMap(int table_size): KVHashtable<K, V, F>(table_size) {}

template <typename K, typename V, MEMFLAGS F>
inline ScalarHashMap<K, V, F>::~ScalarHashMap() {
  clear();
  // Base class ~BasicHashtable deallocates the buckets.
  // Note that ~BasicHashtable is not virtual. Be careful.
}

template <typename K, typename V, MEMFLAGS F>
inline void ScalarHashMap<K, V, F>::clear() {
  for (int i = 0; i < KVHashtable<K, V, F>::table_size(); ++i) {
    for (Entry* e = KVHashtable<K, V, F>::bucket(i); e != nullptr;) {
      Entry* cur = e;
      // read next before freeing.
      e = e->next();
      KVHashtable<K, V, F>::free_entry(cur);
    }
    Entry** p = (Entry**) KVHashtable<K, V, F>::bucket_addr(i);
    *p = nullptr; // clear out buckets.
  }
  assert((KVHashtable<K, V, F>::number_of_entries()) == 0, "sanity");
}

/**
 * BasicHashtable<F>::maybe_grow is not good enough.
 */
template <typename K, typename V, MEMFLAGS F>
inline bool ScalarHashMap<K, V, F>::resize_if_needed() {
  if (KVHashtable<K, V, F>::number_of_entries() > (_resize_load_trigger * KVHashtable<K, V, F>::table_size())) {
    int desired_size = KVHashtable<K, V, F>::calculate_resize(false);
    if (desired_size == KVHashtable<K, V, F>::table_size()) {
      return false;
    }
    assert(desired_size != 0, "sanity");
    return KVHashtable<K, V, F>::resize(desired_size);
  }
  return false;
}

#endif // SHARE_JBOOSTER_UTILITIES_SCALARHASHMAP_INLINE_HPP
