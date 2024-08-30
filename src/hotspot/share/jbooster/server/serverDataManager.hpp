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

#ifndef SHARE_JBOOSTER_SERVER_SERVERDATAMANAGER_HPP
#define SHARE_JBOOSTER_SERVER_SERVERDATAMANAGER_HPP

#include "jbooster/dataTransmissionUtils.hpp"
#include "jbooster/jBoosterManager.hpp"
#include "jbooster/jClientArguments.hpp"
#include "jbooster/net/serialization.hpp"
#include "jbooster/utilities/concurrentHashMap.hpp"

// Every client has a JClientSessionData and a JClientProgramData on the server.
//
// The JClientSessionData maintains the runtime info of the client session (a
// session represents a run of java program), such as the pointers of the class
// loaders.
//
// The JClientProgramData maintains the immutable data of a program (a program
// can be a .jar or a .class), especially the keys of the class loaders.
//
// Each JClientSessionData has one JClientProgramData and different JClientSessionData
// can share the same JClientProgramData, so the program data such as the class
// loaders can be shared between different client sessions.
//
// On the server the ServerDataManager manages the JClientSessionData list and
// the JClientProgramData list.
//
// +--------------------------------------------------------------------------+
// |                            ServerDataManager                             |
// | +------------------+                                +------------------+ |
// | | ClassLoaderTable |               .--------------->| ClassLoaderTable | |
// | +------------------+               |                +------------------+ |
// |       A        \                   |                   /        A        |
// |       |     +--------------------+ | +--------------------+     |        |
// |       |     | JClientProgramData | | | JClientProgramData |     |        |
// |       |     | task1.class        | | | task2.jar          |     |        |
// |       |     +--------------------+ | +--------------------+     |        |
// |       |         /                  |     /           \          |        |
// |       |        /                   |    /             \         |        |
// | +--------------------+   +--------------------+   +--------------------+ |
// | | JClientSessionData |   | JClientSessionData |   | JClientSessionData | |
// | | 192.168.0.11 (1)   |   | 192.168.0.11 (2)   |   |   192.168.0.12     | |
// | +--------------------+   +--------------------+   +--------------------+ |
// |                                                                          |
// +--------------------------------------------------------------------------+

// For each class loader on the client, we generate a corresponding fake class
// loader on the server.
//
//                 +---------------------+
//                 |  bootstrap loader   |
//                 +---------------------+
//                             A               (share boot loader
//                             |                and platform loader)
//                 +---------------------+
//                 | PlatformClassLoader |
//                 +---------------------+
//                           A  A
// +-----------------------+ |  | +----------------------------------------------+
// |   ClassLoaderTable1   | |  | |              ClassLoaderTable2               |
// |                       | |  | |                                              |
// | +-------------------+ | |  | |            +--------------------+            |
// | |  FakeAppLoader1   |---+  +--------------|   FakeAppLoader2   |            |
// | +-------------------+ |      |            +--------------------+            |
// |           A           |      |                A            A                |
// |           |           |      |                |            |                |
// | +-------------------+ |      | +-------------------+  +-------------------+ |
// | | FakeCustomLoader1 | |      | | FakeCustomLoader3 |  | FakeCustomLoader4 | |
// | +-------------------+ |      | +-------------------+  +-------------------+ |
// |           A           |      |                                              |
// |           |           |      +----------------------------------------------+
// | +-------------------+ |
// | | FakeCustomLoader2 | |
// | +-------------------+ |
// |                       |
// +-----------------------+

class ClassLoaderData;
class InstanceKlass;
class ServerControlThread;
class ServerListeningThread;

class RefCnt: public StackObj {
  static const int LOCKED = -1000;

  mutable volatile int _ref_cnt;

public:
  RefCnt(int init_ref_cnt = 0): _ref_cnt(init_ref_cnt) {}

  int get() const;
  int inc() const;
  int dec() const;
  bool try_lock() const;
  bool is_locked() const { return get() == LOCKED; }
};

class RefCntWithTime: public RefCnt {
  mutable volatile jlong _no_ref_begin_time;

public:
  RefCntWithTime(int init_ref_cnt = 0);
  int dec_and_update_time() const;
  jlong no_ref_time() const;
  jlong no_ref_time(jlong current_time) const;
};

class JClientCacheState final: public StackObj {
  friend class JClientProgramData;

  static const int NOT_GENERATED    = 0;
  static const int BEING_GENERATED  = 1;
  static const int GENERATED        = 2;

  bool          _is_allowed;
  volatile int  _state;
  const char*   _file_path;
  uint64_t      _file_timestamp;

private:
  NONCOPYABLE(JClientCacheState);
  void update_file_timestamp();
  void remove_file_and_set_not_generated_sync();
  bool set_being_generated_from_generated();
  void set_not_generated_from_being_generated();

public:
  JClientCacheState();
  ~JClientCacheState();

  void init(bool allow, const char* file_path);

  bool is_allowed() { return _is_allowed; }

  // <cache_dir>/server/cache-<parogram_str_id>-<cache-name>
  const char* file_path() { return _file_path; }
  uint64_t file_timestamp() { return _file_timestamp; }

