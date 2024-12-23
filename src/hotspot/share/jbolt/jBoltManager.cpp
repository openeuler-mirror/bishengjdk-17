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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "classfile/javaClasses.inline.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/vmSymbols.hpp"
#include "code/codeBlob.hpp"
#include "code/codeCache.hpp"
#include "compiler/compileBroker.hpp"
#include "jbolt/jBoltCallGraph.hpp"
#include "jbolt/jBoltControlThread.hpp"
#include "jbolt/jBoltManager.hpp"
#include "jbolt/jBoltUtils.inline.hpp"
#include "jfr/jfr.hpp"
#include "jfr/support/jfrMethodLookup.hpp"
#include "logging/log.hpp"
#include "logging/logStream.hpp"
#include "memory/resourceArea.hpp"
#include "oops/klass.inline.hpp"
#include "oops/method.inline.hpp"
#include "runtime/arguments.hpp"
#include "runtime/atomic.hpp"
#include "runtime/globals_extension.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/jniHandles.hpp"
#include "runtime/os.hpp"
#include "runtime/safepointVerifiers.hpp"
#include "runtime/sweeper.hpp"
#include "utilities/formatBuffer.hpp"

static constexpr int LINE_BUF_SIZE = 8192;  // used to parse JBolt order file
static constexpr int MIN_FRAMESCOUNT = 2;   // used as default stacktrace depth
static constexpr int ILL_NM_STATE = -2;     // used to present nmethod illegal state

#define B_TF(b) (b ? "V" : "X")

GrowableArray<JBoltMethodKey>* JBoltManager::_hot_methods_sorted = nullptr;
JBoltManager::MethodKeyMap* JBoltManager::_hot_methods_vis = nullptr;
int JBoltManager::_reorder_method_threshold_cnt = 0;

volatile int JBoltManager::_reorder_phase = JBoltReorderPhase::Available;
volatile int JBoltManager::_reorderable_method_cnt = 0;
Method* volatile JBoltManager::_cur_reordering_method = nullptr;

Thread* JBoltManager::_start_reordering_thread = nullptr;

JBoltManager::StackFrameKeyMap* JBoltManager::_sampled_methods_refs = nullptr;

bool JBoltManager::_auto_mode = false;

// swap between MethodJBoltHot and MethodJBoltTmp
volatile int JBoltManager::_primary_hot_seg = CodeBlobType::MethodJBoltHot;
volatile int JBoltManager::_secondary_hot_seg = CodeBlobType::MethodJBoltTmp;

GrowableArray<JBoltFunc>* _order_stored = nullptr;

// This is a tmp obj used only in initialization phases.
// We cannot alloc Symbol in phase 1 so we have to parses the order file again
// in phase 2.
// This obj will be freed after initialization.
static FILE* _order_fp = nullptr;

// The threshold to trigger JBolt reorder in load mode.
static const double _jbolt_reorder_threshold = 0.8;

static bool read_line(FILE* fp, char* buf, int buf_len, int* res_len) {
  if (fgets(buf, buf_len, fp) == nullptr) {
    return false;
  }
  int len = (int) strcspn(buf, "\r\n");
  buf[len] = '\0';
  *res_len = len;
  return true;
}

static bool read_a_size(char* buf, size_t* res) {
  char* t = strchr(buf, ' ');
  if (t == nullptr) return false;
  *t = '\0';
  julong v;
  if (!Arguments::atojulong(buf, &v)) {
    *t = ' ';
    return false;
  }
  *t = ' ';
  *res = (size_t) v;
  return true;
}

static void replace_all(char* s, char from, char to) {
  char* begin = s;
  while (true) {
    char* t = strchr(begin, from);
    if (t == nullptr) {
      break;
    }
    *t = to;
    begin = t + 1;
  }
}

JBoltMethodValue::~JBoltMethodValue() {
  if (_comp_info != nullptr) delete get_comp_info();
}

CompileTaskInfo* JBoltMethodValue::get_comp_info() {
  return Atomic::load_acquire(&_comp_info);
}

bool JBoltMethodValue::set_comp_info(CompileTaskInfo* info) {
  return Atomic::cmpxchg(&_comp_info, (CompileTaskInfo*) nullptr, info) == nullptr;
}

void JBoltMethodValue::clear_comp_info_but_not_release() {
  Atomic::release_store(&_comp_info, (CompileTaskInfo*) nullptr);
}

JBoltStackFrameValue::~JBoltStackFrameValue() {
  if (_method_holder != nullptr) {
    if (JNIHandles::is_weak_global_handle(_method_holder)) {
      JNIHandles::destroy_weak_global(_method_holder);
    } else {
      JNIHandles::destroy_global(_method_holder);
    }
  }
}

jobject JBoltStackFrameValue::get_method_holder() { return _method_holder; }

void JBoltStackFrameValue::clear_method_holder_but_not_release() { _method_holder = nullptr; }

CompileTaskInfo::CompileTaskInfo(Method* method, int osr_bci, int comp_level, int comp_reason, Method* hot_method, int hot_cnt):
        _method(method), _osr_bci(osr_bci), _comp_level(comp_level), _comp_reason(comp_reason), _hot_method(hot_method), _hot_count(hot_cnt) {
  Thread* thread = Thread::current();

  assert(_method != nullptr, "sanity");
  // _method_holder can be null for boot loader (the null loader)
  _method_holder = JNIHandles::make_weak_global(Handle(thread, _method->method_holder()->klass_holder()));

  if (_hot_method != nullptr && _hot_method != _method) {
    _hot_method_holder = JNIHandles::make_weak_global(Handle(thread, _hot_method->method_holder()->klass_holder()));
  } else {
    _hot_method_holder = nullptr;
  }
}

CompileTaskInfo::~CompileTaskInfo() {
  if (_method_holder != nullptr) {
    if (JNIHandles::is_weak_global_handle(_method_holder)) {
      JNIHandles::destroy_weak_global(_method_holder);
    } else {
      JNIHandles::destroy_global(_method_holder);
    }
  }
  if (_hot_method_holder != nullptr) {
    if (JNIHandles::is_weak_global_handle(_hot_method_holder)) {
      JNIHandles::destroy_weak_global(_hot_method_holder);
    } else {
      JNIHandles::destroy_global(_hot_method_holder);
    }
  }
}

/**
 * Set the weak reference to strong reference if the method is not unloaded.
 * It seems that the life cycle of Method is consistent with that of the Klass and CLD.
 * @see CompileTask::select_for_compilation()
 */
bool CompileTaskInfo::try_select() {
  NoSafepointVerifier nsv;
  Thread* thread = Thread::current();
  // is unloaded
  if (_method_holder != nullptr && JNIHandles::is_weak_global_handle(_method_holder) && JNIHandles::is_global_weak_cleared(_method_holder)) {
    if (log_is_enabled(Debug, jbolt)) {
      log_debug(jbolt)("Some method has been unloaded so skip reordering for it: p=%p.", _method);
    }
    return false;
  }

  assert(_method->method_holder()->is_loader_alive(), "should be alive");
  Handle method_holder(thread, _method->method_holder()->klass_holder());
  JNIHandles::destroy_weak_global(_method_holder);
  _method_holder = JNIHandles::make_global(method_holder);

  if (_hot_method_holder != nullptr) {
    Handle hot_method_holder(thread, _hot_method->method_holder()->klass_holder());
    JNIHandles::destroy_weak_global(_hot_method_holder);
    _hot_method_holder = JNIHandles::make_global(Handle(thread, _hot_method->method_holder()->klass_holder()));
  }
  return true;
}

