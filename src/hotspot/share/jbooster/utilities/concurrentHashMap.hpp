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

#ifndef SHARE_JBOOSTER_UTILITIES_CONCURRENTHASHMAP_HPP
#define SHARE_JBOOSTER_UTILITIES_CONCURRENTHASHMAP_HPP

#include "memory/allocation.hpp"
#include "utilities/concurrentHashTable.hpp"
#include "utilities/globalDefinitions.hpp"

template <typename K, typename V>
struct DefaultConcurrentHashMapEvents {
  static bool is_dead(const K& key, V& value) { return false; }

  static void on_get(const K& key, V& value) {}

  /**
   * This function will be invoked in two cases:
   * 1. The element is being removed from the map;
   * 2. The element is failed to be inserted into the map (lost a race).
   * Consider both cases when implementing this function.
   *
   * @see SymbolTableConfig::free_node
   * @see ServerDataManager::get_or_create_program
   */
  static void on_del(const K& key, V& value) {}
};

/**
 * A simple and user-friendly concurrent hash map based on ConcurrentHashTable.
 *
 * About the `hash` of the key:
 * - If K is a scalar type (arithmetic, enum, pointer and nullptr types), then
 *   the hash is calculated by primitive_hash();
 * - Or please implement the member function `uintx hash() const` for your
 *   custom K class.
 *
 * About the `equals` of the key:
 * - Always use the operater `==`. Please overload the operator `==` in your
 *   custom K class.
 */
template <typename K, typename V, MEMFLAGS F, typename Events = DefaultConcurrentHashMapEvents<K, V>>
class ConcurrentHashMap: public CHeapObj<F> {
public:
  class KVNode {
    const K _key;
    mutable V _value;
  public:
    KVNode(const K& key, const V& value): _key(key), _value(value) {}
    const K& key() const { return _key; }
    V& value() const { return _value; }
  };

private:
  class KVNodeConfig : public AllStatic {
  public:
    typedef KVNode Value;
    static uintx get_hash(KVNode const& node, bool* is_dead);
    static void* allocate_node(void* context, size_t size, KVNode const& node);
    static void free_node(void* context, void* memory, KVNode const& node);
  };

  typedef ConcurrentHashTable<KVNodeConfig, F> KVNodeTable;

  class Lookup: public StackObj {
  protected:
    const K& _key;
    const uintx _hash;
  public:
    Lookup(const K& key);
    uintx get_hash() const { return _hash; }
    bool equals(KVNode* kv_node) const;
    bool is_dead(KVNode* value) const { return Events::is_dead(value->key(), value->value()); }
  };

  template <typename EVALUATE_FUNC>
  class RmLookup: public Lookup {
    EVALUATE_FUNC& _eval_f;
  public:
    RmLookup(const K& key, EVALUATE_FUNC& eval_f): Lookup(key), _eval_f(eval_f) {}
    bool equals(KVNode* kv_node) const;
  };

  class Get: public StackObj {
    KVNode* _res;
  public:
    Get() : _res(nullptr) {}
    void operator () (KVNode* kv_node);
    KVNode* res() const { return _res; }
  };

private:
  const float PREF_AVG_LIST_LEN = 2.0F;

  KVNodeTable _table;
  volatile size_t _items_count;

private:
  void init_lock(int lock_rank) NOT_DEBUG_RETURN;

  size_t item_added();
  size_t item_removed();
  size_t table_size(Thread* thread);
  float load_factor(Thread* thread);
  void grow_if_needed(Thread* thread);

public:
  ConcurrentHashMap();
  ConcurrentHashMap(int lock_rank);
  virtual ~ConcurrentHashMap();

  size_t size();

  bool contains(const K& key, Thread* thread);

  /**
   * Returns the value if found, or returns null.
   */
  V* get(const K& key, Thread* thread);

  /**
   * Returns the input value if the insertion is successful,
   * or returns the previously inserted value.
   */
  V* put_if_absent(const K& key, V& value, Thread* thread);

  /**
   * Returns whether the key is deleted.
   * * We cannot return the pointer of the removed value because
   *   it has been deleted.
   */
  bool remove(const K& key, Thread* thread);

  template <typename EVALUATE_FUNC>
  bool remove_if(const K& key, EVALUATE_FUNC& eval_f, Thread* thread);

  template <typename SCAN_FUNC>
  void for_each(SCAN_FUNC& scan_f, Thread* thread);

  template <typename EVALUATE_FUNC, typename DELETE_FUNC>
  void bulk_remove_if(EVALUATE_FUNC& eval_f, DELETE_FUNC& del_f, Thread* thread);
};

#endif // SHARE_JBOOSTER_UTILITIES_CONCURRENTHASHMAP_HPP
