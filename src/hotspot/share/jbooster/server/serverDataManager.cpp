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

#include "classfile/javaClasses.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "jbooster/jBoosterManager.hpp"
#include "jbooster/net/serialization.hpp"
#include "jbooster/net/serverListeningThread.hpp"
#include "jbooster/server/serverControlThread.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/utilities/concurrentHashMap.inline.hpp"
#include "jbooster/utilities/fileUtils.hpp"
#include "logging/log.hpp"
#include "memory/metadataFactory.hpp"
#include "oops/instanceKlass.inline.hpp"
#include "oops/methodData.hpp"
#include "oops/symbol.hpp"
#include "runtime/atomic.hpp"
#include "runtime/globals.hpp"
#include "runtime/globals_extension.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/os.inline.hpp"
#include "runtime/thread.hpp"
#include "utilities/formatBuffer.hpp"
#include "utilities/stringUtils.hpp"

static InstanceKlass* get_jbooster_class_loader_klass(TRAPS) {
  assert(THREAD->thread_state() == _thread_in_vm, "sanity");

  TempNewSymbol loader_k_name = SymbolTable::new_symbol("jdk/jbooster/JBoosterClassLoader");
  Handle class_loader(THREAD, SystemDictionary::java_system_loader());
  Klass* loader_k = SystemDictionary::resolve_or_fail(loader_k_name, class_loader, Handle(), true, CHECK_NULL);
  guarantee(loader_k != nullptr && loader_k->is_instance_klass(), "sanity");
  InstanceKlass* loader_ik = InstanceKlass::cast(loader_k);
  loader_ik->initialize(CHECK_NULL);
  return loader_ik;
}

static ClassLoaderData* create_jbooster_class_loader(uint32_t program_id,
                                                     const ClassLoaderKey& key,
                                                     ClassLoaderData* parent,
                                                     TRAPS) {
  ThreadInVMfromNative tiv(THREAD);

  // get the class
  InstanceKlass* loader_ik = get_jbooster_class_loader_klass(CHECK_NULL);

  // get the method
  TempNewSymbol method_name = SymbolTable::new_symbol("create");
  TempNewSymbol method_sig = SymbolTable::new_symbol("(ILjava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)"
                                                     "Ljdk/jbooster/JBoosterClassLoader;");

  // invoke it and get a new loader obj
  JavaValue result(T_OBJECT);
  JavaCallArguments java_args;
  int arg1 = (int) program_id;
  Handle arg2 = key.loader_class_name() == nullptr ? Handle()
              : java_lang_String::create_from_symbol(key.loader_class_name(), CHECK_NULL);
  Handle arg3 = key.loader_name() == nullptr ? Handle()
              : java_lang_String::create_from_symbol(key.loader_name(), CHECK_NULL);
  Handle arg4(THREAD, parent->class_loader());
  java_args.push_int(arg1);
  java_args.push_oop(arg2);
  java_args.push_oop(arg3);
  java_args.push_oop(arg4);
  JavaCalls::call_static(&result, loader_ik, method_name, method_sig, &java_args, CHECK_NULL);
  Handle new_loader(THREAD, result.get_oop());

  // register and get ClassLoaderData
  ClassLoaderData* res_data = SystemDictionary::register_loader(new_loader);
  guarantee(res_data != nullptr, "sanity");

  ResourceMark rm;
  log_trace(jbooster, compilation)("New custom class loader:\n - loader: \"%s:%p\"\n - parent: \"%s:%p\"",
                                   res_data->loader_name(), res_data, parent->loader_name(), parent);

  return res_data;
}

static void destroy_jbooster_class_loader(uint32_t program_id, TRAPS) {
  ThreadInVMfromNative tiv(THREAD);

  // get the class
  InstanceKlass* loader_ik = get_jbooster_class_loader_klass(CHECK);

  // get the method
  TempNewSymbol method_name = SymbolTable::new_symbol("destroy");
  TempNewSymbol method_sig = vmSymbols::int_void_signature();

  // invoke it
  JavaValue result(T_VOID);
  JavaCallArguments java_args;
  int arg1 = (int) program_id;
  java_args.push_int(arg1);
  JavaCalls::call_static(&result, loader_ik, method_name, method_sig, &java_args, CHECK);
}

