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

#ifndef SHARE_JBOLT_JBOLTMANAGER_HPP
#define SHARE_JBOLT_JBOLTMANAGER_HPP

#include "compiler/compileTask.hpp"
#include "jbolt/jbolt_globals.hpp"
#include "jfr/recorder/stacktrace/jfrStackTraceRepository.hpp"
#include "jfr/dcmd/jfrDcmds.hpp"
#include "memory/allocation.hpp"
#include "memory/heap.hpp"
#include "oops/symbol.hpp"
#include "runtime/handles.hpp"
#include "utilities/growableArray.hpp"
#include "utilities/resourceHash.hpp"

class CompileTask;
class CompileTaskInfo;
class Method;
class Thread;

enum JBoltErrorCode {
  JBoltOK = 0,
  JBoltOrderNULL = 1,
  JBoltOpenFileError = 2
};

struct JBoltReorderPhase {
  static const int Waiting    = -1; // JBolt logic is waiting for something to be done.
  static const int Available  = 0;  // JBolt logic is not working or is done (can be reordered again now).
  static const int Collecting = 1;  // Collecting methods in the order file (this phase is for two-phase only).
  static const int Profiling  = 2;  // JFR is working (this phase is for one-phase only).
  static const int Reordering = 3;  // Recompiling and re-laying.
  static const int End        = 4;  // JBolt is not available anymore (for two-phase, or error happened on one-phase).
};

class JBoltMethodKey : public StackObj {
  Symbol* _klass;
  Symbol* _name;
  Symbol* _sig;

  void inc_ref_cnt() {
    Symbol* arr[] = { _klass, _name, _sig };
    for (int i = 0; i < (int) (sizeof(arr) / sizeof(arr[0])); ++i) {
      if (arr[i] != nullptr) arr[i]->increment_refcount();
    }
  }

  void dec_ref_cnt() {
    Symbol* arr[] = { _klass, _name, _sig };
    for (int i = 0; i < (int) (sizeof(arr) / sizeof(arr[0])); ++i) {
      if (arr[i] != nullptr) arr[i]->decrement_refcount();
    }
  }
public:

  JBoltMethodKey(Symbol* klass, Symbol* name, Symbol* sig): _klass(klass), _name(name), _sig(sig) { /* no inc_ref_cnt() here for SymbolTable::new_symbol() */ }
  JBoltMethodKey(Method* method): _klass(method->method_holder()->name()), _name(method->name()), _sig(method->signature()) { inc_ref_cnt(); }
  JBoltMethodKey(const JBoltMethodKey& other): _klass(other._klass), _name(other._name), _sig(other._sig) { inc_ref_cnt(); }
  JBoltMethodKey(): _klass(nullptr), _name(nullptr), _sig(nullptr) {}
  ~JBoltMethodKey() { dec_ref_cnt(); }

  JBoltMethodKey& operator = (const JBoltMethodKey& other) {
    dec_ref_cnt();
    _klass = other._klass;
    _name = other._name;
    _sig = other._sig;
    inc_ref_cnt();
    return *this;
  }

  unsigned hash() const {
    int v = primitive_hash(_klass);
    v = v * 31 + primitive_hash(_name);
    v = v * 31 + primitive_hash(_sig);
    return v;
  }
  bool equals(const JBoltMethodKey& other) const {
    return _klass == other._klass && _name == other._name && _sig == other._sig;
  }

  static unsigned calc_hash(const JBoltMethodKey& k) {
    return k.hash();
  }
  static bool calc_equals(const JBoltMethodKey& k1, const JBoltMethodKey& k2) {
    return k1.equals(k2);
  }

  Symbol* klass() const    { return _klass;     }
  Symbol* name() const     { return _name;      }
  Symbol* sig() const      { return _sig;       }
};

class JBoltMethodValue : public StackObj {
private:
  CompileTaskInfo* volatile _comp_info;

public:
  JBoltMethodValue(): _comp_info(nullptr) {}
  ~JBoltMethodValue();