static void check_arg_not_set(JVMFlagsEnum flag) {
  if (JVMFlag::is_cmdline(flag)) {
    vm_exit_during_initialization(err_msg("Do not set VM option %s without UseJBolt enabled.",
                                          JVMFlag::flag_from_enum(flag)->name()));
  }
}

static const char *method_type_to_string(u1 type) {
  switch (type) {
    case JfrStackFrame::FRAME_INTERPRETER:
      return "Interpreted";
    case JfrStackFrame::FRAME_JIT:
      return "JIT compiled";
    case JfrStackFrame::FRAME_INLINE:
      return "Inlined";
    case JfrStackFrame::FRAME_NATIVE:
      return "Native";
    default:
      ShouldNotReachHere();
      return "Unknown";
  }
}

uintptr_t related_data_jbolt[] = {
  (uintptr_t)in_bytes(JfrStackTrace::hash_offset()),
  (uintptr_t)in_bytes(JfrStackTrace::id_offset()),
  (uintptr_t)in_bytes(JfrStackTrace::hotcount_offset()),
  (uintptr_t)in_bytes(JfrStackTrace::frames_offset()),
  (uintptr_t)in_bytes(JfrStackTrace::frames_count_offset()),

  (uintptr_t)in_bytes(JfrStackFrame::klass_offset()),
  (uintptr_t)in_bytes(JfrStackFrame::methodid_offset()),
  (uintptr_t)in_bytes(JfrStackFrame::bci_offset()),
  (uintptr_t)in_bytes(JfrStackFrame::type_offset()),

  (uintptr_t)JBoltFunc::constructor,
  (uintptr_t)JBoltFunc::copy_constructor,
  (uintptr_t)JBoltCall::constructor,
  (uintptr_t)JBoltCall::copy_constructor,
  (uintptr_t)JBoltCallGraph::static_add_func,
  (uintptr_t)JBoltCallGraph::static_add_call
};

/**
 * Invoked in JfrStackTraceRepository::add_jbolt().
 * Each time JFR record a valid stacktrace,
 * we log a weak ptr of each unique method in _sampled_methods_refs.
 */
void JBoltManager::log_stacktrace(const JfrStackTrace& stacktrace) {
  Thread* thread = Thread::current();
  HandleMark hm(thread);

  const JfrStackFrame* frames = stacktrace.get_frames();
  unsigned int framesCount = stacktrace.get_framesCount();

  for (u4 i = 0; i < framesCount; ++i) {
    const JfrStackFrame& frame = frames[i];

    JBoltStackFrameKey stackframe_key(const_cast<InstanceKlass *>(frame.get_klass()), frame.get_methodId());

    if (!_sampled_methods_refs->contains(stackframe_key)) {
      jobject method_holder = JNIHandles::make_weak_global(Handle(thread, frame.get_klass()->klass_holder()));
      JBoltStackFrameValue stackframe_value(method_holder);
      _sampled_methods_refs->put(stackframe_key, stackframe_value);
      // put() transmits method_holder ownership to element in map
      // set the method_holder to nullptr in temp variable stackframe_value, to avoid double free
      stackframe_value.clear_method_holder_but_not_release();
    }
  }
}

methodHandle JBoltManager::lookup_method(InstanceKlass* klass, traceid method_id) {
  Thread* thread = Thread::current();
  JBoltStackFrameKey stackframe_key(klass, method_id);
  JBoltStackFrameValue* stackframe_value = _sampled_methods_refs->get(stackframe_key);
  if (stackframe_value == nullptr) {
    return methodHandle();
  }

  jobject method_holder = stackframe_value->get_method_holder();
  if (method_holder != nullptr && JNIHandles::is_weak_global_handle(method_holder) && JNIHandles::is_global_weak_cleared(method_holder)) {
    log_debug(jbolt)("method klass at %p is unloaded", (void*)klass);
    return methodHandle();
  }

  const Method* const lookup_method = JfrMethodLookup::lookup(klass, method_id);
  if (lookup_method == NULL) {
    // stacktrace obsolete
    return methodHandle();
  }
  assert(lookup_method != NULL, "invariant");
  methodHandle method(thread, const_cast<Method*>(lookup_method));

  return method;
}

void JBoltManager::construct_stacktrace(const JfrStackTrace& stacktrace) {
  NoSafepointVerifier nsv;
  if (stacktrace.get_framesCount() < MIN_FRAMESCOUNT)
    return;

  u4 topFrameIndex = 0;
  u4 max_frames = 0;

  const JfrStackFrame* frames = stacktrace.get_frames();
  unsigned int framesCount = stacktrace.get_framesCount();

  // Native method subsidence
  while (topFrameIndex < framesCount) {
    const JfrStackFrame& frame = frames[topFrameIndex];

    if (method_type_to_string(frame.get_type()) != "Native") {
      break;
    }

    topFrameIndex++;
  }

  if (framesCount - topFrameIndex < MIN_FRAMESCOUNT) {
    return;
  }

  os::Linux::jboltLog_precalc(topFrameIndex, max_frames);

  JBoltFunc **tempfunc = NULL;

  for (u4 i = 0; i < max_frames; ++i) {
    const JfrStackFrame& frame = frames[topFrameIndex + i];

    methodHandle method = lookup_method(const_cast<InstanceKlass*>(frame.get_klass()), frame.get_methodId());
    if (method.is_null()) {
      break;
    }
    const CompiledMethod* const compiled = method->code();

    log_trace(jbolt)(
      "Method id - %lu\n\tBytecode index - %hu\n\tSignature - %s\n\tType - %s\n\tCompiler - %s\n\tCompile Level - %d\n\tSize - %dB\n",
      frame.get_methodId(),
      frame.get_byteCodeIndex(),
      method->external_name(),
      method_type_to_string(frame.get_type()),
      compiled != NULL ? compiled->compiler_name() : "None",
      compiled != NULL ? compiled->comp_level() : -1,
      compiled != NULL ? compiled->size() : 0);

    if (compiled == NULL) break;

    JBoltMethodKey method_key(method->constants()->pool_holder()->name(), method->name(), method->signature());
    JBoltFunc* func = JBoltFunc::constructor(frame.get_klass(), frame.get_methodId(), compiled->size(), method_key);

    if (!os::Linux::jboltLog_do(related_data_jbolt, (address)&stacktrace, i, compiled->comp_level(), (address)func, (address*)&tempfunc)) {
      delete func;
      func = NULL;
      break;
    }
  }

  log_trace(jbolt)(
    "StackTrace hash - %u hotcount - %d\n==============================\n", stacktrace.hash(), stacktrace.hotcount());
}

/**
 * Invoked in JfrStackTraceRepository::write().
 * Each time JfrChunkWrite do write and clear stacktrace table,
 * we update the CG by invoke construct_stacktrace().
 */