// -------------------------------------- RefCnt --------------------------------------

int RefCnt::get() const {
  return Atomic::load_acquire(&_ref_cnt);
}

/**
 * If the returned value is < 0, this ref cnt has been
 * locked by another thread and is no longer avaliable.
 */
int RefCnt::inc() const {
  int res;
  do {
    res = Atomic::load_acquire(&_ref_cnt);
    if (res == LOCKED) return res;
  } while (Atomic::cmpxchg(&_ref_cnt, res, res + 1) != res);
  return res + 1;
}

/**
 * This function should be invoked after inc()
 * (and the returned value of inc() must be >= 0).
 */
int RefCnt::dec() const {
  return Atomic::add(&_ref_cnt, (int) -1);
}

bool RefCnt::try_lock() const {
  return Atomic::cmpxchg(&_ref_cnt, 0, LOCKED) == 0;
}

RefCntWithTime::RefCntWithTime(int init_ref_cnt): RefCnt(init_ref_cnt),
                                                  _no_ref_begin_time(os::javaTimeMillis()) {}

int RefCntWithTime::dec_and_update_time() const {
  if (get() == 1) {
    Atomic::release_store(&_no_ref_begin_time, os::javaTimeMillis());
  }
  return dec();
}

jlong RefCntWithTime::no_ref_time() const {
  return no_ref_time(os::javaTimeMillis());
}

jlong RefCntWithTime::no_ref_time(jlong current_time) const {
  return current_time - Atomic::load_acquire(&_no_ref_begin_time);
}

// -------------------------------- JClientCacheState ---------------------------------

JClientCacheState::JClientCacheState(): _is_allowed(false),
                                        _state(NOT_GENERATED),
                                        _file_path(nullptr),
                                        _file_timestamp(0) {
  // The real assignment is in JClientProgramData::JClientProgramData().
}

void JClientCacheState::init(bool allow, const char* file_path) {
  _is_allowed = allow;
  _file_path = file_path;
  bool file_exists = FileUtils::is_file(file_path);
  _state = file_exists ? GENERATED : NOT_GENERATED;
  update_file_timestamp();
}

JClientCacheState::~JClientCacheState() {
  remove_file();
  StringUtils::free_from_heap(_file_path);
}

void JClientCacheState::update_file_timestamp() {
  if (is_not_generated()) {
    _file_timestamp = 0;
  } else {
    _file_timestamp = FileUtils::modify_time(_file_path);
  }
}

void JClientCacheState::remove_file_and_set_not_generated_sync() {
  if (set_being_generated_from_generated()) {
    FileUtils::remove(file_path());
    update_file_timestamp();
    set_not_generated_from_being_generated();
  }
}

bool JClientCacheState::is_not_generated() {
  return Atomic::load_acquire(&_state) == NOT_GENERATED;
}

bool JClientCacheState::is_being_generated() {
  return Atomic::load_acquire(&_state) == BEING_GENERATED;
}

bool JClientCacheState::is_generated() {
  return Atomic::load_acquire(&_state) == GENERATED;
}

bool JClientCacheState::set_being_generated() {
  return Atomic::cmpxchg(&_state, NOT_GENERATED, BEING_GENERATED) == NOT_GENERATED;
}

void JClientCacheState::set_generated() {
  update_file_timestamp();
  bool success = Atomic::cmpxchg(&_state, BEING_GENERATED, GENERATED) == BEING_GENERATED;
  guarantee(success, "sanity");
}

void JClientCacheState::set_not_generated() {
  bool success = Atomic::cmpxchg(&_state, BEING_GENERATED, NOT_GENERATED) == BEING_GENERATED;
  guarantee(success, "sanity");
}

bool JClientCacheState::set_being_generated_from_generated() {
  return Atomic::cmpxchg(&_state, GENERATED, BEING_GENERATED) == GENERATED;
}

