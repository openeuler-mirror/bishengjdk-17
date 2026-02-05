/*
 * Copyright (c) 2025, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2019 Alibaba Group Holding Limited. All rights reserved.
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

#ifndef SHARED_VM_JPROFILECACHE_JITPROFILECLASSCHAIN_HPP
#define SHARED_VM_JPROFILECACHE_JITPROFILECLASSCHAIN_HPP

#include "jprofilecache/jitProfileCacheHolders.hpp"
#include "runtime/jniHandles.hpp"

class ProfileCacheClassChain;

class MethodHolderIterator {
public:
  MethodHolderIterator()
    : _profile_cache_class_chain(nullptr),
      _current_method_hold(nullptr),
      _holder_index(-1) {
  }

  MethodHolderIterator(ProfileCacheClassChain* chain, ProfileCacheMethodHold* holder, int index)
    : _profile_cache_class_chain(chain),
      _current_method_hold(holder),
      _holder_index(index) {
  }

  ~MethodHolderIterator() { }

  ProfileCacheMethodHold* operator*() { return _current_method_hold; }

  int index() { return _holder_index; }

  bool initialized() { return _profile_cache_class_chain != nullptr; }

  ProfileCacheMethodHold* next();

private:
  ProfileCacheClassChain*      _profile_cache_class_chain;
  ProfileCacheMethodHold* _current_method_hold;
  int                  _holder_index;  // current holder's position in ProfileCacheClassChain
};

class  ProfileCacheClassEntry;

class ProfileCacheClassChain : public CHeapObj<mtClass> {
public:
  class ProfileCacheClassChainEntry : public CHeapObj<mtClass> {
  public:
    enum ClassState {
      _not_loaded = 0,
      _load_skipped,
      _class_loaded,
      _class_inited
    };

    ProfileCacheClassChainEntry()
      : _class_name(nullptr),
        _class_loader_name(nullptr),
        _class_path(nullptr),
        _class_state(_not_loaded),
        _method_holder(nullptr),
        _resolved_klasses(new (ResourceObj::C_HEAP, mtClass) GrowableArray<InstanceKlass*>(1, mtClass)),
        _method_keep_holders(new (ResourceObj::C_HEAP, mtClass) GrowableArray<jobject>(1, mtClass)) {  }

    ProfileCacheClassChainEntry(Symbol* class_name, Symbol* loader_name, Symbol* path)
      : _class_name(class_name),
        _class_loader_name(loader_name),
        _class_path(path),
        _class_state(_not_loaded),
        _method_holder(nullptr),
        _resolved_klasses(new (ResourceObj::C_HEAP, mtClass) GrowableArray<InstanceKlass*>(1, mtClass)),
        _method_keep_holders(new (ResourceObj::C_HEAP, mtClass) GrowableArray<jobject>(1, mtClass)) {  }

    virtual ~ProfileCacheClassChainEntry() {
      if(!_method_keep_holders->is_empty()) {
        int len = _method_keep_holders->length();
        for (int i = 0; i < len; i++) {
          JNIHandles::destroy_global(_method_keep_holders->at(i));
        }
      }
    }

    Symbol*        class_name()                  const { return _class_name; }
    Symbol*        class_loader_name()                 const { return _class_loader_name; }
    Symbol*        class_path()                        const { return _class_path; }
    void           set_class_name(Symbol* name)  { _class_name = name; }
    void           set_class_loader_name(Symbol* name) { _class_loader_name = name; }
    void           set_class_path(Symbol* path)        { _class_path = path; }

    GrowableArray<InstanceKlass*>* resolved_klasses()
                   { return _resolved_klasses; }

    GrowableArray<jobject>* method_keep_holders()
                   { return _method_keep_holders; }

    // entry state
    bool           is_not_loaded()        const { return _class_state == _not_loaded; }
    bool           is_skipped()           const { return _class_state == _load_skipped; }
    bool           is_loaded()            const { return _class_state == _class_loaded; }
    bool           is_inited()            const { return _class_state == _class_inited; }
    void           set_not_loaded()       { _class_state = _not_loaded; }
    void           set_skipped()          { _class_state = _load_skipped; }
    void           set_loaded()           { _class_state = _class_loaded; }
    void           set_inited()           { _class_state = _class_inited; }

    void           set_class_state(int state)   { _class_state = state;}

    int            class_state()                { return _class_state; }

    void add_method_holder(ProfileCacheMethodHold* h) {
      h->set_next(_method_holder);
      _method_holder = h;
    }

    bool is_all_initialized();

    bool contains_redefined_class();

    InstanceKlass* get_first_uninitialized_klass();

    ProfileCacheMethodHold* method_holder()  { return _method_holder; }

  private:

    Symbol*                              _class_name;
    Symbol*                              _class_loader_name;
    Symbol*                              _class_path;
    int                                  _class_state;

    ProfileCacheMethodHold*              _method_holder;
    GrowableArray<InstanceKlass*>*       _resolved_klasses;
    GrowableArray<jobject>*              _method_keep_holders;
  };

  ProfileCacheClassChain(unsigned int size);
  virtual ~ProfileCacheClassChain();

  enum ClassChainState {
    NOT_INITED = 0,
    INITED = 1,
    PRE_PROFILECACHE = 2,
    PROFILECACHE_COMPILING = 3,
    PROFILECACHE_DONE = 4,
    PROFILECACHE_PRE_DEOPTIMIZE = 5,
    PROFILECACHE_DEOPTIMIZING = 6,
    PROFILECACHE_DEOPTIMIZED = 7,
    PROFILECACHE_ERROR_STATE = 8
  };
  const char* get_state(ClassChainState state);
  bool try_transition_to_state(ClassChainState new_state);
  ClassChainState current_state() { return _state; }

  int  class_chain_inited_index()        const { return _class_chain_inited_index; }
  int  loaded_index()        const { return _loaded_class_index; }
  int  length()              const { return _length; }

  void set_loaded_index(int index)         { _loaded_class_index = index; }
  void set_length(int length)              { _length = length; }
  void set_inited_index(int index)         { _class_chain_inited_index = index; }

  bool notify_deopt_signal() {
    return try_transition_to_state(PROFILECACHE_PRE_DEOPTIMIZE);
  }

  bool can_record_class() {
    return _state == INITED || _state == PRE_PROFILECACHE || _state == PROFILECACHE_COMPILING;
  }

  bool deopt_has_done() {
    return _state == PROFILECACHE_DEOPTIMIZED;
  }

  void mark_loaded_class(InstanceKlass* klass, ProfileCacheClassEntry* class_entry);

  ProfileCacheClassChainEntry* at(int index) { return &_entries[index]; }

  void refresh_indexes();

  void precompilation();

  void unload_class();

  // count method
  void add_method_at_index(ProfileCacheMethodHold* mh, int index);

  bool compile_method(ProfileCacheMethodHold* mh);

  void preload_class_in_constantpool();

private:
  int                   _class_chain_inited_index;
  int                   _loaded_class_index;
  int                   _length;

  volatile ClassChainState _state;

  ProfileCacheClassChainEntry*  _entries;

  TimeStamp             _init_timestamp;
  TimeStamp             _last_timestamp;

  int                   _deopt_index;
  ProfileCacheMethodHold*  _deopt_cur_holder;

  bool                  _has_unmarked_compiling_flag;

  void handle_duplicate_class(InstanceKlass* k, int chain_index);

  void resolve_class_methods(InstanceKlass* k, ProfileCacheClassHolder* holder, int chain_index);

  void update_class_chain(InstanceKlass* ky, int chain_index);

  void compile_methodholders_queue(Stack<ProfileCacheMethodHold*, mtInternal>& compile_queue);

  void update_loaded_index(int index);

  ProfileCacheMethodHold* resolve_method_info(Method* method,
                                           ProfileCacheClassHolder* holder);
};

#endif // SHARED_VM_JPROFILECACHE_JITPROFILECLASSCHAIN_HPP