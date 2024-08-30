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

#ifndef SHARE_JBOOSTER_UTILITIES_SCALARHASHMAP_HPP
#define SHARE_JBOOSTER_UTILITIES_SCALARHASHMAP_HPP

#include "utilities/hashtable.hpp"

/**
 * A simple hash map that does not manage the lifecycle of the storage elements.
 */
template <typename K, typename V, MEMFLAGS F = mtJBooster>
class ScalarHashMap: protected KVHashtable<K, V, F> {
  static_assert(std::is_scalar<K>::value, "K must be scalar because we use primitive_hash() and primitive_equals()");
  static_assert(std::is_scalar<V>::value, "V must be scalar because we don't manage its lifecycle at all");

  using Entry = typename KVHashtable<K, V, F>::KVHashtableEntry;

  static const int _resize_load_trigger = 4;

  bool resize_if_needed();

public:
  ScalarHashMap(int table_size = 107);
  virtual ~ScalarHashMap();

  V* get(K key) const {
    return KVHashtable<K, V, F>::lookup(key);
  }

  V* add_if_absent(K key, V value, bool* p_created) {
    V* res = KVHashtable<K, V, F>::add_if_absent(key, value, p_created);
    if (*p_created) {
      resize_if_needed();
    }
    return res;
  }

  void clear();

  int size() const { return KVHashtable<K, V, F>::number_of_entries(); }
};

/**
 * A simple hash set that does not manage the lifecycle of the storage elements.
 * We do not choose to extend Hashtable because it uses hashtable.cpp, which leads
 * to its poor availability (because of template).
 */
template <class T, MEMFLAGS F = mtJBooster>
class ScalarHashSet: protected ScalarHashMap<T, bool, F> {

public:
  ScalarHashSet(int table_size = 107): ScalarHashMap<T, bool, F>(table_size) {}

  /**
   * true: existing; false: not found
   */
  bool has(T o) const {
    return ScalarHashMap<T, bool, F>::get(o) != nullptr;
  }

  /**
   * true: added; false: existing
   */
  bool add(T o) {
    bool created = false;
    ScalarHashMap<T, bool, F>::add_if_absent(o, true, &created);
    return created;
  }

  void clear() { ScalarHashMap<T, bool, F>::clear(); }

  int size() const { return ScalarHashMap<T, bool, F>::number_of_entries(); }
};

#endif // SHARE_JBOOSTER_UTILITIES_SCALARHASHMAP_HPP