void JClientCacheState::set_not_generated_from_being_generated() {
  bool success = Atomic::cmpxchg(&_state, BEING_GENERATED, NOT_GENERATED) == BEING_GENERATED;
  guarantee(success, "sanity");
}

bool JClientCacheState::is_cached() {
  if (is_generated()) {
    bool is_file = FileUtils::is_file(file_path());
    if (is_file) {
      uint64_t timestamp = FileUtils::modify_time(file_path());
      if (timestamp == _file_timestamp) return true;
      log_warning(jbooster)("Cache file \"%s\" is tampered with so will be deleted: "
                            "actual=" UINT64_FORMAT ", expect=" UINT64_FORMAT ".",
                            file_path(), timestamp, _file_timestamp);
    } else {
      log_warning(jbooster)("Cache file \"%s\" does not exist! Reset state to NOT_GENERATED.", file_path());
    }
    remove_file_and_set_not_generated_sync();
  }
  return false;
}

void JClientCacheState::remove_file() {
  if (is_cached()) {
    remove_file_and_set_not_generated_sync();
  }
}

// ------------------------------ JClientProgramData -------------------------------

JClientProgramData::JClientProgramData(uint32_t program_id, JClientArguments* program_args):
                                       _program_id(program_id),
                                       _program_args(program_args->move()),
                                       _class_loaders(),
                                       _ref_cnt(1) {
  const char* sd = ServerDataManager::get().cache_dir_path();
  _program_str_id = JBoosterManager::calc_program_string_id(_program_args->program_name(),
                                                            _program_args->program_entry(),
                                                            _program_args->is_jar(),
                                                            _program_args->hash());
  bool allow_clr = _program_args->jbooster_allow_clr();
  bool allow_cds = _program_args->jbooster_allow_cds();
  bool allow_aot = _program_args->jbooster_allow_aot();
  bool allow_pgo = _program_args->jbooster_allow_pgo();
  const char* clr_path = JBoosterManager::calc_cache_path(sd, _program_str_id, "clr.log");
  const char* cds_path = JBoosterManager::calc_cache_path(sd, _program_str_id, "cds.jsa");
  const char* aot_path_suffix = allow_pgo ? "aot-pgo.so" : "aot.so";
  const char* aot_path = JBoosterManager::calc_cache_path(sd, _program_str_id, aot_path_suffix);
  clr_cache_state().init(allow_clr, clr_path);
  cds_cache_state().init(allow_cds, cds_path);
  aot_cache_state().init(allow_aot, aot_path);
  _using_pgo = allow_pgo;
}

JClientProgramData::~JClientProgramData() {
  guarantee(ref_cnt().get() == 0, "ref=%d, pd=%u", ref_cnt().get(), program_id());
  delete _program_args;
  StringUtils::free_from_heap(_program_str_id);
}

ClassLoaderData* JClientProgramData::class_loader_data(const ClassLoaderKey& key, Thread* thread) {
  ClassLoaderData** res = _class_loaders.get(key, thread);
  return res == nullptr ? nullptr : *res;
}

ClassLoaderData* JClientProgramData::add_class_loader_if_absent(const ClassLoaderKey& key,
                                                               const ClassLoaderKey& parent_key,
                                                               Thread* thread) {
  // return the existing cld if if exists
  ClassLoaderData* res_data = class_loader_data(key, thread);
  if (res_data != nullptr) return res_data;

  // get/create the cld to insert
  ClassLoaderData* new_data;
  if (key.is_boot_loader() && parent_key.is_boot_loader()) {
    new_data = ClassLoaderData::the_null_class_loader_data();
  } else if (key.is_platform_loader() && parent_key.is_boot_loader()) {
    new_data = ServerDataManager::get().platform_class_loader_data();
  } else {
    ClassLoaderData* parent_data = class_loader_data(parent_key, thread);
    JavaThread* THREAD = thread->as_Java_thread();
    new_data = create_jbooster_class_loader(program_id(), key, parent_data, CATCH);
  }

  // try to insert
  ClassLoaderData** res = _class_loaders.put_if_absent(key, new_data, thread);
  guarantee(res != nullptr, "sanity");
  res_data = *res;

  // duplicate insertion, so delete the duplicate cld
  if (res_data != new_data) {
    if (!new_data->is_builtin_class_loader_data()) {
      // [JBOOSTER TODO] failed to insert so unregister the `res_data` above
    }
  }

  return res_data;
}