void JBoltManager::construct_cg_once() {
  guarantee((UseJBolt && JBoltManager::reorder_phase_is_profiling_or_waiting()), "sanity");

  GrowableArray<JfrStackTrace*>* traces = create_growable_array<JfrStackTrace*>();

  {
    MutexLocker lock(JfrStacktrace_lock, Mutex::_no_safepoint_check_flag);
    const JfrStackTraceRepository& repository = JfrStackTraceRepository::instance();

    if (repository.get_entries_count_jbolt() == 0) {
      return;
    }

    const JfrStackTrace* const * table = repository.get_stacktrace_table_jbolt();
    for (uint i = 0; i < repository.TABLE_SIZE; ++i) {
      for (const JfrStackTrace* trace = table[i]; trace != nullptr; trace = trace->next()) {
        traces->append(const_cast<JfrStackTrace*>(trace));
      }
    }
  }

  for (int i = 0; i < traces->length(); ++i) {
    construct_stacktrace(*(traces->at(i)));
  }

  log_trace(jbolt)(
    "+++++++ one time log over ++++++\n\n");
  delete traces;
}

static void write_order(const GrowableArray<JBoltFunc>* order, fileStream& fs) {
  assert(order != nullptr, "sanity");
  const char* methodFlag = "M";
  const char* segmentor = "C\n";

  log_debug(jbolt)("+============================+\n\t\t\tORDER\n");

  for (int i = 0; i < order->length(); ++i) {
    const JBoltFunc& func = order->at(i);
    if (func.klass() == NULL) {
      fs.write(segmentor, strlen(segmentor));
      continue;
    }

    char* holder_name = func.method_key().klass()->as_C_string();
    char* name = func.method_key().name()->as_C_string();
    char* signature = func.method_key().sig()->as_C_string();
    char size[LINE_BUF_SIZE] = {0};
    snprintf(size, sizeof(size), "%d", func.size());

    log_debug(jbolt)("order %d --- Method - %s %s %s\n", i, holder_name, name, signature);

    fs.write(methodFlag, strlen(methodFlag));
    fs.write(" ", 1);
    fs.write(size, strlen(size));
    fs.write(" ", 1);
    fs.write(holder_name, strlen(holder_name));
    fs.write(" ", 1);
    fs.write(name, strlen(name));
    fs.write(" ", 1);
    fs.write(signature, strlen(signature));
    fs.write("\n", 1);
  }
}

/**
 * Invoked in before_exit().
 * Only use in manual mode.
 * Dump the order to JBoltOrderFile before vm exit.
 */
void JBoltManager::dump_order_in_manual() {
  guarantee((UseJBolt && JBoltDumpMode), "sanity");
  guarantee(reorder_phase_profiling_to_waiting(), "sanity");
  NoSafepointVerifier nsv;
  ResourceMark rm;
  GrowableArray<JBoltFunc>* order = JBoltCallGraph::callgraph_instance().hfsort();

  fileStream orderFile(JBoltOrderFile, "w+");

  if (JBoltOrderFile == NULL || !orderFile.is_open()) {
    log_error(jbolt)("JBoltOrderFile open error");
    vm_exit_during_initialization("JBoltOrderFile open error");
  }

  write_order(order, orderFile);

  log_info(jbolt)("order generate successful !!");
  log_debug(jbolt)("+============================+\n");
  delete order;
  delete _sampled_methods_refs;
  _sampled_methods_refs = nullptr;
  JBoltCallGraph::deinitialize();
}

JBoltErrorCode JBoltManager::dump_order_in_jcmd(const char* filename) {
  guarantee(UseJBolt, "sanity");
  NoSafepointVerifier nsv;
  ResourceMark rm;

  if (_order_stored == nullptr) return JBoltOrderNULL;

  fileStream orderFile(filename, "w+");

  if (filename == NULL || !orderFile.is_open()) return JBoltOpenFileError;

  write_order(_order_stored, orderFile);

  return JBoltOK;
}

/**
 * Do not set the JBolt-related flags manually if UseJBolt is not enabled.
 */
void JBoltManager::check_arguments_not_set() {
  if (UseJBolt) return;

  check_arg_not_set(FLAG_MEMBER_ENUM(JBoltDumpMode));
  check_arg_not_set(FLAG_MEMBER_ENUM(JBoltLoadMode));
  check_arg_not_set(FLAG_MEMBER_ENUM(JBoltOrderFile));
  check_arg_not_set(FLAG_MEMBER_ENUM(JBoltSampleInterval));
  check_arg_not_set(FLAG_MEMBER_ENUM(JBoltCodeHeapSize));
}

/**
 * Check which mode is JBolt in.
 * If JBoltDumpMode or JBoltLoadMode is set manually then do nothing, else it will be fully auto sched by JBolt itself.
 */
void JBoltManager::check_mode() {
  if (!(JBoltDumpMode || JBoltLoadMode)) {
    _auto_mode = true;
    return;
  }

  if (!FLAG_IS_DEFAULT(JBoltSampleInterval)) {
    log_warning(jbolt)("JBoltSampleInterval is ignored because it is not in auto mode.");
  }

  if (JBoltDumpMode && JBoltLoadMode) {
    vm_exit_during_initialization("Do not set both JBoltDumpMode and JBoltLoadMode!");
  }

  guarantee((JBoltDumpMode ^ JBoltLoadMode), "Must set either JBoltDumpMode or JBoltLoadMode!");
}

/**
 * If in auto mode, JBoltOrderFile will be ignored
 * If in any manual mode, then JBoltOrderFile will be necessary.
 * Check whether the order file exists or is accessable.
 */
void JBoltManager::check_order_file() {
  if (auto_mode()) {
    if (JBoltOrderFile != nullptr) log_warning(jbolt)("JBoltOrderFile is ignored because it is in auto mode.");
    return;
  }

  if (JBoltOrderFile == nullptr) {
    vm_exit_during_initialization("JBoltOrderFile is not set!");
  }

  bool file_exist = (::access(JBoltOrderFile, F_OK) == 0);
  if (file_exist) {
    if (JBoltDumpMode) {
      log_warning(jbolt)("JBoltOrderFile to dump already exists and will be overwritten: file=%s.", JBoltOrderFile);
      ::remove(JBoltOrderFile);
    }
  } else {
    if (JBoltLoadMode) {
      vm_exit_during_initialization(err_msg("JBoltOrderFile does not exist or cannot be accessed! file=\"%s\".", JBoltOrderFile));
    }
  }
}

void JBoltManager::check_dependency() {
  if (FLAG_IS_CMDLINE(FlightRecorder) ? !FlightRecorder : false) {
    vm_exit_during_initialization("JBolt depends on JFR!");
  }

  if (!CompilerConfig::is_c2_enabled()) {
    vm_exit_during_initialization("JBolt depends on C2!");
  }

  if (!SegmentedCodeCache) {
    vm_exit_during_initialization("JBolt depends on SegmentedCodeCache!");
  }
}

size_t JBoltManager::calc_nmethod_size_with_padding(size_t nmethod_size) {
  return align_up(nmethod_size, (size_t) CodeCacheSegmentSize);
}

size_t JBoltManager::calc_segment_size_with_padding(size_t segment_size) {
  size_t page_size = CodeCache::page_size();
  if (segment_size < page_size) return page_size;
  return align_down(segment_size, page_size);
}

/**
 * We have to parse the file twice because SymbolTable is not inited in phase 1...
 */
