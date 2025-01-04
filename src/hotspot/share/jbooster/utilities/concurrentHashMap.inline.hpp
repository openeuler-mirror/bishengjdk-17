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

#ifndef SHARE_JBOOSTER_UTILITIES_CONCURRENTHASHMAP_INLINE_HPP
#define SHARE_JBOOSTER_UTILITIES_CONCURRENTHASHMAP_INLINE_HPP

#include <type_traits>

#include "jbooster/utilities/concurrentHashMap.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "utilities/concurrentHashTable.inline.hpp"

template <typename K, typename Enable = void>
struct ConcurrentKeyHashEquals: public AllStatic {
  static uintx hash(const K& k) { return k.hash(); }
  static bool equals(const K& k1, const K& k2) { return k1.equals(k2); }
};

template <typename K>
struct ConcurrentKeyHashEquals<K, typename std::enable_if<std::is_scalar<K>::value>::type>: public AllStatic {
  static uintx hash(const K& key) { return primitive_hash(key); }
  static bool equals(const K& k1, const K& k2) { return k1 == k2; }
};

template <typename K, typename V, MEMFLAGS F, typename Events>
inline uintx ConcurrentHashMap<K, V, F, Events>::KVNodeConfig::get_hash(KVNode const& kv_node, bool* is_dead) {
  *is_dead = Events::is_dead(kv_node.key(), kv_node.value());
  if (*is_dead) return 0;
  return ConcurrentKeyHashEquals<K>::hash(kv_node.key());
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline void* ConcurrentHashMap<K, V, F, Events>::KVNodeConfig::allocate_node(void* context,
                                                                             size_t size,
                                                                             KVNode const& kv_node) {
  static_cast<ConcurrentHashMap<K, V, F, Events>*>(context)->item_added();
  return AllocateHeap(size, F);
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline void ConcurrentHashMap<K, V, F, Events>::KVNodeConfig::free_node(void* context,
                                                                        void* memory,
                                                                        KVNode const& kv_node) {
  Events::on_del(kv_node.key(), kv_node.value());
  kv_node.~KVNode();
  FreeHeap(memory);
  static_cast<ConcurrentHashMap<K, V, F, Events>*>(context)->item_removed();
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline ConcurrentHashMap<K, V, F, Events>::Lookup::Lookup(const K& key):
        _key(key),
        _hash(ConcurrentKeyHashEquals<K>::hash(key)) {}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline bool ConcurrentHashMap<K, V, F, Events>::Lookup::equals(KVNode* kv_node) const {
  if (ConcurrentKeyHashEquals<K>::equals(_key, kv_node->key())) {
    Events::on_get(kv_node->key(), kv_node->value());
    return true;
  }
  return false;
}

template <typename K, typename V, MEMFLAGS F, typename Events>
template <typename EVALUATE_FUNC>
inline bool ConcurrentHashMap<K, V, F, Events>::RmLookup<EVALUATE_FUNC>::equals(KVNode* kv_node) const {
  // Do not invoke Events::on_get() here.
  return ConcurrentKeyHashEquals<K>::equals(Lookup::_key, kv_node->key())
         && _eval_f(kv_node->key(), kv_node->value());
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline void ConcurrentHashMap<K, V, F, Events>::Get::operator () (KVNode* kv_node) {
  _res = kv_node;
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline ConcurrentHashMap<K, V, F, Events>::ConcurrentHashMap(): _table(8), _items_count(0) {
  _table._context = static_cast<void*>(this);
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline ConcurrentHashMap<K, V, F, Events>::ConcurrentHashMap(int lock_rank): _table(8), _items_count(0) {
  init_lock(lock_rank);
  _table._context = static_cast<void*>(this);
}

#ifdef ASSERT
template <typename K, typename V, MEMFLAGS F, typename Events>
inline void ConcurrentHashMap<K, V, F, Events>::init_lock(int lock_rank) {
  if (_table._resize_lock->rank() == lock_rank) return;
  delete _table._resize_lock;
  _table._resize_lock = new Mutex(lock_rank, "ConcurrentHashMap", true,
                                  Mutex::_safepoint_check_never);
}
#endif // ASSERT

template <typename K, typename V, MEMFLAGS F, typename Events>
inline ConcurrentHashMap<K, V, F, Events>::~ConcurrentHashMap() {}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline size_t ConcurrentHashMap<K, V, F, Events>::item_added() {
  return Atomic::add(&_items_count, (size_t) 1);
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline size_t ConcurrentHashMap<K, V, F, Events>::item_removed() {
  return Atomic::add(&_items_count, (size_t) -1);
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline size_t ConcurrentHashMap<K, V, F, Events>::table_size(Thread* thread) {
  return ((size_t) 1) << _table.get_size_log2(thread);
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline float ConcurrentHashMap<K, V, F, Events>::load_factor(Thread* thread) {
  return float(_items_count) / float(table_size(thread));
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline void ConcurrentHashMap<K, V, F, Events>::grow_if_needed(Thread* thread) {
  if (load_factor(thread) > PREF_AVG_LIST_LEN && !_table.is_max_size_reached()) {
    _table.grow(thread);
  }
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline size_t ConcurrentHashMap<K, V, F, Events>::size() {
  return Atomic::load_acquire(&_items_count);
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline bool ConcurrentHashMap<K, V, F, Events>::contains(const K& key, Thread* thread) {
  Lookup lookup(key);
  Get get;
  bool grow_hint = false;

  return _table.get(thread, lookup, get, &grow_hint);
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline V* ConcurrentHashMap<K, V, F, Events>::get(const K& key, Thread* thread) {
  Lookup lookup(key);
  Get get;
  bool grow_hint = false;

  V* res = nullptr;

  if (_table.get(thread, lookup, get, &grow_hint)) {
    assert(get.res() != nullptr, "sanity");
    res = &(get.res()->value());
  }

  return res;
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline V* ConcurrentHashMap<K, V, F, Events>::put_if_absent(const K& key, V& value, Thread* thread) {
  Lookup lookup(key);
  Get get;
  bool grow_hint = false;
  bool clean_hint = false;

  V* res = nullptr;

  KVNode kv_node(key, value);
  bool success = _table.insert_get(thread, lookup, kv_node, get, &grow_hint, &clean_hint);
  assert(get.res() != nullptr, "sanity");
  res = &(get.res()->value());

  if (grow_hint) {
    grow_if_needed(thread);
  }

  return res;
}

template <typename K, typename V, MEMFLAGS F, typename Events>
inline bool ConcurrentHashMap<K, V, F, Events>::remove(const K& key, Thread* thread) {
  Lookup lookup(key);
  return _table.remove(thread, lookup);
}

template <typename K, typename V, MEMFLAGS F, typename Events>
template <typename EVALUATE_FUNC>
bool ConcurrentHashMap<K, V, F, Events>::remove_if(const K& key, EVALUATE_FUNC& eval_f, Thread* thread) {
  RmLookup<EVALUATE_FUNC> lookup(key, eval_f);
  return _table.remove(thread, lookup);
}

template <typename K, typename V, MEMFLAGS F, typename Events>
template <typename SCAN_FUNC>
inline void ConcurrentHashMap<K, V, F, Events>::for_each(SCAN_FUNC& scan_f, Thread* thread) {
  // Unlike ConcurrentHashTable, ConcurrentHashMap never do scan without resize lock.
  // So we don't have to assert `!SafepointSynchronize::is_at_safepoint()`.
  assert(_table._resize_lock_owner != thread, "Re-size lock held");
  _table.lock_resize_lock(thread);
  _table.do_scan_locked(thread, scan_f);
  _table.unlock_resize_lock(thread);
  assert(_table._resize_lock_owner != thread, "Re-size lock held");
}

template <typename K, typename V, MEMFLAGS F, typename Events>
template <typename EVALUATE_FUNC, typename DELETE_FUNC>
inline void ConcurrentHashMap<K, V, F, Events>::bulk_remove_if(EVALUATE_FUNC& eval_f, DELETE_FUNC& del_f, Thread* thread) {
  // Unlike ConcurrentHashTable, ConcurrentHashMap never do scan without resize lock.
  // So we don't have to assert `!SafepointSynchronize::is_at_safepoint()`.
  assert(_table._resize_lock_owner != thread, "Re-size lock held");
  _table.lock_resize_lock(thread);
  _table.do_bulk_delete_locked(thread, eval_f, del_f);
  _table.unlock_resize_lock(thread);
  assert(_table._resize_lock_owner != thread, "Re-size lock held");
}

#endif // SHARE_JBOOSTER_UTILITIES_CONCURRENTHASHMAP_INLINE_HPP