// ------------------------------ JClientSessionData -------------------------------

class AddressMapKVArray: public StackObj {
  GrowableArray<address>* _key_array;
  GrowableArray<address>* _value_array;
public:
  AddressMapKVArray(GrowableArray<address>* key_array,
                    GrowableArray<address>* value_array) :
                    _key_array(key_array),
                    _value_array(value_array) {}

  bool operator () (JClientSessionData::AddressMap::KVNode* kv_node) {
    assert(kv_node->key() != nullptr && kv_node->value() != nullptr, "sanity");
    _key_array->append(kv_node->key());
    _value_array->append(kv_node->value());
    return true;
  }
};

JClientSessionData::JClientSessionData(uint32_t session_id,
                                       uint64_t client_random_id,
                                       JClientProgramData* program_data):
        _session_id(session_id),
        _random_id(client_random_id),
        _program_data(program_data),
        _cl_s2c(),
        _cl_c2s(),
        _k_c2s(),
        _m2md(Mutex::nonleaf),
        _ref_cnt(1) {}

JClientSessionData::~JClientSessionData() {
  guarantee(ref_cnt().get() == 0, "sanity");
  _program_data->ref_cnt().dec_and_update_time();
  if (_m2md.size() > 0) {
    JavaThread* THREAD = JavaThread::current();
    auto clear_func = [] (JClientSessionData::AddressMap::KVNode* kv_node) -> bool {
      assert(kv_node->key() != nullptr && kv_node->value() != nullptr, "sanity");
      Method* m = (Method*)kv_node->key();
      MethodData* md = (MethodData*)kv_node->value();
      assert(*md->get_failed_speculations_address() == NULL, "must be");
      md->~MethodData();
      ClassLoaderData* loader_data = m->method_holder()->class_loader_data();
      MetadataFactory::free_metadata(loader_data, md);
      return true;
    };
    _m2md.for_each(clear_func, THREAD);
  }
}

address JClientSessionData::get_address(AddressMap& table, address key, Thread* thread) {
  address* res = table.get(key, thread);
  if (res == nullptr) return nullptr;
  return *res;
}

bool JClientSessionData::put_address(AddressMap& table, address key, address value, Thread* thread) {
  address* res = table.put_if_absent(key, value, thread);
  return res != nullptr && *res == value;
}

bool JClientSessionData::remove_address(AddressMap& table, address key, Thread* thread) {
  return table.remove(key, thread);
}

address JClientSessionData::client_cld_address(ClassLoaderData* server_data, Thread* thread) {
  return get_address(_cl_s2c, (address) server_data, thread);
}

ClassLoaderData* JClientSessionData::server_cld_address(address client_data, Thread* thread) {
  return (ClassLoaderData*) get_address(_cl_c2s, client_data, thread);
}

Klass* JClientSessionData::server_klass_address(address client_klass_addr, Thread* thread) {
  return (Klass*) get_address(_k_c2s, client_klass_addr, thread);
}

void JClientSessionData::add_klass_address(address client_klass_addr,
                                           address server_cld_addr,
                                           Thread* thread) {
  put_address(_k_c2s, client_klass_addr, server_cld_addr, thread);
}

void JClientSessionData::klass_array(GrowableArray<address>* key_array,
                                     GrowableArray<address>* value_array,
                                     Thread* thread) {
  AddressMapKVArray array(key_array, value_array);
  _k_c2s.for_each(array, thread);
}