void JBoltManager::load_order_file_phase1(int* method_cnt, size_t* segment_size) {
  assert(JBoltOrderFile != nullptr, "sanity");

  _order_fp = os::fopen(JBoltOrderFile, "r");
  if (_order_fp == nullptr) {
    vm_exit_during_initialization(err_msg("Cannot open file JBoltOrderFile! file=\"%s\".", JBoltOrderFile));
  }

  int mth_cnt = 0;
  size_t seg_size = 0;

  char line[LINE_BUF_SIZE];
  int len = -1;
  while (read_line(_order_fp, line, sizeof(line), &len)) {
    if (len <= 2) continue;
    if (line[0] != 'M' || line[1] != ' ') continue;
    char* left_start = line + 2;

    // parse nmethod size
    size_t nmethod_size;
    if (!read_a_size(left_start, &nmethod_size)) {
      vm_exit_during_initialization(err_msg("Wrong format of JBolt order line! line=\"%s\".", line));
    }
    ++mth_cnt;
    seg_size += calc_nmethod_size_with_padding(nmethod_size);
  }

  *method_cnt = mth_cnt;
  *segment_size = seg_size;
  log_trace(jbolt)("Read order file method_cnt=%d, estimated_segment_size=" SIZE_FORMAT ".", mth_cnt, seg_size);
}

bool JBoltManager::parse_method_line_phase2(char* const line, const int len) {
  // Skip "M ".
  char* left_start = line + 2;

  // Skip nmethod size (has parsed in phase1).
  {
    char* t = strchr(left_start, ' ');
    if (t == nullptr) return false;
    left_start = t + 1;
  }

  // Modify "java.lang.Obj" to "java/lang/Obj".
  replace_all(left_start, '.', '/');

  // Parse the three symbols: class name, method name, signature.
  Symbol* three_symbols[3];
  for (int i = 0; i < 2; ++i) {
    char* t = strchr(left_start, ' ');
    if (t == nullptr) return false;
    Symbol* sym = SymbolTable::new_symbol(left_start, t - left_start);
    three_symbols[i] = sym;
    left_start = t + 1;
  }
  Symbol* sym = SymbolTable::new_symbol(left_start, line + len - left_start);
  three_symbols[2] = sym;
  if (log_is_enabled(Trace, jbolt)) {
    log_trace(jbolt)("HotMethod init: key={%s %s %s}",
                      three_symbols[0]->as_C_string(),
                      three_symbols[1]->as_C_string(),
                      three_symbols[2]->as_C_string());
  }

  // Add to data structure.
  JBoltMethodKey method_key(three_symbols[0], three_symbols[1], three_symbols[2]);
  _hot_methods_sorted->append(method_key);
  JBoltMethodValue method_value;
  bool put = _hot_methods_vis->put(method_key, method_value);
  if (!put) {
    vm_exit_during_initialization(err_msg("Duplicated method: {%s %s %s}!",
            three_symbols[0]->as_C_string(),
            three_symbols[1]->as_C_string(),
            three_symbols[2]->as_C_string()));
  }

  return true;
}

bool JBoltManager::parse_connected_component_line_phase2(char* const line, const int len) { return true; }

void JBoltManager::load_order_file_phase2(TRAPS) {
  guarantee(_order_fp != nullptr, "sanity");

  // re-scan
  fseek(_order_fp, 0, SEEK_SET);

  char line[LINE_BUF_SIZE];
  int len = -1;
  while (read_line(_order_fp, line, sizeof(line), &len)) {
    if (len <= 0) continue;
    bool success = false;
    switch (line[0]) {
      case '#': success = true; break;  // ignore comments
      case 'M': success = parse_method_line_phase2(line, len); break;
      case 'C': success = parse_connected_component_line_phase2(line, len); break;
      default: break;
    }
    if (!success) {
      vm_exit_during_initialization(err_msg("Wrong format of JBolt order line! line=\"%s\".", line));
    }
  }
  fclose(_order_fp);
  _order_fp = nullptr;
}

void JBoltManager::init_load_mode_phase1() {
  if (!(auto_mode() || JBoltLoadMode)) return;

  if (auto_mode()) {
    // auto mode has no order now, initialize as default.
    _hot_methods_sorted = new (ResourceObj::C_HEAP, mtCompiler) GrowableArray<JBoltMethodKey>(1, mtCompiler);
    _hot_methods_vis = new (ResourceObj::C_HEAP, mtCompiler) MethodKeyMap();
    log_info(jbolt)("Default set JBoltCodeHeapSize=" UINTX_FORMAT " B (" UINTX_FORMAT " MB).", JBoltCodeHeapSize, JBoltCodeHeapSize / 1024 / 1024);
    return;
  }
  guarantee(reorder_phase_available_to_collecting(), "sanity");
  size_t total_nmethod_size = 0;
  int method_cnt = 0;
  load_order_file_phase1(&method_cnt, &total_nmethod_size);

  _hot_methods_sorted = new (ResourceObj::C_HEAP, mtCompiler) GrowableArray<JBoltMethodKey>(method_cnt, mtCompiler);
  _hot_methods_vis = new (ResourceObj::C_HEAP, mtCompiler) MethodKeyMap();

  if (FLAG_IS_DEFAULT(JBoltCodeHeapSize)) {
    FLAG_SET_ERGO(JBoltCodeHeapSize, calc_segment_size_with_padding(total_nmethod_size));
    log_info(jbolt)("Auto set JBoltCodeHeapSize=" UINTX_FORMAT " B (" UINTX_FORMAT " MB).", JBoltCodeHeapSize, JBoltCodeHeapSize / 1024 / 1024);
  }
}

void JBoltManager::init_load_mode_phase2(TRAPS) {
  // Only manual load mode need load phase2
  if (!JBoltLoadMode) return;

  load_order_file_phase2(CHECK);
  _reorderable_method_cnt = 0;
  _reorder_method_threshold_cnt = _hot_methods_sorted->length() * _jbolt_reorder_threshold;
}

void JBoltManager::init_dump_mode_phase2(TRAPS) {
  if (!(auto_mode() || JBoltDumpMode)) return;

  JBoltCallGraph::initialize();
  _sampled_methods_refs = new (ResourceObj::C_HEAP, mtTracing) StackFrameKeyMap();

  // JBolt will create a JFR by itself
  // In auto mode, will stop in JBoltControlThread::start_thread() after JBoltSampleInterval.
  // In manual dump mode, won't stop until program exit.
  log_info(jbolt)("JBolt in dump mode now, start a JFR recording named \"jbolt-jfr\".");
  bufferedStream output;
  DCmd::parse_and_execute(DCmd_Source_Internal, &output, "JFR.start name=jbolt-jfr", ' ', THREAD);
  if (HAS_PENDING_EXCEPTION) {
    ResourceMark rm;
    log_warning(jbolt)("unable to start jfr jbolt-jfr");
    log_warning(jbolt)("exception type: %s", PENDING_EXCEPTION->klass()->external_name());
    // don't unwind this exception
    CLEAR_PENDING_EXCEPTION;
  }
}

static void update_stored_order(const GrowableArray<JBoltFunc>* order) {
  if (_order_stored != nullptr) {
    // use a tmp for releasing space to provent _order_stored from being a wild pointer
    GrowableArray<JBoltFunc>* tmp = _order_stored;
    _order_stored = nullptr;
    delete tmp;
  }
  _order_stored = new (ResourceObj::C_HEAP, mtTracing) GrowableArray<JBoltFunc>(order->length(), mtTracing);
  _order_stored->appendAll(order);
}