  CompileTaskInfo* get_comp_info();
  bool set_comp_info(CompileTaskInfo* info);
  void clear_comp_info_but_not_release();
};

class CompileTaskInfo : public CHeapObj<mtCompiler> {
  Method* const _method;
  jobject _method_holder;
  const int _osr_bci;
  const int _comp_level;
  const int _comp_reason;
  Method* const _hot_method;
  jobject _hot_method_holder;
  const int _hot_count;

public:
  CompileTaskInfo(Method* method, int osr_bci, int comp_level, int comp_reason, Method* hot_method, int hot_cnt);
  ~CompileTaskInfo();

  bool try_select();

  Method* method() const { return _method; }
  int osr_bci() const { return _osr_bci; }
  int comp_level() const { return _comp_level; }
  int comp_reason() const { return _comp_reason; }
  Method* hot_method() const { return _hot_method; }
  int hot_count() const { return _hot_count; }
};

class JBoltStackFrameKey : public StackObj {
  InstanceKlass* _klass;
  traceid _methodid;

public:
  JBoltStackFrameKey(InstanceKlass* klass, traceid methodid): _klass(klass), _methodid(methodid) {}
  JBoltStackFrameKey(const JBoltStackFrameKey& other): _klass(other._klass), _methodid(other._methodid) {}
  JBoltStackFrameKey(): _klass(NULL), _methodid(0) {}
  ~JBoltStackFrameKey() { /* nothing to do as _klass is a softcopy of JfrStackFrame::_klass */ }


  JBoltStackFrameKey& operator = (const JBoltStackFrameKey& other) {
    _klass = other._klass;
    _methodid = other._methodid;
    return *this;
  }

  unsigned hash() const {
    int v = primitive_hash(_klass);
    v = v * 31 + primitive_hash(_methodid);
    return v;
  }

  bool equals(const JBoltStackFrameKey& other) const {
    return _klass == other._klass && _methodid == other._methodid;
  }

  static unsigned calc_hash(const JBoltStackFrameKey& k) {
    return k.hash();
  }

  static bool calc_equals(const JBoltStackFrameKey& k1, const JBoltStackFrameKey& k2) {
    return k1.equals(k2);
  }
};

class JBoltStackFrameValue : public StackObj {
private:
  jobject _method_holder;

public:
  JBoltStackFrameValue(jobject method_holder): _method_holder(method_holder) {}
  ~JBoltStackFrameValue();

  jobject get_method_holder();
  void clear_method_holder_but_not_release();
};

class JBoltManager : public AllStatic {
  friend class JBoltControlThread;

  typedef ResourceHashtable<const JBoltMethodKey, JBoltMethodValue,
                            JBoltMethodKey::calc_hash, JBoltMethodKey::calc_equals,
                            15889, ResourceObj::C_HEAP, mtCompiler> MethodKeyMap;

  typedef ResourceHashtable<const JBoltStackFrameKey, JBoltStackFrameValue,
                            JBoltStackFrameKey::calc_hash, JBoltStackFrameKey::calc_equals,
                            15889, ResourceObj::C_HEAP, mtTracing> StackFrameKeyMap;

  static GrowableArray<JBoltMethodKey>* _hot_methods_sorted;
  static MethodKeyMap* _hot_methods_vis;
  static int _reorder_method_threshold_cnt;

  static volatile int _reorder_phase;
  static volatile int _reorderable_method_cnt;
  static Method* volatile _cur_reordering_method;

  // the CompilerThread to start the new JBoltControlThread
  static Thread* _start_reordering_thread;

  static StackFrameKeyMap* _sampled_methods_refs;

  // when not set JBoltDumpMode or JBoltLoadMode, JBolt will be in one-step auto mode.
  static bool _auto_mode;