void JClientSessionData::klass_pointer_map_to_server(GrowableArray<address>* klass_array, Thread* thread) {
  for (GrowableArrayIterator<address> iter = klass_array->begin();
                                      iter != klass_array->end(); ++iter) {
    InstanceKlass* klass = (InstanceKlass*) (*iter);
    Array<Method*>* methods = klass->methods();
    for (int method_index = 0; method_index < methods->length(); method_index++) {
      MethodData* method_data = method_data_address((address)(methods->at(method_index)), thread);
      if (method_data == nullptr) continue;
      // relocate klass pointer in method_data
      int position = 0;
      while (position < method_data->data_size()) {
        ProfileData* profile_data = method_data->data_at(position);
        profile_data->klass_pointer_map_to_server(this, thread);
        position += profile_data->size_in_bytes();
      }
    }
  }
}

void JClientSessionData::add_method_data(address method, address method_data, Thread* thread) {
  put_address(_m2md, method, method_data, thread);
}

bool JClientSessionData::remove_method_data(address method, Thread* thread) {
  return remove_address(_m2md, method, thread);
}

MethodData* JClientSessionData::method_data_address(address method, Thread* thread) {
  return (MethodData*) get_address(_m2md, method, thread);
}

ClassLoaderData* JClientSessionData::add_class_loader_if_absent(address client_cld_addr,
                                                                const ClassLoaderKey& key,
                                                                const ClassLoaderKey& parent_key,
                                                                Thread* thread) {
  // has been registered
  address server_cld_addr = get_address(_cl_c2s, client_cld_addr, thread);
  if (server_cld_addr != nullptr) return (ClassLoaderData*) server_cld_addr;

  // register
  ClassLoaderData* data = _program_data->add_class_loader_if_absent(key, parent_key, thread);
  server_cld_addr = (address) data;
  bool success = put_address(_cl_s2c, server_cld_addr, client_cld_addr, thread);
  if (success) {
    success = put_address(_cl_c2s, client_cld_addr, server_cld_addr, thread);
  }
  if (!success) {
    ResourceMark rm(thread);
    log_trace(jbooster)("Duplicate class loader key: loader_class=%s, first_loaded_class=%s, "
                        "client_cld_addr=%p, server_cld_addr=%p.",
                        StringUtils::str(key.loader_class_name()),
                        StringUtils::str(key.first_loaded_class_name()),
                        client_cld_addr, server_cld_addr);
  }
  return success ? data : nullptr;
}

// ------------------------------- ServerDataManager -------------------------------

ServerDataManager* ServerDataManager::_singleton = nullptr;

ServerDataManager::ServerDataManager(TRAPS): _programs(Mutex::leaf + 1),
                                             _sessions(Mutex::leaf + 1),
                                             _next_program_allocation_id(0),
                                             _next_session_allocation_id(0) {
  _cache_dir_path = JBoosterCachePath;
  _random_id = JBoosterManager::calc_random_id();

  TempNewSymbol jbooster_main_name = SymbolTable::new_symbol("jdk/jbooster/JBooster");
  Klass* main_k = SystemDictionary::resolve_or_null(
          jbooster_main_name,
          Handle(THREAD, SystemDictionary::java_platform_loader()),
          Handle(), THREAD);
  guarantee(main_k != nullptr && main_k->is_instance_klass(), "sanity");
  _main_klass = InstanceKlass::cast(main_k);

  Handle platform_loader(THREAD, SystemDictionary::java_platform_loader());
  _platform_loader_data = ClassLoaderData::class_loader_data(platform_loader());
}