  bool is_not_generated();
  bool is_being_generated();
  bool is_generated();
  bool set_being_generated();     // from state of not_generated
  void set_generated();           // from state of being_generated
  void set_not_generated();       // from state of being_generate

  // Unlike the APIs above, the APIs above only change the atomic variables,
  // while the following APIs checks whether the cache file exists.
  bool is_cached();

  void remove_file();
};

/**
 * The data of the program (usually a .class or .jar file) executed by the client.
 */
class JClientProgramData final: public CHeapObj<mtJBooster> {
public:
  using ClassLoaderTable = ConcurrentHashMap<ClassLoaderKey, ClassLoaderData*, mtJBooster>;

private:
  uint32_t _program_id;               // generated by the server
  const char* _program_str_id;        // generated by the client
  JClientArguments* _program_args;    // used to uniquely identify a client program

  ClassLoaderTable _class_loaders;

  RefCntWithTime _ref_cnt;

  JClientCacheState _clr_cache_state;
  JClientCacheState _cds_cache_state;
  JClientCacheState _aot_cache_state;

  NONCOPYABLE(JClientProgramData);

public:
  JClientProgramData(uint32_t program_id, JClientArguments* program_args);
  ~JClientProgramData();

  uint32_t program_id() const { return _program_id; }
  JClientArguments* program_args() const { return _program_args; }

  ClassLoaderTable* class_loaders() { return &_class_loaders; }

  ClassLoaderData* class_loader_data(const ClassLoaderKey& key, Thread* thread);
  ClassLoaderData* add_class_loader_if_absent(const ClassLoaderKey& key,
                                              const ClassLoaderKey& parent_key, Thread* thread);

  RefCntWithTime& ref_cnt() { return _ref_cnt; }

  JClientCacheState& clr_cache_state() { return _clr_cache_state; }
  JClientCacheState& cds_cache_state() { return _cds_cache_state; }
  JClientCacheState& aot_cache_state() { return _aot_cache_state; }
};

/**
 * @see TempNewSymbol
 */
class TempJClientProgramData : public StackObj {
  JClientProgramData* _temp;

public:
  TempJClientProgramData(JClientProgramData *s) : _temp(s) {}
  TempJClientProgramData(const TempJClientProgramData& rhs) : _temp(rhs._temp) { if (_temp != nullptr) _temp->ref_cnt().inc(); }
  ~TempJClientProgramData() { if (_temp != nullptr) _temp->ref_cnt().dec(); }

  void operator=(TempJClientProgramData rhs) = delete;
  bool    operator == (JClientProgramData* o) const = delete;
  operator JClientProgramData*()                             { return _temp; }
  JClientProgramData* operator -> () const                   { return _temp; }
};

/**
 * The session data of a client.
 */
class JClientSessionData final: public CHeapObj<mtJBooster> {
  friend class ServerStream;

public:
  using AddressMap = ConcurrentHashMap<address, address, mtJBooster>;

private:
  uint32_t _session_id;   // generated by the server
  uint64_t _random_id;    // generated by the client

  JClientProgramData* const _program_data;

  // server-side CLD pointer -> client-side CLD pointer
  AddressMap _cl_s2c;
  // client-side CLD pointer -> server-side CLD pointer
  AddressMap _cl_c2s;

  // client-side klass pointer -> server-side klass pointer
  AddressMap _k_c2s;

  // method pointer -> method data pointer (both server-side)
  AddressMap _m2md;

  RefCntWithTime _ref_cnt;

private:
  NONCOPYABLE(JClientSessionData);

  static address get_address(AddressMap& table, address key, Thread* thread);
  static bool put_address(AddressMap& table, address key, address value, Thread* thread);
  static bool remove_address(AddressMap& table, address key, Thread* thread);

public:
  JClientSessionData(uint32_t session_id, uint64_t client_random_id, JClientProgramData* program_data);
  ~JClientSessionData();

  uint32_t session_id() const { return _session_id; }

  uint64_t random_id() const { return _random_id; }

  JClientProgramData* program_data() const { return _program_data; }

  address client_cld_address(ClassLoaderData* server_data, Thread* thread);
  ClassLoaderData* server_cld_address(address client_data, Thread* thread);
  ClassLoaderData* add_class_loader_if_absent(address client_cld_addr,
                                              const ClassLoaderKey& key,
                                              const ClassLoaderKey& parent_key,
                                              Thread* thread);

  Klass* server_klass_address(address client_klass_addr, Thread* thread);
  void add_klass_address(address client_klass_addr, address server_cld_addr, Thread* thread);

  void klass_array(GrowableArray<address>* key_array, GrowableArray<address>* value_array, Thread* thread);

  void klass_pointer_map_to_server(GrowableArray<address>* klass_array, Thread* thread);

  void add_method_data(address method, address method_data, Thread* thread);
  bool remove_method_data(address method, Thread* thread);
  MethodData* method_data_address(address method, Thread* thread);

  RefCntWithTime& ref_cnt() { return _ref_cnt; }
};

/**
 * @see TempNewSymbol
 */