static CompileTaskInfo* create_compile_task_info(methodHandle& method) {
    CompiledMethod* compiled = method->code();
    if (compiled == nullptr) {
      log_warning(jbolt)("Recompilation Task init failed because of null nmethod. func: %s.", method->external_name());
      return nullptr;
    }
    int osr_bci = compiled->is_osr_method() ? compiled->osr_entry_bci() : InvocationEntryBci;
    int comp_level = compiled->comp_level();
    // comp_level adaptation for deoptmization
    if (comp_level > CompLevel_simple && comp_level <= CompLevel_full_optimization) comp_level = CompLevel_full_optimization;
    CompileTask::CompileReason comp_reason = CompileTask::Reason_Reorder;
    CompileTaskInfo* ret = new CompileTaskInfo(method(), osr_bci, comp_level, (int)comp_reason,
                                                       nullptr, 0);
    return ret;
}

/**
 * This function is invoked by JBoltControlThread.
 * Do initialization for converting dump mode to load mode.
 */
void JBoltManager::init_auto_transition(size_t* segment_size, TRAPS) {
  guarantee(UseJBolt && auto_mode(), "sanity");
  NoSafepointVerifier nsv;
  ResourceMark rm;

  GrowableArray<JBoltFunc>* order = JBoltCallGraph::callgraph_instance().hfsort();
  update_stored_order(order);

  size_t seg_size = 0;
  for (int i = 0; i < order->length(); ++i) {
    const JBoltFunc& func = order->at(i);
    if (func.klass() == NULL) {
      continue;
    }

    methodHandle method = lookup_method(const_cast<InstanceKlass*>(func.klass()), func.method_id());
    if (method.is_null()) {
      continue;
    }

    CompileTaskInfo* cti = create_compile_task_info(method);
    if (cti == nullptr) {
      continue;
    }

    JBoltMethodKey method_key = func.method_key();
    JBoltMethodValue method_value;
    if (!method_value.set_comp_info(cti)) {
      delete cti;
      continue;
    }

    seg_size += calc_nmethod_size_with_padding(func.size());
    _hot_methods_sorted->append(method_key);
    bool put = _hot_methods_vis->put(method_key, method_value);
    if (!put) {
      vm_exit_during_initialization(err_msg("Duplicated method: {%s %s %s}!",
              method_key.klass()->as_C_string(),
              method_key.name()->as_C_string(),
              method_key.sig()->as_C_string()));
    }
    method_value.clear_comp_info_but_not_release();
  }
  log_info(jbolt)("order generate successful !!");
  *segment_size = calc_segment_size_with_padding(seg_size);
  delete order;
}

/**
 * This function must be invoked after CompilerConfig::ergo_initialize() in Arguments::apply_ergo().
 * This function must be invoked before CodeCache::initialize_heaps() in codeCache_init() in init_globals().
 * Thread and SymbolTable is not inited now!
 */
void JBoltManager::init_phase1() {
  if (!UseJBolt) return;
  check_mode();
  check_dependency();
  check_order_file();

  /* dump mode has nothing to do in phase1 */
  init_load_mode_phase1();
}

void JBoltManager::init_phase2(TRAPS) {
  if (!UseJBolt) return;

  ResourceMark rm(THREAD);
  init_dump_mode_phase2(CHECK);
  init_load_mode_phase2(CHECK);

  // Manual dump mode doesn't need JBoltControlThread, directly go to profiling phase
  if (JBoltDumpMode) {
    guarantee(JBoltManager::reorder_phase_available_to_profiling(), "sanity");
    return;
  }

  JBoltControlThread::init(CHECK);
  // Auto mode will start control thread earlier.
  // Manual load mode start later in check_start_reordering()
  if (auto_mode()) {
    JBoltControlThread::start_thread(CHECK_AND_CLEAR);
  }
}

/**
 * Code heaps are initialized between init phase 1 and init phase 2.
 */
void JBoltManager::init_code_heaps(size_t non_nmethod_size, size_t profiled_size, size_t non_profiled_size, size_t cache_size, size_t ps, size_t alignment) {
  assert(UseJBolt && !JBoltDumpMode, "sanity");
  if(!is_aligned(JBoltCodeHeapSize, alignment)) {
    vm_exit_during_initialization(err_msg("JBoltCodeHeapSize should be %ld aligned, please adjust", alignment));
  }

  size_t jbolt_hot_size     = JBoltCodeHeapSize;
  size_t jbolt_tmp_size     = JBoltCodeHeapSize;
  size_t jbolt_total_size   = jbolt_hot_size + jbolt_tmp_size;
  if (non_profiled_size <= jbolt_total_size) {
    vm_exit_during_initialization(err_msg("Not enough space in non-profiled code heap to split out JBolt heap(s): " SIZE_FORMAT "K <= " SIZE_FORMAT "K", non_profiled_size/K, jbolt_total_size/K));
  }
  non_profiled_size -= jbolt_total_size;
  non_profiled_size = align_down(non_profiled_size, alignment);
  FLAG_SET_ERGO(NonProfiledCodeHeapSize, non_profiled_size);

  // Memory layout with JBolt:
  // ---------- high -----------
  //    Non-profiled nmethods
  //      JBolt tmp nmethods
  //      JBolt hot nmethods
  //         Non-nmethods
  //      Profiled nmethods
  // ---------- low ------------
  ReservedCodeSpace rs = CodeCache::reserve_heap_memory(cache_size, ps);
  ReservedSpace profiled_space      = rs.first_part(profiled_size);
  ReservedSpace r1                  = rs.last_part(profiled_size);
  ReservedSpace non_nmethod_space   = r1.first_part(non_nmethod_size);
  ReservedSpace r2                  = r1.last_part(non_nmethod_size);
  ReservedSpace jbolt_hot_space     = r2.first_part(jbolt_hot_size);
  ReservedSpace r3                  = r2.last_part(jbolt_hot_size);
  ReservedSpace jbolt_tmp_space     = r3.first_part(jbolt_tmp_size);
  ReservedSpace non_profiled_space  = r3.last_part(jbolt_tmp_size);

  CodeCache::add_heap(non_nmethod_space, "CodeHeap 'non-nmethods'", CodeBlobType::NonNMethod);
  CodeCache::add_heap(profiled_space, "CodeHeap 'profiled nmethods'", CodeBlobType::MethodProfiled);
  CodeCache::add_heap(non_profiled_space, "CodeHeap 'non-profiled nmethods'", CodeBlobType::MethodNonProfiled);
  const char* no_space = nullptr;
  CodeCache::add_heap(jbolt_hot_space, "CodeHeap 'jbolt hot nmethods'", CodeBlobType::MethodJBoltHot);
  if (jbolt_hot_size != jbolt_hot_space.size()) {
    no_space = "hot";
  }
  CodeCache::add_heap(jbolt_tmp_space, "CodeHeap 'jbolt tmp nmethods'", CodeBlobType::MethodJBoltTmp);
  if (jbolt_tmp_size != jbolt_tmp_space.size()) {
    no_space = "tmp";
  }
  if (no_space != nullptr) {
    vm_exit_during_initialization(FormatBuffer<1024>(
        "No enough space for JBolt %s heap: \n"
        "Expect: cache_size=" SIZE_FORMAT "K, profiled_size=" SIZE_FORMAT "K, non_nmethod_size=" SIZE_FORMAT "K, jbolt_hot_size=" SIZE_FORMAT "K, non_profiled_size=" SIZE_FORMAT "K, jbolt_tmp_size=" SIZE_FORMAT "K\n"
        "Actual: cache_size=" SIZE_FORMAT "K, profiled_size=" SIZE_FORMAT "K, non_nmethod_size=" SIZE_FORMAT "K, jbolt_hot_size=" SIZE_FORMAT "K, non_profiled_size=" SIZE_FORMAT "K, jbolt_tmp_size=" SIZE_FORMAT "K\n"
        "alignment=" SIZE_FORMAT,
        no_space,
        cache_size/K, profiled_size/K,         non_nmethod_size/K,         jbolt_hot_size/K,         non_profiled_size/K,         jbolt_tmp_size/K,
        rs.size()/K,  profiled_space.size()/K, non_nmethod_space.size()/K, jbolt_hot_space.size()/K, non_profiled_space.size()/K, jbolt_tmp_space.size()/K,
        alignment));
  }
}