jint ServerDataManager::init_server_vm_options() {
#define DO_NOT_SET_FLAG(flag)                                                       \
  if (FLAG_IS_CMDLINE(flag)) {                                                      \
    vm_exit_during_initialization("The flag " #flag " is client-only!");            \
  }

  // Check some typical flags.
  // In fact, all flags should not be used (manually) on the server.
  DO_NOT_SET_FLAG(UseJBooster);
  DO_NOT_SET_FLAG(JBoosterPort);
  DO_NOT_SET_FLAG(JBoosterTimeout);
  DO_NOT_SET_FLAG(JBoosterCachePath);

#undef DO_NOT_SET_FLAG
  if (FLAG_SET_CMDLINE(TypeProfileWidth, 8) != JVMFlag::SUCCESS) {
    return JNI_EINVAL;
  }
  return JNI_OK;
}

jint ServerDataManager::init_phase1() {
  JBoosterManager::server_only();
  return init_server_vm_options();
}

void ServerDataManager::init_cache_path(const char* optional_cache_path) {
  if (optional_cache_path == nullptr) {
    FLAG_SET_DEFAULT(JBoosterCachePath, JBoosterManager::calc_cache_dir_path(false));
  } else {
    FLAG_SET_CMDLINE(JBoosterCachePath, StringUtils::copy_to_heap(optional_cache_path, mtJBooster));
  }
  FileUtils::mkdirs(JBoosterCachePath);
}

void ServerDataManager::init_phase3(int server_port, int connection_timeout, int cleanup_timeout, const char* cache_path, TRAPS) {
  JBoosterManager::server_only();
  init_cache_path(cache_path);

  _singleton = new ServerDataManager(CHECK);

  ServerControlThread::set_unused_shared_data_cleanup_timeout(cleanup_timeout);
  _singleton->_control_thread = ServerControlThread::start_thread(CHECK);
  _singleton->_listening_thread = ServerListeningThread::start_thread(JBoosterAddress, (uint16_t) server_port, connection_timeout, CHECK);
}

/**
 * The ref_cnt of the returned JClientProgramData will +1.
 */
JClientProgramData* ServerDataManager::get_program(JClientArguments* program_args, Thread* thread) {
  JClientProgramData** res = _programs.get(program_args, thread);
  if (res == nullptr) return nullptr;
  return *res;
}

/**
 * The ref_cnt of the returned JClientProgramData will +1.
 */
JClientProgramData* ServerDataManager::get_or_create_program(JClientArguments* program_args, Thread* thread) {
  JClientProgramData** res = _programs.get(program_args, thread);
  if (res != nullptr) return *res;

  uint32_t program_id = Atomic::add(&_next_program_allocation_id, 1u);
  JClientProgramData* new_pd = new JClientProgramData(program_id, program_args);
  res = _programs.put_if_absent(new_pd->program_args(), new_pd, thread);
  guarantee(res != nullptr, "sanity");
  JClientProgramData* res_pd = *res;

  if (res_pd != new_pd) {
    new_pd->ref_cnt().dec();
    delete new_pd;
  }

  return res_pd;
}

bool ServerDataManager::try_remove_program(JClientArguments* program_args, Thread* thread) {
  auto eval_func = [](const JClientProgramDataKey& key, JClientProgramData* pd) -> bool {
    return pd->ref_cnt().get() == 0;
  };
  return _programs.remove_if(program_args, eval_func, thread);
}

/**
 * We can't call a java method with a lock held by this thread.
 * So we destroy the java-side data after JClientProgramDataMapEvents::on_del().
 */
void ServerDataManager::remove_java_side_program_data(uint32_t program_id, TRAPS) {
  destroy_jbooster_class_loader(program_id, THREAD);
}

/**
 * The ref_cnt of the returned JClientSessionData will +1.
 */
JClientSessionData* ServerDataManager::get_session(uint32_t session_id, Thread* thread) {
  JClientSessionData** res = _sessions.get(session_id, thread);
  if (res == nullptr) return nullptr;
  return *res;
}

/**
 * The ref_cnt of the returned JClientSessionData will +1.
 */
JClientSessionData* ServerDataManager::create_session(uint64_t client_random_id,
                                                      JClientArguments* program_args,
                                                      Thread* thread) {
  uint32_t session_id = Atomic::add(&_next_session_allocation_id, 1u);
  JClientProgramData* pd = get_or_create_program(program_args, thread);
  JClientSessionData* sd = new JClientSessionData(session_id, client_random_id, pd);

  JClientSessionData** res = _sessions.put_if_absent(session_id, sd, thread);
  guarantee(res != nullptr && *res == sd, "sanity");
  return sd;
}

bool ServerDataManager::try_remove_session(uint32_t session_id, Thread* thread) {
  auto eval_func = [](uint32_t session_id, JClientSessionData* sd) -> bool {
    return sd->ref_cnt().get() == 0;
  };
  return _sessions.remove_if(session_id, eval_func, thread);
}
