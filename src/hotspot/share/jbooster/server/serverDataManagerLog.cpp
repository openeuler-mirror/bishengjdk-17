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


#include "classfile/classLoaderData.inline.hpp"
#include "classfile/javaClasses.hpp"
#include "jbooster/jBoosterManager.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/utilities/concurrentHashMap.inline.hpp"
#include "memory/allocation.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/globals.hpp"
#include "utilities/growableArray.hpp"

// log mark of ServerDataManager
#define OUT_0(format, args...) tty->print_cr("" format, ##args)
#define OUT_1(format, args...) tty->print_cr("  " format, ##args)
#define OUT_2(format, args...) tty->print_cr("    " format, ##args)
#define OUT_3(format, args...) tty->print_cr("      " format, ##args)
#define OUT_4(format, args...) tty->print_cr("        " format, ##args)
#define OUT_5(format, args...) tty->print_cr("          " format, ##args)

class LogClassLoaderNodeIterator: public StackObj {
  bool _print_details;

public:
  LogClassLoaderNodeIterator(bool print_details): _print_details(print_details) {}

  bool operator () (JClientProgramData::ClassLoaderTable::KVNode* p) {
    const ClassLoaderKey& key = p->key();
    ClassLoaderData* cld = p->value();

    ResourceMark rm;
    Symbol* s1 = key.loader_class_name();
    Symbol* s2 = key.first_loaded_class_name();

    OUT_3("-");

    int cnt = 0;
    for (Klass* loaded_k = cld->klasses(); loaded_k != nullptr; loaded_k = loaded_k->next_link()) {
      guarantee(loaded_k->class_loader_data() == cld, "sanity");
      ++cnt;
    }

    if (cld->is_builtin_class_loader_data()) {
      OUT_4("loader_name: %s", cld->loader_name());
      OUT_4("loaded_class_cnt: %d", cnt);
      return true;
    } else {
      OUT_4("client_loader_class: %s", s1 == nullptr ? "<null>" : s1->as_C_string());
      OUT_4("first_loaded_class: %s", s2 == nullptr ? "<null>" : s2->as_C_string());
      OUT_4("parent: %s", ClassLoaderData::class_loader_data_or_null(
                            java_lang_ClassLoader::parent(cld->class_loader()))->loader_name());
      OUT_4("loaded_class_cnt: %d", cnt);
      if (_print_details && cnt > 0) {
        OUT_4("loaded_classes:");
        for (Klass* loaded_k = cld->klasses(); loaded_k != nullptr; loaded_k = loaded_k->next_link()) {
          guarantee(loaded_k->class_loader_data() == cld, "sanity");
          OUT_5("class_name: %s", loaded_k->internal_name());
        }
      }
    }
    return true;
  }
};

class LogJClientProgramDataIterator: public StackObj {
  bool _print_details;

public:
  LogJClientProgramDataIterator(bool print_details): _print_details(print_details) {}

  bool operator () (ServerDataManager::JClientProgramDataMap::KVNode* kv_node) {
    JClientProgramData* pd = kv_node->value();
    JClientCacheState& clr = pd->clr_cache_state();
    JClientCacheState& agg_cds = pd->agg_cds_cache_state();
    JClientCacheState& dy_cds = pd->dy_cds_cache_state();
    JClientCacheState& aot_static = pd->aot_static_cache_state();
    JClientCacheState& aot_pgo = pd->aot_pgo_cache_state();
    OUT_1("-");
    OUT_2("program_id: %u", pd->program_id());
    OUT_2("program_name: %s", pd->program_args()->program_name());
    OUT_2("program_hash: %x", pd->program_args()->hash());
    OUT_2("ref_cnt: %d", pd->ref_cnt().get());
    OUT_2("clr_cache: %s", clr.cache_state_str());
    OUT_2("dy_cds_cache: %s", dy_cds.cache_state_str());
    OUT_2("agg_cds_cache: %s", agg_cds.cache_state_str());
    OUT_2("aot_static_cache: %s", aot_static.cache_state_str());
    OUT_2("aot_pgo_cache: %s", aot_pgo.cache_state_str());
    OUT_2("class_loader_size: " SIZE_FORMAT, pd->class_loaders()->size());
    if (pd->class_loaders()->size() > 0) {
      OUT_2("class_loaders:");
      LogClassLoaderNodeIterator ci(_print_details);
      pd->class_loaders()->for_each(ci, Thread::current());
    }
    return true;
  }
};

class LogJClientSessionDataIterator: public StackObj {
public:
  bool operator () (ServerDataManager::JClientSessionDataMap::KVNode* kv_node) {
    JClientSessionData* sd = kv_node->value();
    OUT_1("-");
    OUT_2("random_id: " UINT64_FORMAT, sd->random_id());
    OUT_2("session_id: %u", sd->session_id());
    OUT_2("program_id: %u", sd->program_data()->program_id());
    OUT_2("ref_cnt: %d", sd->ref_cnt().get());
    return true;
  }
};

void ServerDataManager::log_all_state(bool print_details) {
  Thread* THREAD = Thread::current();
  ResourceMark rm(THREAD);
  LogJClientSessionDataIterator ri;
  LogJClientProgramDataIterator pi(print_details);

  OUT_0("JClientSessionData:");
  _sessions.for_each(ri, THREAD);
  OUT_0("");

  OUT_0("JClientProgramData:");
  _programs.for_each(pi, THREAD);
  OUT_0("");
}