int JBoltManager::reorder_phase() {
  return Atomic::load_acquire(&_reorder_phase);
}

bool JBoltManager::reorder_phase_available_to_collecting() {
  assert(!auto_mode(), "two-phase only");
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Available, JBoltReorderPhase::Collecting) == JBoltReorderPhase::Available;
}

bool JBoltManager::reorder_phase_collecting_to_reordering() {
  assert(!auto_mode(), "two-phase only");
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Collecting, JBoltReorderPhase::Reordering) == JBoltReorderPhase::Collecting;
}

bool JBoltManager::reorder_phase_available_to_profiling() {
  assert(auto_mode(), "one-phase only");
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Available, JBoltReorderPhase::Profiling) == JBoltReorderPhase::Available;
}

bool JBoltManager::reorder_phase_profiling_to_reordering() {
  assert(auto_mode(), "one-phase only");
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Profiling, JBoltReorderPhase::Reordering) == JBoltReorderPhase::Profiling;
}

bool JBoltManager::reorder_phase_reordering_to_available() {
  assert(auto_mode(), "one-phase only");
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Reordering, JBoltReorderPhase::Available) == JBoltReorderPhase::Reordering;
}

bool JBoltManager::reorder_phase_profiling_to_available() {
  assert(auto_mode(), "one-phase only");
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Profiling, JBoltReorderPhase::Available) == JBoltReorderPhase::Profiling;
}

bool JBoltManager::reorder_phase_profiling_to_waiting() {
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Profiling, JBoltReorderPhase::Waiting) == JBoltReorderPhase::Profiling;
}

bool JBoltManager::reorder_phase_waiting_to_reordering() {
  assert(auto_mode(), "one-phase only");
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Waiting, JBoltReorderPhase::Reordering) == JBoltReorderPhase::Waiting;
}

bool JBoltManager::reorder_phase_waiting_to_available() {
  assert(auto_mode(), "one-phase only");
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Waiting, JBoltReorderPhase::Available) == JBoltReorderPhase::Waiting;
}

bool JBoltManager::reorder_phase_reordering_to_end() {
  return Atomic::cmpxchg(&_reorder_phase, JBoltReorderPhase::Reordering, JBoltReorderPhase::End) == JBoltReorderPhase::Reordering;
}

bool JBoltManager::reorder_phase_is_waiting() {
  return Atomic::load_acquire(&_reorder_phase) == JBoltReorderPhase::Waiting;
}

bool JBoltManager::reorder_phase_is_available() {
  bool res = (Atomic::load_acquire(&_reorder_phase) == JBoltReorderPhase::Available);
  assert(!res || auto_mode(), "one-phase only");
  return res;
}

bool JBoltManager::reorder_phase_is_collecting() {
  bool res = (Atomic::load_acquire(&_reorder_phase) == JBoltReorderPhase::Collecting);
  assert(!res || !auto_mode(), "two-phase only");
  return res;
}

bool JBoltManager::reorder_phase_is_profiling() {
  bool res = (Atomic::load_acquire(&_reorder_phase) == JBoltReorderPhase::Profiling);
  assert(!res || auto_mode(), "one-phase only");
  return res;
}

bool JBoltManager::reorder_phase_is_reordering() {
  return Atomic::load_acquire(&_reorder_phase) == JBoltReorderPhase::Reordering;
}

bool JBoltManager::reorder_phase_is_collecting_or_reordering() {
  int p = Atomic::load_acquire(&_reorder_phase);
  assert(p != JBoltReorderPhase::Collecting || !auto_mode(), "two-phase only");
  return p == JBoltReorderPhase::Collecting || p == JBoltReorderPhase::Reordering;
}

bool JBoltManager::reorder_phase_is_profiling_or_waiting() {
  int p = Atomic::load_acquire(&_reorder_phase);
  return p == JBoltReorderPhase::Profiling || p == JBoltReorderPhase::Waiting;
}

Method* JBoltManager::cur_reordering_method() {
  return Atomic::load_acquire(&_cur_reordering_method);
}

void JBoltManager::set_cur_reordering_method(Method* method) {
  Atomic::release_store(&_cur_reordering_method, method);
}

int JBoltManager::inc_reorderable_method_cnt() {
  return Atomic::add(&_reorderable_method_cnt, +1);
}

bool JBoltManager::can_reorder_now() {
  return Atomic::load_acquire(&_reorderable_method_cnt) >= _reorder_method_threshold_cnt;
}

bool JBoltManager::should_reorder_now() {
  return Atomic::load_acquire(&_reorderable_method_cnt) == _reorder_method_threshold_cnt;
}

int JBoltManager::primary_hot_seg() {
  return Atomic::load_acquire(&_primary_hot_seg);
}

int JBoltManager::secondary_hot_seg() {
  return Atomic::load_acquire(&_secondary_hot_seg);
}

int JBoltManager::clear_manager() {
  /* _hot_methods_sorted, _hot_methods_vis and _sampled_methods_refs have been cleared in other pos, don't delete again */
  guarantee(_hot_methods_sorted == nullptr, "sanity");
  guarantee(_hot_methods_vis == nullptr, "sanity");
  guarantee(_sampled_methods_refs == nullptr, "sanity");
  // Re-allocate them
  _hot_methods_sorted = new (ResourceObj::C_HEAP, mtCompiler) GrowableArray<JBoltMethodKey>(1, mtCompiler);
  _hot_methods_vis = new (ResourceObj::C_HEAP, mtCompiler) MethodKeyMap();
  _sampled_methods_refs = new (ResourceObj::C_HEAP, mtTracing) StackFrameKeyMap();

  return 0;
}

/**
 * Invoked in JBoltControlThread::prev_control_schedule().
 * Expect to only execute in auto mode while JBolt.start triggered.
 * Clear JBolt related data structures to restore a initial env same as sample never happening.
*/
int JBoltManager::clear_last_sample_datas() {
  int ret = 0;
  // Clear _table_jbolt in JfrStackTraceRepository
  ret = JfrStackTraceRepository::clear_jbolt();
  // Clear JBoltCallGraph
  ret = JBoltCallGraph::callgraph_instance().clear_instance();
  // Clear JBoltManager
  ret = clear_manager();

  return ret;
}

/**
 * Invoked in JBoltControlThread::prev_control_schedule().
 * Swap primary hot segment with secondary hot segment
 */