class TempJClientSessionData : public StackObj {
  JClientSessionData* _temp;

public:
  TempJClientSessionData(JClientSessionData *s) : _temp(s) {}
  TempJClientSessionData(const TempJClientSessionData& rhs) : _temp(rhs._temp) { if (_temp != nullptr) _temp->ref_cnt().inc(); }
  ~TempJClientSessionData() { if (_temp != nullptr) _temp->ref_cnt().dec(); }

  void operator=(TempJClientSessionData rhs) = delete;
  bool    operator == (JClientSessionData* o) const = delete;
  operator JClientSessionData*()                             { return _temp; }
  JClientSessionData* operator -> () const                   { return _temp; }
};

/**
 * The manager of JClientSessionData and JClientProgramData.
 */
class ServerDataManager: public CHeapObj<mtJBooster> {
public:
  class JClientProgramDataKey: public StackObj {
  private:
    // The lifetime of _args is managed by the value of JClientProgramDataMap.
    // So make sure the lifetime of JClientProgramData is outlive its JClientProgramDataKey.
    JClientArguments* _args;
  public:
    JClientProgramDataKey(JClientArguments* args): _args(args) {}
    uintx hash() const { return (uintx) _args->hash(); }
    bool equals(const JClientProgramDataKey& other) const {
      return _args->equals(other._args);
    }
  };

  struct JClientProgramDataMapEvents {
    static bool is_dead(const JClientProgramDataKey& key, JClientProgramData*& value) { return false; }
    static void on_get(const JClientProgramDataKey& key, JClientProgramData*& value) { value->ref_cnt().inc(); }
    static void on_del(const JClientProgramDataKey& key, JClientProgramData*& value) {
      if (value->ref_cnt().get() == 0) {
        // Do not invoke destroy_jbooster_class_loader() here as we are now in the lock of the map.
        delete value;
      } else {
        // The thread lost a race to insert this newly created object.
        guarantee(value->ref_cnt().get() == 1, "sanity");
      }
    }
  };

  struct JClientSessionDataMapEvents {
    static bool is_dead(const uint32_t& key, JClientSessionData*& value) { return false; }
    static void on_get(const uint32_t& key, JClientSessionData*& value) { value->ref_cnt().inc(); }
    static void on_del(const uint32_t& key, JClientSessionData*& value) {
      // There shouldn't be a insert race failure here, so the ref won't be 1.
      assert(value->ref_cnt().get() == 0, "sanity");
      delete value;
    }
  };

  using JClientProgramDataMap = ConcurrentHashMap<JClientProgramDataKey, JClientProgramData*, mtJBooster, JClientProgramDataMapEvents>;
  using JClientSessionDataMap = ConcurrentHashMap<uint32_t, JClientSessionData*, mtJBooster, JClientSessionDataMapEvents>;

private:
  static ServerDataManager* _singleton;

  const char* _cache_dir_path;

  uint64_t _random_id;

  JClientProgramDataMap _programs;
  JClientSessionDataMap _sessions;

  volatile uint32_t _next_program_allocation_id;
  volatile uint32_t _next_session_allocation_id;

  ServerListeningThread* _listening_thread;
  ServerControlThread* _control_thread;

  InstanceKlass* _main_klass;
  ClassLoaderData* _platform_loader_data;

private:
  NONCOPYABLE(ServerDataManager);

  ServerDataManager(TRAPS);

  static jint init_server_vm_options();
  static void init_cache_path(const char* optional_cache_path);

  JClientProgramData* get_or_create_program(JClientArguments* program_args, Thread* thread);

public:
  static ServerDataManager& get() {
    JBoosterManager::server_only();
    assert(_singleton != nullptr, "sanity");
    return *_singleton;
  }

  static jint init_phase1();
  static void init_phase2(TRAPS) { /* do nothing */ }
  static void init_phase3(int server_port, int connection_timeout, int cleanup_timeout, const char* cache_path, TRAPS);

  // $HOME/.jbooster/server
  const char* cache_dir_path() { return _cache_dir_path; }

  uint64_t random_id() { return _random_id; }

  JClientProgramDataMap* programs() { return &_programs; }
  JClientSessionDataMap* sessions() { return &_sessions; }

  JClientProgramData* get_program(JClientArguments* program_args, Thread* thread);
  bool try_remove_program(JClientArguments* program_args, Thread* thread);
  void remove_java_side_program_data(uint32_t program_id, TRAPS);

  JClientSessionData* get_session(uint32_t session_id, Thread* thread);
  JClientSessionData* create_session(uint64_t client_random_id,
                                     JClientArguments* program_args,
                                     Thread* thread);
  bool try_remove_session(uint32_t session_id, Thread* thread);

  ServerListeningThread* listening_thread() { return _listening_thread; }
  ServerControlThread* control_thread() { return _control_thread; }

  InstanceKlass* main_klass() { return _main_klass; };
  ClassLoaderData* platform_class_loader_data() { return _platform_loader_data; }

  void log_all_state(bool print_all = false);
};

#endif // SHARE_JBOOSTER_SERVER_SERVERDATAMANAGER_HPP