  // use MethodJBoltHot and MethodJBoltTmp as two semi hot space.
  // each time restart a schedule, we exchange primary and secondary
  static volatile int _primary_hot_seg;
  static volatile int _secondary_hot_seg;

private:
  // Used in dump mode.
  static methodHandle lookup_method(InstanceKlass* klass, traceid method_id);
  static void construct_stacktrace(const JfrStackTrace &stacktrace);

  // Used in init phase 1.
  static void check_mode();
  static void check_order_file();
  static void check_dependency();
  static size_t calc_nmethod_size_with_padding(size_t nmethod_size);
  static size_t calc_segment_size_with_padding(size_t segment_size);
  static void load_order_file_phase1(int* method_cnt , size_t* total_nmethod_size);
  static void init_load_mode_phase1();

  // Used in init phase 2.
  static bool parse_method_line_phase2(char* const line, const int len);
  static bool parse_connected_component_line_phase2(char* const line, const int len);
  static void load_order_file_phase2(TRAPS);
  static void init_load_mode_phase2(TRAPS);
  static void init_dump_mode_phase2(TRAPS);

  // Used in auto mode.
  static int primary_hot_seg();
  static int secondary_hot_seg();

  // Used in auto mode prev_control_schedule
  static int clear_last_sample_datas();
  static void swap_semi_jbolt_segs();
  static int clear_manager();

  // Used in auto mode control_schedule
  static void init_auto_transition(size_t* segment_size, TRAPS);

  // Used in auto mode post_control_schedule
  static void clear_secondary_hot_seg(TRAPS);

  // JBolt phases

  static int reorder_phase();

  static bool reorder_phase_available_to_collecting();
  static bool reorder_phase_collecting_to_reordering();

  static bool reorder_phase_available_to_profiling();
  static bool reorder_phase_profiling_to_reordering();
  static bool reorder_phase_reordering_to_available();
  static bool reorder_phase_profiling_to_available();
  static bool reorder_phase_profiling_to_waiting();
  static bool reorder_phase_waiting_to_reordering();
  static bool reorder_phase_waiting_to_available();

  static bool reorder_phase_reordering_to_end();

  static Method* cur_reordering_method();
  static void set_cur_reordering_method(Method* method);
  static int inc_reorderable_method_cnt();

  // Used in reordering phase.
  static CompileTask* create_a_task_instance(CompileTaskInfo* cti, methodHandle& method, methodHandle& hot_method, TRAPS);
  static void check_compiled_result(Method* method, int check_blob_type, TRAPS);
  static bool enqueue_recompile_task(CompileTaskInfo* cti, methodHandle& method, methodHandle& hot_method, TRAPS);
  static bool recompile_one(CompileTaskInfo* cti, methodHandle& method, methodHandle& hot_method, TRAPS);

  static void print_code_heap(outputStream& ls, CodeHeap* heap, const char* name);
public:
  static void log_stacktrace(const JfrStackTrace &stacktrace);
  static void construct_cg_once();
  static void dump_order_in_manual();
  static JBoltErrorCode dump_order_in_jcmd(const char* filename);

  static void check_arguments_not_set();
  static void init_phase1();
  static void init_phase2(TRAPS);
  static void init_code_heaps(size_t non_nmethod_size, size_t profiled_size, size_t non_profiled_size, size_t cache_size, size_t ps, size_t alignment);

  static bool auto_mode() { return _auto_mode; }

  static bool reorder_phase_is_waiting();
  static bool reorder_phase_is_available();
  static bool reorder_phase_is_collecting();
  static bool reorder_phase_is_profiling();
  static bool reorder_phase_is_reordering();
  static bool reorder_phase_is_profiling_or_waiting();
  static bool reorder_phase_is_collecting_or_reordering();

  static bool can_reorder_now();
  static bool should_reorder_now();

  static int calc_code_blob_type(Method* method, CompileTask* task, TRAPS);

  static void check_start_reordering(TRAPS);
  static void reorder_all_methods(TRAPS);
  static void clear_structures();

  static void print_code_heaps();
};

#endif // SHARE_JBOLT_JBOLTMANAGER_HPP
