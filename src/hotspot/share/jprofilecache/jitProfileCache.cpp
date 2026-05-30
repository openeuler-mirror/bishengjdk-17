/*
* Copyright (c) 2025, Huawei Technologies Co., Ltd. All rights reserved.
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

#include "precompiled.hpp"
#include "classfile/classLoaderData.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "compiler/compileBroker.hpp"
#include "jprofilecache/jitProfileCache.hpp"
#include "jprofilecache/jitProfileRecord.hpp"
#include "jprofilecache/jitProfileCacheFileParser.hpp"
#include "jprofilecache/jitProfileCacheUtils.hpp"
#include "logging/log.hpp"
#include "logging/logStream.hpp"
#include "oops/method.hpp"
#include "oops/typeArrayKlass.hpp"
#include "runtime/arguments.hpp"
#include "compiler/compilationPolicy.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/os.hpp"
#include "runtime/thread.hpp"
#include "utilities/hashtable.inline.hpp"
#include "utilities/stack.hpp"
#include "utilities/stack.inline.hpp"
#include "runtime/atomic.hpp"
#include "libadt/dict.hpp"

JitProfileCache*                JitProfileCache::_jit_profile_cache_instance         = nullptr;

JitProfileCache::JitProfileCache()
  : profilecacheComplete(false),
    _jit_profile_cache_state(NOT_INIT),
    _jit_profile_cache_version(JITPROFILECACHE_VERSION),
    _dummy_method(nullptr),
    _jit_profile_cache_recorder(nullptr),
    _jit_profile_cache_info(nullptr),
    _excluding_matcher(nullptr) {}

JitProfileCache::~JitProfileCache() {
  delete _jit_profile_cache_recorder;
  delete _jit_profile_cache_info;
}

JitProfileCache* JitProfileCache::create_instance() {
  _jit_profile_cache_instance = new JitProfileCache();
  return _jit_profile_cache_instance;
}

JitProfileCache::JitProfileCacheState JitProfileCache::init_for_recording() {
  assert(JProfilingCacheRecording && !JProfilingCacheCompileAdvance, " JitProfileCache JVM option verify failure");
  _jit_profile_cache_recorder = new JitProfileRecorder();
  _jit_profile_cache_recorder->init();

  // check state
  if (_jit_profile_cache_recorder->is_valid()) {
    _jit_profile_cache_state = JitProfileCache::IS_OK;
  } else  {
    _jit_profile_cache_state = JitProfileCache::IS_ERR;
  }
  return _jit_profile_cache_state;
}

JitProfileCache::JitProfileCacheState JitProfileCache::init_for_profilecache() {
  if (CompilationProfileCacheExclude != nullptr) {
    _excluding_matcher = new SymbolRegexMatcher<mtClass>(CompilationProfileCacheExclude);
  }

  _jit_profile_cache_info = new JitProfileCacheInfo();
  _jit_profile_cache_info->set_holder(this);
  _jit_profile_cache_info->init();
  if (_jit_profile_cache_info->is_valid()) {
      _jit_profile_cache_state = JitProfileCache::IS_OK;
  } else  {
      _jit_profile_cache_state = JitProfileCache::IS_ERR;
  }
  return _jit_profile_cache_state;
}

void JitProfileCache::init() {
  if (JProfilingCacheCompileAdvance) {
    init_for_profilecache();
  } else if(JProfilingCacheRecording) {
    init_for_recording();
  }
  if ((JProfilingCacheRecording || JProfilingCacheCompileAdvance) && !JitProfileCache::is_valid()) {
    log_error(jprofilecache)("[JitProfileCache] ERROR: JProfileCache init error.");
    vm_exit(-1);
  }
}

JitProfileCache::JitProfileCacheState JitProfileCache::flush_recorder() {
  if(_jit_profile_cache_state == IS_ERR) {
    return _jit_profile_cache_state;
  }
  if (!_jit_profile_cache_recorder->flush_record()) {
    return IS_ERR;
  }
  if (!_jit_profile_cache_recorder->is_valid()) {
    _jit_profile_cache_state = IS_ERR;
    return _jit_profile_cache_state;
  }
  _jit_profile_cache_state = IS_OK;
  return _jit_profile_cache_state;
}

#define PRELOAD_CLASS_HS_SIZE    10240

JitProfileCacheInfo::JitProfileCacheInfo()
  : _jit_profile_cache_dict(nullptr),
    _profile_cache_chain(nullptr),
    _method_loaded_count(0),
    _state(NOT_INIT),
    _holder(nullptr),
    _jvm_booted_is_done(false) {
}

JitProfileCacheInfo::~JitProfileCacheInfo() {
  delete _jit_profile_cache_dict;
  delete _profile_cache_chain;
}

void JitProfileCacheInfo::jvm_booted_is_done() {
  _jvm_booted_is_done = true;
  ProfileCacheClassChain* chain = this->chain();
  assert(chain != nullptr, "ProfileCacheClassChain is nullptr");
}

void JitProfileCacheInfo::notify_precompilation() {
  ProfileCacheClassChain *chain = this->chain();
  assert(chain != nullptr, "ProfileCacheClassChain is nullptr");
  chain->try_transition_to_state(ProfileCacheClassChain::PRE_PROFILECACHE);

  // preload class
  log_info(jprofilecache)("JProfileCache [INFO]: start preload class from constant pool");
  chain->preload_class_in_constantpool();

  // precompile cache method
  log_info(jprofilecache)("JProfileCache [INFO]: start profilecache compilation");
  chain->precompilation();
  Thread *THREAD = Thread::current();
  if (HAS_PENDING_EXCEPTION) {
    return;
  }

  if (!chain->try_transition_to_state(ProfileCacheClassChain::PROFILECACHE_DONE)) {
    log_error(jprofilecache)("JProfileCache [ERROR]: can not change state to PROFILECACHE_DONE");
  } else {
    log_info(jprofilecache)("JProfileCache [INFO]: profilecache compilation is done");
  }
}

bool JitProfileCacheInfo::should_preload_class(Symbol* s) {
  if (UseJProfilingCacheSystemBlackList &&
      JitProfileCacheUtils::is_in_unpreloadable_classes_black_list(s)) {
    return false;
  }
  SymbolRegexMatcher<mtClass>* matcher = holder()->excluding_matcher();
  if (matcher != nullptr && matcher->matches(s)) {
    return false;
  }
  int hash = s->identity_hash();
  ProfileCacheClassEntry* e = jit_profile_cache_dict()->find_head_entry(hash, s);
  if (e == nullptr) {
    return false;
  }
  if (!CompilationProfileCacheResolveClassEagerly) {
    int offset = e->chain_offset();
    ProfileCacheClassChain::ProfileCacheClassChainEntry* entry = chain()->at(offset);
    return entry->is_not_loaded();
  } else {
    return true;
  }
}

bool JitProfileCacheInfo::resolve_loaded_klass(InstanceKlass* k) {
  if (k == nullptr) { return false; }
  if (k->is_jprofilecache_recorded()) {
    return false;
  }
  {
    MutexLocker mu(ProfileCacheClassChain_lock);
    if (!chain()->can_record_class()) {
      return false;
    }
  }
  k->set_jprofilecache_recorded(true);
  chain()->mark_loaded_class(k, jit_profile_cache_dict()->find_entry(k));
  return true;
}

class RandomFileStreamGuard : StackObj {
public:
  RandomFileStreamGuard(randomAccessFileStream* fs)
    : _file_stream(fs) {
  }

  ~RandomFileStreamGuard() { delete _file_stream; }

  randomAccessFileStream* operator ->() const { return _file_stream; }
  randomAccessFileStream* operator ()() const { return _file_stream; }

private:
  randomAccessFileStream*  _file_stream;
};

#define MAX_DEOPT_NUMBER 500

void JitProfileCacheInfo::init() {
  if (JProfilingCacheRecording) {
    log_error(jprofilecache)("[JitProfileCache] ERROR: you can not set both JProfilingCacheCompileAdvance and JProfilingCacheRecording");
    _state = IS_ERR;
    return;
  }

  _jit_profile_cache_dict = new JProfileCacheClassDictionary(PRELOAD_CLASS_HS_SIZE);
  // initialization parameters
  _method_loaded_count = 0;
  _state = IS_OK;

  if (JProfilingCacheAutoArchiveDir != nullptr) {
    ProfilingCacheFile = JitProfileRecorder::auto_jpcfile_name();
  }

  if (ProfilingCacheFile == nullptr) {
    _state = IS_ERR;
    return;
  }

  RandomFileStreamGuard fsg(new (ResourceObj::C_HEAP, mtInternal) randomAccessFileStream(
    ProfilingCacheFile, "rb"));
  JitProfileCacheFileParser parser(fsg(), this);
  if (!fsg->is_open()) {
    log_error(jprofilecache)("[JitProfileCache] ERROR : JitProfile doesn't exist");
    _state = IS_ERR;
    return;
  }
  parser.set_file_size(fsg->fileSize());

  // parse header
  if (!parser.parse_header()) {
    _state = IS_ERR;
    return;
  }

  // parse class
  if (!parser.parse_class()) {
    _state = IS_ERR;
    return;
  }

  // parse method
  while (parser.has_next_method_record()) {
    ProfileCacheMethodHold* holder = parser.parse_method();
    if (holder != nullptr) {
      // count method parse successfully
      ++_method_loaded_count;
    }
    parser.increment_parsed_number_count();
  }
  log_info(jprofilecache)("JProfileCache [INFO]: parsed method number %d successful loaded %" PRIu64, parser.parsed_methods(), _method_loaded_count);
}