void JBoltManager::swap_semi_jbolt_segs() {
  guarantee(reorder_phase_is_waiting(), "swap must happen in reorder phase Profiling.");
  int tmp = Atomic::xchg(&_secondary_hot_seg, Atomic::load_acquire(&_primary_hot_seg));
  Atomic::xchg(&_primary_hot_seg, tmp);
}

/**
 * Invoked in JBoltControlThread::post_control_schdule().
 * Free scondary hot segment space for next reorder.
 */
void JBoltManager::clear_secondary_hot_seg(TRAPS) {
  guarantee(reorder_phase_is_available(), "secondary clear must happen in reorder phase Available.");
  // scan secondary hot seg and recompile alive nmethods to non-profiled
  ResourceMark rm(THREAD);
  // We cannot alloc weak handle within CodeCache_lock because of the mutex rank check.
  // So instead we keep the methods alive only within the scope of this method.
  JBoltUtils::MetaDataKeepAliveMark mdm(THREAD);
  const GrowableArray<Metadata*>& to_recompile = mdm.kept();

  {
    MutexLocker mu(CodeCache_lock, Mutex::_no_safepoint_check_flag);
    CodeHeap* sec_hot = CodeCache::get_code_heap(secondary_hot_seg());
    for (CodeBlob* cb = (CodeBlob*) sec_hot->first(); cb != nullptr; cb = (CodeBlob*) sec_hot->next(cb)) {
      nmethod* nm = cb->as_nmethod_or_null();
      Method* m = nm->method();
      if (nm && nm->get_state() == CompiledMethod::in_use && m != nullptr) {
        mdm.add(m);
      }
    }
  }

  for (int i = 0; i < to_recompile.length(); ++i) {
    Method* m = (Method*) to_recompile.at(i);
    methodHandle method(THREAD, m);
    CompileTaskInfo* cti = create_compile_task_info(method);
    if (cti == nullptr) continue;
    guarantee(cti->try_select(), "method is on stack, should be ok");
    assert(cti->hot_method() == nullptr, "sanity");
    methodHandle hot_method;

    bool recompile_result = enqueue_recompile_task(cti, method, hot_method, THREAD);
    if(recompile_result) {
      check_compiled_result(method(), CodeBlobType::MethodNonProfiled, THREAD);
    }
    delete cti;
  }

  // need three times force_sweep to thoroughly clear secondary hot segment
  NMethodSweeper::force_sweep();
  NMethodSweeper::force_sweep();
  NMethodSweeper::force_sweep();
  log_info(jbolt)("Sweep secondary segment");
  print_code_heaps();
}

/**
 * Invoked in ciEnv::register_method() in CompilerThread.
 * Controls where the new nmethod should be allocated.
 *
 * Returns CodeBlobType::All if it is not determined by JBolt logic.
 */
int JBoltManager::calc_code_blob_type(Method* method, CompileTask* task, TRAPS) {
  assert(UseJBolt && reorder_phase_is_collecting_or_reordering(), "sanity");
  const int not_care = CodeBlobType::All;

  // Only cares about non-profiled segment.
  int lvl = task->comp_level();
  if (lvl != CompLevel_full_optimization && lvl != CompLevel_simple) {
    return not_care;
  }

  // Ignore on-stack-replacement.
  if (task->osr_bci() != InvocationEntryBci) {
    return not_care;
  }

  int cur_reorder_phase = reorder_phase();
  // Do nothing after reordering.
  if (cur_reorder_phase != JBoltReorderPhase::Collecting && cur_reorder_phase != JBoltReorderPhase::Reordering) {
    return not_care;
  }
  // Only cares about the current reordering method.
  if (cur_reorder_phase == JBoltReorderPhase::Reordering) {
    if (cur_reordering_method() == method) {
      log_trace(jbolt)("Compiling to JBolt heap: method=%s.", method->name_and_sig_as_C_string());
      return primary_hot_seg();
    }
    return not_care;
  }
  guarantee(cur_reorder_phase == JBoltReorderPhase::Collecting, "sanity");
  assert(!auto_mode(), "sanity");

  JBoltMethodKey method_key(method);
  JBoltMethodValue* method_value = _hot_methods_vis->get(method_key);
  if (method_value == nullptr) {
    return not_care;
  }

  // Register the method and the compile task.
  if (method_value->get_comp_info() == nullptr) {
    CompileTaskInfo* cti = new CompileTaskInfo(method, task->osr_bci(), task->comp_level(), (int) task->compile_reason(),
                                                       task->hot_method(), task->hot_count());
    if (method_value->set_comp_info(cti)) {
      int cnt = inc_reorderable_method_cnt();
      log_trace(jbolt)("Reorderable method found: cnt=%d, lvl=%d, p=%p, method=%s.",
              cnt, task->comp_level(), method, method->name_and_sig_as_C_string());
      if (is_power_of_2(_reorder_method_threshold_cnt - cnt)) {
        log_debug(jbolt)("Reorderable cnt: %d/%d/%d", cnt, _reorder_method_threshold_cnt, _hot_methods_sorted->length());
      }
      if (cnt == _reorder_method_threshold_cnt) {
        log_info(jbolt)("Time to reorder: %d/%d/%d", cnt, _reorder_method_threshold_cnt, _hot_methods_sorted->length());
        _start_reordering_thread = THREAD;
      }
    } else {
      delete cti;
    }
  }

  return secondary_hot_seg();
}

/**
 * Check if reordering should start.
 * The reordering should only start once (for now).
 * We don't do this check in "if (cnt == _reorder_method_threshold_cnt)" in calc_code_blob_type()
 * because it will cause an assert error: "Possible safepoint reached by thread that does not allow it".
 */
void JBoltManager::check_start_reordering(TRAPS) {
  // _start_reordering_thread is set and tested in the same thread. No need to be atomic.
  if (_start_reordering_thread == THREAD) {
    _start_reordering_thread = nullptr;
    if (JBoltControlThread::get_thread() == nullptr) {
      assert(can_reorder_now(), "sanity");
      log_info(jbolt)("Starting JBoltControlThread to reorder.");
      JBoltControlThread::start_thread(CHECK_AND_CLEAR);
    }
  }
}

/**
 * The task will be added to the compile queue and be compiled just like other tasks.
 */
CompileTask* JBoltManager::create_a_task_instance(CompileTaskInfo* cti, methodHandle& method, methodHandle& hot_method, TRAPS) {
  int osr_bci = cti->osr_bci();
  int comp_level = cti->comp_level();
  CompileTask::CompileReason comp_reason = (CompileTask::CompileReason) cti->comp_reason();
  int hot_count = cti->hot_count();
  bool is_blocking = true;

  // init a task (@see CompileBroker::create_compile_task())
  CompileTask* task = CompileTask::allocate();
  int compile_id = CompileBroker::assign_compile_id(method, osr_bci);
  task->initialize(compile_id, method, osr_bci, comp_level,
                  hot_method, hot_count, comp_reason,
                  is_blocking);
  return task;
}

/**
 * Print the failure reason if something is wrong in recompilation.
 */
