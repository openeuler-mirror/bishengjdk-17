/*
 * Copyright (c) 2020, 2024, Huawei Technologies Co., Ltd. All rights reserved.
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
#include "jbolt/jBoltCallGraph.hpp"
#include "jfr/utilities/jfrAllocation.hpp"
#include "jfr/support/jfrMethodLookup.hpp"
#include "utilities/defaultStream.hpp"
#include "oops/method.inline.hpp"
#include "logging/log.hpp"
#include "logging/logStream.hpp"

#define PAGE_SIZE os::vm_page_size()

static GrowableArray<JBoltCluster>* _clusters = NULL;
static GrowableArray<JBoltCall>* _calls = NULL;
static GrowableArray<JBoltFunc>* _funcs = NULL;

// (JBolt hfsort optional)sort final clusters by density
static const bool _jbolt_density_sort = false;
// (JBolt hfsort optional)freeze merging while exceeding pagesize
static const bool _jbolt_merge_frozen = false;

void JBoltCallGraph::initialize() {
  ::_clusters = JBoltCallGraph::callgraph_instance().callgraph_clusters();
  ::_calls = JBoltCallGraph::callgraph_instance().callgraph_calls();
  ::_funcs = JBoltCallGraph::callgraph_instance().callgraph_funcs();
}

void JBoltCallGraph::deinitialize() {
  ::_clusters = NULL;
  ::_calls = NULL;
  ::_funcs = NULL;
}

int JBoltCallGraph::clear_instance() {
  delete _clusters;
  delete _calls;
  delete _funcs;

  // Reinit default cluster start id
  _init_cluster_id = 0;

  // Re-allocate
  _clusters = create_growable_array<JBoltCluster>();
  _calls = create_growable_array<JBoltCall>();
  _funcs = create_growable_array<JBoltFunc>();

  // Re-initialize
  initialize();

  return 0;
}

static GrowableArray<JBoltCluster>* clusters_copy() {
  GrowableArray<JBoltCluster>* copy = create_growable_array<JBoltCluster>(_clusters->length());
  copy->appendAll(_clusters);
  return copy;
}

static GrowableArray<JBoltFunc>* funcs_copy() {
  GrowableArray<JBoltFunc>* copy = create_growable_array<JBoltFunc>(_funcs->length());
  copy->appendAll(_funcs);
  return copy;
}

static int find_func_index(const JBoltFunc* func) {
  for (int i = 0; i < _funcs->length(); ++i) {
    JBoltFunc& existing = _funcs->at(i);
    if (existing == (*func)) {
      return i;
    }
  }
  return -1;
}

// Searching for a cluster with corresponding func or creating a new one if doesn't exist
static JBoltCluster* find_cluster(JBoltFunc* func) {
  for (int i = 0; i < _clusters->length(); ++i) {
    JBoltCluster& cluster = _clusters->at(i);
    int index = cluster.func_indexes()->at(0);
    if (_funcs->at(index) == (*func)) {
      return &cluster;
    }
  }
  _funcs->append(*func);
  _clusters->append(JBoltCluster(*func));
  JBoltCluster& cluster = _clusters->at(_clusters->length() - 1);
  _funcs->at(_funcs->length() - 1).set_cluster_id(cluster.id());
  return &cluster;
}

// Creating a new call in graph or updating the weight if exists
static void add_call_to_calls(GrowableArray<JBoltCall>* calls, const JBoltCall* call) {
  for (int i = 0; i < calls->length(); ++i) {
    JBoltCall& existing_call = calls->at(i);
    if (existing_call == *call) {
      if (existing_call.stacktrace_id() == call->stacktrace_id()) {
        assert(call->call_count() > existing_call.call_count(), "invariant");
        existing_call.callee().add_heat(call->call_count() - existing_call.call_count());
        existing_call.set_call_count(call->call_count());
      }
      else {
        existing_call.callee().add_heat(call->call_count());
        existing_call.set_call_count(existing_call.call_count() + call->call_count());
      }
      return;
    }
  }

  calls->append(*call);
  call->callee().add_heat(call->call_count());
  call->callee().append_call_index(calls->length() - 1);
}

// Getting final funcs order from an array of processed clusters
static GrowableArray<JBoltFunc>* clusters_to_funcs_order(GrowableArray<JBoltCluster>* clusters) {
  log_debug(jbolt)( "sorted clusters:\n");
  for (int i = 0; i < clusters->length(); ++i) {
    log_debug(jbolt)( "cluster id: %d heats: %ld size: %dB density: %f\n", clusters->at(i).id(), clusters->at(i).heats(), clusters->at(i).size(), clusters->at(i).density());
    for (int j = 0; j < clusters->at(i).get_funcs_count(); ++j) {
        JBoltFunc& func = _funcs->at(clusters->at(i).func_indexes()->at(j));
        const Method* const method = JfrMethodLookup::lookup(func.klass(), func.method_id());
        if (method != NULL) {
          log_debug(jbolt)( "%d: method signature:%s heat: %ld size: %dB\n",
            j, method->external_name(), func.heat(), func.size());
        }
    }
  }

  GrowableArray<JBoltFunc>* order = create_growable_array<JBoltFunc>(_funcs->length());
  // used to seperator distinct cluster, klass = NULL
  JBoltFunc seperator_func;
  order->append(seperator_func);
  for (int i = 0; i < clusters->length(); ++i) {
    JBoltCluster& cluster = clusters->at(i);
    GrowableArray<int>* func_indexes = cluster.func_indexes();

    for (int j = 0; j < func_indexes->length(); ++j) {
      int index = func_indexes->at(j);
      order->append(_funcs->at(index));
    }

    order->append(seperator_func);
  }
  return order;
}

// Comparing function needed to sort an array of funcs by their weights (in decreasing order)
static int func_comparator(JBoltFunc* func1, JBoltFunc* func2) {
  return func1->heat() < func2->heat();
}

// Comparing cluster needed to sort an array of clusters by their densities (in decreasing order)
static int cluster_comparator(JBoltCluster* cluster1, JBoltCluster* cluster2) {
  return _jbolt_density_sort ? (cluster1->density() < cluster2->density()) : (cluster1->heats() < cluster2 -> heats());
}

// Comparing call indexes needed to sort an array of call indexes by their call counts (in decreasing order)
static int func_call_indexes_comparator(int* index1, int* index2) {
  return _calls->at(*index1).call_count() < _calls->at(*index2).call_count();
}

JBoltCallGraph& JBoltCallGraph::callgraph_instance() {
  static JBoltCallGraph _call_graph;
  return _call_graph;
}

void JBoltCallGraph::add_func(JBoltFunc* func) {
  if (!(UseJBolt && JBoltManager::reorder_phase_is_profiling_or_waiting())) return;
  JBoltCluster* cluster = find_cluster(func);
  assert(cluster != NULL, "invariant");
}

void JBoltCallGraph::add_call(JBoltCall* call) {
  if (!(UseJBolt && JBoltManager::reorder_phase_is_profiling_or_waiting())) return;
  add_call_to_calls(_calls, call);
}

uintptr_t data_layout_jbolt[] = {
  (uintptr_t)in_bytes(JBoltCluster::id_offset()),
  (uintptr_t)in_bytes(JBoltCluster::heats_offset()),
  (uintptr_t)in_bytes(JBoltCluster::frozen_offset()),
  (uintptr_t)in_bytes(JBoltCluster::size_offset()),
  (uintptr_t)in_bytes(JBoltCluster::density_offset()),
  (uintptr_t)in_bytes(JBoltCluster::func_indexes_offset()),

  (uintptr_t)in_bytes(GrowableArrayView<address>::data_offset()),

  (uintptr_t)JBoltCluster::find_cluster_by_id,
  (uintptr_t)_jbolt_merge_frozen
};

static void deal_with_each_func(GrowableArray<JBoltCluster>* clusters, GrowableArray<JBoltFunc>* funcs, GrowableArray<int>* merged) {
  for (int i = 0; i < funcs->length(); ++i) {
    JBoltFunc& func = funcs->at(i);

    JBoltCluster* cluster = JBoltCluster::find_cluster_by_id(clusters, func.cluster_id());

    // for cluster size larger than page size, should be frozen and don't merge with any cluster
    if (_jbolt_merge_frozen && cluster->frozen()) continue;

    // find best predecessor
    func.call_indexes()->sort(&func_call_indexes_comparator);

    int bestPred = -1;

    for (int j = 0; j < func.call_indexes()->length(); ++j) {
      const JBoltCall& call = _calls->at(func.call_indexes()->at(j));

      bestPred = os::Linux::jboltMerge_judge(data_layout_jbolt, call.caller().cluster_id(), (address)clusters, (address)merged, (address)cluster);

      if (bestPred == -1) continue;

      break;
    }

    // not merge -- no suitable caller nodes
    if (bestPred == -1) {
      continue;
    }

    JBoltCluster* predCluster = JBoltCluster::find_cluster_by_id(clusters, bestPred);

    // merge callee cluster to caller cluster
    for (int j = 0; j < cluster->func_indexes()->length(); ++j) {
      int index = cluster->func_indexes()->at(j);
      predCluster->append_func_index(index);
    }
    predCluster->add_heat(cluster->heats());
    predCluster->add_size(cluster->size());
    predCluster->update_density();
    merged->at(cluster->id()) = bestPred;
    cluster->clear();
  }
}

// Every node is a cluster with funcs
// Initially each cluster has only one func inside
GrowableArray<JBoltFunc>* JBoltCallGraph::hfsort() {
  if (!(UseJBolt && (JBoltDumpMode || JBoltManager::auto_mode()))) return NULL;
  log_debug(jbolt)( "hfsort begin...\n");
  // Copies are needed for saving initial graph in memory
  GrowableArray<JBoltCluster>* clusters = clusters_copy();
  GrowableArray<JBoltFunc>* funcs = funcs_copy();

  // store a map for finding head of merge chain
  GrowableArray<int>* merged = create_growable_array<int>(clusters->length());
  for (int i = 0; i < clusters->length(); ++i) {
    merged->append(-1);
  }

  // sorted by func(initially a node) weight(now just as 'heat')
  funcs->sort(&func_comparator);

  // Process each function, and consider merging its cluster with the
  // one containing its most likely predecessor.
  deal_with_each_func(clusters, funcs, merged);

  // the set of clusters that are left
  GrowableArray<JBoltCluster>* sortedClusters = create_growable_array<JBoltCluster>();
  for (int i = 0; i < clusters->length(); ++i) {
    if (clusters->at(i).id() != -1) {
      sortedClusters->append(clusters->at(i));
    }
  }

  sortedClusters->sort(&cluster_comparator);

  GrowableArray<JBoltFunc>* order = clusters_to_funcs_order(sortedClusters);

  delete clusters;
  delete funcs;
  delete merged;
  delete sortedClusters;
  log_debug(jbolt)( "hfsort over...\n");

  return order;
}

JBoltFunc::JBoltFunc() :
  _klass(NULL),
  _method_id(0),
  _heat(0),
  _size(0),
  _cluster_id(-1),
  _method_key(),
  _call_indexes(create_growable_array<int>()) {}

JBoltFunc::JBoltFunc(const JBoltFunc& func) :
  _klass(func._klass),
  _method_id(func._method_id),
  _heat(func._heat),
  _size(func._size),
  _cluster_id(func._cluster_id),
  _method_key(func._method_key),
  _call_indexes(create_growable_array<int>(func.get_calls_count())) {
    GrowableArray<int>* array = func.call_indexes();
    _call_indexes->appendAll(array);
  }

JBoltFunc::JBoltFunc(const InstanceKlass* klass, traceid method_id, int size, JBoltMethodKey method_key) :
  _klass(klass),
  _method_id(method_id),
  _heat(0),
  _size(size),
  _cluster_id(-1),
  _method_key(method_key),
  _call_indexes(create_growable_array<int>()) {
    // not new_symbol, need to inc reference cnt
    _method_key.klass()->increment_refcount();
    _method_key.name()->increment_refcount();
    _method_key.sig()->increment_refcount();
  }

void JBoltFunc::add_heat(int64_t heat)  {
  _heat += heat;
  assert(_cluster_id != -1, "invariant");
  _clusters->at(_cluster_id).add_heat(heat);
  _clusters->at(_cluster_id).update_density();
}

void JBoltFunc::set_heat(int64_t heat)  {
  int64_t diff = heat - _heat;
  _heat = heat;
  assert(_cluster_id != -1, "invariant");
  _clusters->at(_cluster_id).add_heat(diff);
  _clusters->at(_cluster_id).update_density();
}

void JBoltFunc::set_cluster_id(int cluster_id)            { _cluster_id = cluster_id;                                          }

void JBoltFunc::append_call_index(int index)              { _call_indexes->append(index);                                      }

JBoltFunc* JBoltFunc::constructor(const InstanceKlass* klass, traceid method_id, int size, JBoltMethodKey method_key) {
  JBoltFunc *ret = new JBoltFunc(klass, method_id, size, method_key);
  return ret;
}

JBoltFunc* JBoltFunc::copy_constructor(const JBoltFunc* func) {
  JBoltFunc *ret = new JBoltFunc(*func);
  return ret;
}

JBoltCluster::JBoltCluster() :
  _id(-1),
  _heats(0),
  _frozen(false),
  _size(0),
  _density(0.0),
  _func_indexes(create_growable_array<int>()) {}

JBoltCluster::JBoltCluster(const JBoltFunc& func) :
  _id(_init_cluster_id++),
  _heats(func.heat()),
  _frozen(false),
  _size(func.size()),
  _density(0.0),
  _func_indexes(create_growable_array<int>()) {
    if (_size >= PAGE_SIZE)
        freeze();

    update_density();

    int func_idx = find_func_index(&func);
    assert(func_idx != -1, "invariant");
    _func_indexes->append(func_idx);
  }

JBoltCluster::JBoltCluster(const JBoltCluster& cluster) :
  _id(cluster.id()),
  _heats(cluster.heats()),
  _frozen(cluster.frozen()),
  _size(cluster.size()),
  _density(cluster.density()),
  _func_indexes(create_growable_array<int>(cluster.get_funcs_count())) {
    GrowableArray<int>* array = cluster.func_indexes();
    _func_indexes->appendAll(array);
  }

void JBoltCluster::add_heat(int64_t heat)                      { _heats += heat;                            }

void JBoltCluster::freeze()                                    { _frozen = true;                            }

void JBoltCluster::add_size(int size)                          { _size += size;                             }

void JBoltCluster::update_density()                            { _density = (double)_heats / (double)_size; }

void JBoltCluster::append_func_index(int index)                { _func_indexes->append(index);              }

void JBoltCluster::clear() {
  _id = -1;
  _heats = 0;
  _frozen = false;
  _size = 0;
  _density = 0.0;
  _func_indexes->clear();
}

// Searching for a cluster by its id
JBoltCluster* JBoltCluster::find_cluster_by_id(GrowableArray<JBoltCluster>* clusters, u4 id) {
  if (id >= (u4)clusters->length()) return NULL;

  return &(clusters->at(id));
}

JBoltCluster* JBoltCluster::constructor(const JBoltFunc* func) {
  JBoltCluster *ret = new JBoltCluster(*func);
  return ret;
}

JBoltCluster* JBoltCluster::copy_constructor(const JBoltCluster* cluster) {
  JBoltCluster *ret = new JBoltCluster(*cluster);
  return ret;
}

JBoltCall::JBoltCall() :
  _caller_index(-1),
  _callee_index(-1),
  _call_count(0),
  _stacktrace_id(0) {}

JBoltCall::JBoltCall(const JBoltCall& call) :
  _caller_index(call._caller_index),
  _callee_index(call._callee_index),
  _call_count(call._call_count),
  _stacktrace_id(call._stacktrace_id) {}

JBoltCall::JBoltCall(const JBoltFunc& caller_func, const JBoltFunc& callee_func, u4 call_count, traceid stacktrace_id) :
  _call_count(call_count),
  _stacktrace_id(stacktrace_id) {
    _caller_index = find_func_index(&caller_func);
    _callee_index = find_func_index(&callee_func);
    assert(_caller_index != -1, "invariant");
    assert(_callee_index != -1, "invariant");
  }

JBoltFunc& JBoltCall::caller() const { return _funcs->at(_caller_index); }

JBoltFunc& JBoltCall::callee() const { return _funcs->at(_callee_index); }

void JBoltCall::set_caller_index(int index)   { _caller_index = index;                 }

void JBoltCall::set_callee_index(int index)   { _callee_index = index;                 }

void JBoltCall::set_call_count(u4 call_count) { _call_count = call_count;              }

JBoltCall* JBoltCall::constructor(const JBoltFunc* caller_func, const JBoltFunc* callee_func, u4 call_count, traceid stacktrace_id) {
  JBoltCall *ret = new JBoltCall(*caller_func, *callee_func, call_count, stacktrace_id);
  return ret;
}

JBoltCall* JBoltCall::copy_constructor(const JBoltCall* call) {
  JBoltCall *ret = new JBoltCall(*call);
  return ret;
}