void JBoltManager::check_compiled_result(Method* method, int check_blob_type, TRAPS) {
  CompiledMethod* cm = method->code();
  if (cm == nullptr) {
    log_warning(jbolt)("Recompilation failed because of null nmethod.");
    return;
  }
  nmethod* nm = cm->as_nmethod_or_null();
  if (nm == nullptr) {
    log_warning(jbolt)("Recompilation failed because the code is not a nmethod.");
    return;
  }
  int code_blob_type = CodeCache::get_code_blob_type(nm);
  if (code_blob_type != check_blob_type) {
    log_warning(jbolt)("Recompilation failed because the nmethod is not in heap [%s]: it's in [%s].",
                        CodeCache::get_code_heap_name(check_blob_type), CodeCache::get_code_heap_name(code_blob_type));
    return;
  }
  log_trace(jbolt)("Recompilation good: code=%p, size=%d, method=%s, heap=%s.",
                    nm, nm->size(), method->name_and_sig_as_C_string(), CodeCache::get_code_heap_name(check_blob_type));
}

/**
 * Create the compile task instance and enqueue into compile queue
 */
bool JBoltManager::enqueue_recompile_task(CompileTaskInfo* cti, methodHandle& method, methodHandle& hot_method, TRAPS) {
  CompileTask* task = nullptr;
  CompileQueue* queue = CompileBroker::compile_queue(cti->comp_level());
  { MutexLocker locker(THREAD, MethodCompileQueue_lock);
    if (CompileBroker::compilation_is_in_queue(method)) {
      log_warning(jbolt)("JBOLT won't compile as \"compilation is in queue\": method=%s.", method->name_and_sig_as_C_string());
      return false;
    }

    task = create_a_task_instance(cti, method, hot_method, CHECK_AND_CLEAR_false);
    if (task == nullptr) {
      log_warning(jbolt)("JBOLT won't compile as \"task instance is NULL\": method=%s.", method->name_and_sig_as_C_string());
      return false;
    }
    queue->add(task);
  }

  // Same waiting logic as CompileBroker::wait_for_completion().
  { MonitorLocker ml(THREAD, task->lock());
    while (!task->is_complete() && !CompileBroker::is_compilation_disabled_forever()) {
      ml.wait();
    }
  }

  CompileBroker::wait_for_completion(task);
  task = nullptr; // freed
  return true;
}

/**
 * Recompilation is to move the nmethod to _primary_hot_seg.
 */
bool JBoltManager::recompile_one(CompileTaskInfo* cti, methodHandle& method, methodHandle& hot_method, TRAPS) {
  ResourceMark rm(THREAD);

  if (cti->osr_bci() != InvocationEntryBci) {
    log_trace(jbolt)("We don't handle on-stack-replacement nmethods: method=%s.", method->name_and_sig_as_C_string());
    return false;
  }

  if (log_is_enabled(Trace, jbolt)) {
    const char* heap_name = nullptr;
    CompiledMethod* cm = method->code();
    if (cm == nullptr) heap_name = "<null>";
    else if (!cm->is_nmethod()) heap_name = "<not-nmethod>";
    else heap_name = CodeCache::get_code_heap_name(CodeCache::get_code_blob_type(cm));
    log_trace(jbolt)("Start to recompile & reorder: heap=%s, method=%s.", heap_name, method->name_and_sig_as_C_string());
  }

  // Add a compilation task.
  set_cur_reordering_method(method());
  enqueue_recompile_task(cti, method, hot_method, CHECK_AND_CLEAR_false);
  check_compiled_result(method(), primary_hot_seg(), CHECK_AND_CLEAR_false);

  return true;
}

/**
 * This method is invoked in a new thread JBoltControlThread.
 * Recompiles the methods in the order list one by one (serially) based on the hot order.
 * The methods to recompile were almost all in MethodJBoltTmp, and will in install in
 * MethodJBoltHot after recompilation.
 */
void JBoltManager::reorder_all_methods(TRAPS) {
  guarantee(UseJBolt && reorder_phase_is_reordering(), "sanity");
  log_info(jbolt)("Start to reorder!");
  print_code_heaps();

  ResourceMark rm(THREAD);
  for (int i = 0; i < _hot_methods_sorted->length(); ++i) {
    JBoltMethodKey k = _hot_methods_sorted->at(i);
    JBoltMethodValue* v = _hot_methods_vis->get(k);
    if (v == nullptr) continue;
    CompileTaskInfo* cti = v->get_comp_info();
    if (cti == nullptr) continue;
    if (!cti->try_select()) continue;

    methodHandle method(THREAD, cti->method());
    methodHandle hot_method(THREAD, cti->hot_method());

    recompile_one(cti, method, hot_method, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      Handle ex(THREAD, PENDING_EXCEPTION);
      CLEAR_PENDING_EXCEPTION;
      LogTarget(Warning, jbolt) lt;
      if (lt.is_enabled()) {
        LogStream ls(lt);
        ls.print("Failed to recompile the method: %s.", method->name_and_sig_as_C_string());
        java_lang_Throwable::print(ex(), &ls);
      }
    }
  }

  log_info(jbolt)("JBolt reordering succeeds.");
  print_code_heaps();

}

void JBoltManager::clear_structures() {
  delete _sampled_methods_refs;
  _sampled_methods_refs = nullptr;
  JBoltCallGraph::deinitialize();
  set_cur_reordering_method(nullptr);
  delete _hot_methods_sorted;
  _hot_methods_sorted = nullptr;
  delete _hot_methods_vis;
  _hot_methods_vis = nullptr;
}

void JBoltManager::print_code_heap(outputStream& ls, CodeHeap* heap, const char* name) {
  for (CodeBlob* cb = (CodeBlob*) heap->first(); cb != nullptr; cb = (CodeBlob*) heap->next(cb)) {
    nmethod* nm = cb->as_nmethod_or_null();
    Method* m = nm != nullptr ? nm->method() : nullptr;
    ls.print_cr("%s %p %d alive=%s, zombie=%s, nmethod=%s, entrant=%s, name=[%s %s %s]",
                                name,
                                cb, cb->size(),
                                B_TF(cb->is_alive()),
                                B_TF(cb->is_zombie()),
                                B_TF(cb->is_nmethod()),
                                nm ? B_TF(!nm->is_not_entrant()) : "?",
                                m ? m->method_holder()->name()->as_C_string() : cb->name(),
                                m ? m->name()->as_C_string() : nullptr,
                                m ? m->signature()->as_C_string() : nullptr);
  }
}

void JBoltManager::print_code_heaps() {
  {
    LogTarget(Debug, jbolt) lt;
    if (!lt.is_enabled()) return;
    LogStream ls(lt);
    MutexLocker mu(CodeCache_lock, Mutex::_no_safepoint_check_flag);
    CodeCache::print_summary(&ls, true);
  }

  {
    LogTarget(Trace, jbolt) lt;
    if (!lt.is_enabled()) return;
    LogStream ls(lt);
    CodeHeap* hot_heap = CodeCache::get_code_heap(CodeBlobType::MethodJBoltHot);
    CodeHeap* tmp_heap = CodeCache::get_code_heap(CodeBlobType::MethodJBoltTmp);

    ResourceMark rm;
    if (hot_heap == nullptr) {
      ls.print_cr("The jbolt hot heap is null.");
    } else {
      print_code_heap(ls, hot_heap, "hot");
    }
    if (tmp_heap == nullptr) {
      ls.print_cr("The jbolt tmp heap is null.");
    } else {
      print_code_heap(ls, tmp_heap, "tmp");
    }
  }
}

#undef B_TF