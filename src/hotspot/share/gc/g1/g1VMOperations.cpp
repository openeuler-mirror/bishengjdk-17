/*
 * Copyright (c) 2001, 2022, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "gc/g1/g1CollectedHeap.inline.hpp"
#include "gc/g1/g1ConcurrentMarkThread.inline.hpp"
#include "gc/g1/g1Policy.hpp"
#include "gc/g1/g1VMOperations.hpp"
#include "gc/g1/g1Trace.hpp"
#include "gc/shared/concurrentGCBreakpoints.hpp"
#include "gc/shared/gcCause.hpp"
#include "gc/shared/gcId.hpp"
#include "gc/shared/gcTimer.hpp"
#include "gc/shared/gcTraceTime.inline.hpp"
#include "gc/shared/isGCActiveMark.hpp"
#include "memory/universe.hpp"
#include "runtime/interfaceSupport.inline.hpp"

void VM_G1CollectFull::doit() {
  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  GCCauseSetter x(g1h, _gc_cause);
  _gc_succeeded = g1h->do_full_collection(true  /* explicit_gc */,
                                          false /* clear_all_soft_refs */,
                                          false /* do_maximum_compaction */);
}

VM_G1TryInitiateConcMark::VM_G1TryInitiateConcMark(uint gc_count_before,
                                                   GCCause::Cause gc_cause,
                                                   double target_pause_time_ms) :
  VM_GC_Operation(gc_count_before, gc_cause),
  _target_pause_time_ms(target_pause_time_ms),
  _transient_failure(false),
  _cycle_already_in_progress(false),
  _whitebox_attached(false),
  _terminating(false),
  _gc_succeeded(false)
{}

bool VM_G1TryInitiateConcMark::doit_prologue() {
  bool result = VM_GC_Operation::doit_prologue();
  // The prologue can fail for a couple of reasons. The first is that another GC
  // got scheduled and prevented the scheduling of the concurrent start GC. The
  // second is that the GC locker may be active and the heap can't be expanded.
  // In both cases we want to retry the GC so that the concurrent start pause is
  // actually scheduled. In the second case, however, we should stall until
  // until the GC locker is no longer active and then retry the concurrent start GC.
  if (!result) _transient_failure = true;
  return result;
}

void VM_G1TryInitiateConcMark::doit() {
  G1CollectedHeap* g1h = G1CollectedHeap::heap();

  GCCauseSetter x(g1h, _gc_cause);

  // Record for handling by caller.
  _terminating = g1h->_cm_thread->should_terminate();

  if (_terminating && GCCause::is_user_requested_gc(_gc_cause)) {
    // When terminating, the request to initiate a concurrent cycle will be
    // ignored by do_collection_pause_at_safepoint; instead it will just do
    // a young-only or mixed GC (depending on phase).  For a user request
    // there's no point in even doing that much, so done.  For some non-user
    // requests the alternative GC might still be needed.
  } else if (!g1h->policy()->force_concurrent_start_if_outside_cycle(_gc_cause)) {
    // Failure to force the next GC pause to be a concurrent start indicates
    // there is already a concurrent marking cycle in progress.  Set flag
    // to notify the caller and return immediately.
    _cycle_already_in_progress = true;
  } else if ((_gc_cause != GCCause::_wb_breakpoint) &&
             ConcurrentGCBreakpoints::is_controlled()) {
    // WhiteBox wants to be in control of concurrent cycles, so don't try to
    // start one.  This check is after the force_concurrent_start_xxx so that a
    // request will be remembered for a later partial collection, even though
    // we've rejected this request.
    _whitebox_attached = true;
  } else if (!g1h->do_collection_pause_at_safepoint(_target_pause_time_ms)) {
    // Failure to perform the collection at all occurs because GCLocker is
    // active, and we have the bad luck to be the collection request that
    // makes a later _gc_locker collection needed.  (Else we would have hit
    // the GCLocker check in the prologue.)
    _transient_failure = true;
  } else if (g1h->should_upgrade_to_full_gc()) {
    _gc_succeeded = g1h->upgrade_to_full_collection();
  } else {
    _gc_succeeded = true;
  }
}

VM_G1CollectForAllocation::VM_G1CollectForAllocation(size_t         word_size,
                                                     uint           gc_count_before,
                                                     GCCause::Cause gc_cause,
                                                     double         target_pause_time_ms) :
  VM_CollectForAllocation(word_size, gc_count_before, gc_cause),
  _gc_succeeded(false),
  _target_pause_time_ms(target_pause_time_ms) {

  guarantee(target_pause_time_ms > 0.0,
            "target_pause_time_ms = %1.6lf should be positive",
            target_pause_time_ms);
  _gc_cause = gc_cause;
}

bool VM_G1CollectForAllocation::should_try_allocation_before_gc() {
  // Don't allocate before a preventive GC.
  return _gc_cause != GCCause::_g1_preventive_collection;
}

void VM_G1CollectForAllocation::doit() {
  G1CollectedHeap* g1h = G1CollectedHeap::heap();

  if (should_try_allocation_before_gc() && _word_size > 0) {
    // An allocation has been requested. So, try to do that first.
    _result = g1h->attempt_allocation_at_safepoint(_word_size,
                                                   false /* expect_null_cur_alloc_region */);
    if (_result != NULL) {
      // If we can successfully allocate before we actually do the
      // pause then we will consider this pause successful.
      _gc_succeeded = true;
      return;
    }
  }

  GCCauseSetter x(g1h, _gc_cause);
  // Try a partial collection of some kind.
  _gc_succeeded = g1h->do_collection_pause_at_safepoint(_target_pause_time_ms);

  if (_gc_succeeded) {
    if (_word_size > 0) {
      // An allocation had been requested. Do it, eventually trying a stronger
      // kind of GC.
      _result = g1h->satisfy_failed_allocation(_word_size, &_gc_succeeded);
    } else if (g1h->should_upgrade_to_full_gc()) {
      // There has been a request to perform a GC to free some space. We have no
      // information on how much memory has been asked for. In case there are
      // absolutely no regions left to allocate into, do a full compaction.
      _gc_succeeded = g1h->upgrade_to_full_collection();
    }
  }
}

void VM_G1PauseConcurrent::doit() {
  GCIdMark gc_id_mark(_gc_id);
  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  GCTraceCPUTime tcpu(g1h->concurrent_mark()->gc_tracer_cm());

  // GCTraceTime(...) only supports sub-phases, so a more verbose version
  // is needed when we report the top-level pause phase.
  GCTraceTimeLogger(Info, gc) logger(_message, GCCause::_no_gc, true);
  GCTraceTimePauseTimer       timer(_message, g1h->concurrent_mark()->gc_timer_cm());
  GCTraceTimeDriver           t(&logger, &timer);

  TraceCollectorStats tcs(g1h->g1mm()->conc_collection_counters());
  SvcGCMarker sgcm(SvcGCMarker::CONCURRENT);
  IsGCActiveMark x;

  work();
}

bool VM_G1PauseConcurrent::doit_prologue() {
  Heap_lock->lock();
  return true;
}

void VM_G1PauseConcurrent::doit_epilogue() {
  if (Universe::has_reference_pending_list()) {
    Heap_lock->notify_all();
  }
  Heap_lock->unlock();
}

void VM_G1PauseRemark::work() {
  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  g1h->concurrent_mark()->remark();
}

void VM_G1PauseCleanup::work() {
  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  g1h->concurrent_mark()->cleanup();
}

G1_ChangeMaxHeapOp::G1_ChangeMaxHeapOp(size_t new_max_heap) :
  VM_ChangeMaxHeapOp(new_max_heap) {
}

/*
 * No need calculate young/old size, shrink will adjust young automatically.
 * ensure young_list_length, _young_list_max_length, _young_list_target_length align.
 *
 * 1. check if need perform gc: new_heap_max >= minimum_desired_capacity
 * 2. perform full GC if necessary
 * 3. update new limit
 * 4. validation
 */
void G1_ChangeMaxHeapOp::doit() {
  G1CollectedHeap* heap      = static_cast<G1CollectedHeap*>(Universe::heap());
  const size_t min_heap_size = MinHeapSize;
  const size_t max_heap_size = heap->current_max_heap_size();
  bool is_shrink             = _new_max_heap < max_heap_size;

  // step1. calculate maximum_used_percentage for shrink validity check
  const double minimum_free_percentage = static_cast<double>(MinHeapFreeRatio) / 100.0;
  const double maximum_used_percentage = 1.0 - minimum_free_percentage;

  // step2. trigger GC as needed and resize
  if (is_shrink) {
    trigger_gc_shrink(_new_max_heap, maximum_used_percentage, max_heap_size);
  }

  log_debug(dynamic, heap)("G1_ElasticMaxHeapOp: current capacity " SIZE_FORMAT "K, new max heap " SIZE_FORMAT "K",
                            heap->capacity() / K, _new_max_heap / K);

  // step3. check if can update new limit
  if (heap->capacity() <= _new_max_heap) {
    uint dynamic_max_heap_len = static_cast<uint>(_new_max_heap / HeapRegion::GrainBytes);
    heap->set_current_max_heap_size(_new_max_heap);
    heap->_hrm.set_dynamic_max_heap_length(dynamic_max_heap_len);
    // G1 young/old share same max size
    heap->update_gen_max_counter(_new_max_heap);
    _resize_success = true;
    log_debug(dynamic, heap)("G1_ElasticMaxHeapOp success");
  } else {
    log_debug(dynamic, heap)("G1_ElasticMaxHeapOp fail");
  }
}

bool DynamicMaxHeap_G1CanShrink(double used_after_gc_d, size_t _new_max_heap, double maximum_used_percentage, size_t max_heap_size) {
  double minimum_desired_capacity_d = used_after_gc_d / maximum_used_percentage;
  double desired_capacity_upper_bound = static_cast<double>(max_heap_size);
  minimum_desired_capacity_d = (minimum_desired_capacity_d < desired_capacity_upper_bound) ? minimum_desired_capacity_d : desired_capacity_upper_bound;
  size_t minimum_desired_capacity = static_cast<size_t>(minimum_desired_capacity_d);
  minimum_desired_capacity = (minimum_desired_capacity < max_heap_size)? minimum_desired_capacity : max_heap_size;
  bool can_shrink = (_new_max_heap >= minimum_desired_capacity);
  return can_shrink;
}

void G1_ChangeMaxHeapOp::trigger_gc_shrink(size_t _new_max_heap,
                                      double maximum_used_percentage,
                                      size_t max_heap_size){
  G1CollectedHeap* heap      = static_cast<G1CollectedHeap*>(Universe::heap());
  G1CollectorState* collector_state = heap->collector_state();
  bool triggered_full_gc = false;
  bool can_shrink = DynamicMaxHeap_G1CanShrink(static_cast<double>(heap->used()), _new_max_heap, maximum_used_percentage, max_heap_size);
  if (!can_shrink) {
    // trigger Young GC
    collector_state->set_in_young_only_phase(true);
    collector_state->set_in_young_gc_before_mixed(true);
    GCCauseSetter gccs(heap, _gc_cause);
    bool minor_gc_succeeded = heap->do_collection_pause_at_safepoint(MaxGCPauseMillis);
    if (minor_gc_succeeded) {
      log_debug(dynamic, heap)("G1_ElasticMaxHeapOp heap after Young GC");
      LogTarget(Debug, dynamic, heap) lt;
      if (lt.is_enabled()) {
        LogStream ls(lt);
        heap->print_on(&ls);
      }
    }
    can_shrink = DynamicMaxHeap_G1CanShrink(static_cast<double>(heap->used()), _new_max_heap, maximum_used_percentage, max_heap_size);
    if (!can_shrink) {
      // trigger Full GC and adjust everything in resize_if_necessary_after_full_collection
      heap->set_exp_dynamic_max_heap_size(_new_max_heap);
      heap->do_full_collection(true);
      log_debug(dynamic, heap)("G1_ElasticMaxHeapOp heap after Full GC");
      LogTarget(Debug, dynamic, heap) lt;
      if (lt.is_enabled()) {
        LogStream ls(lt);
        heap->print_on(&ls);
      }
      heap->set_exp_dynamic_max_heap_size(0);
      triggered_full_gc = true;
    }
  }

  if (!triggered_full_gc) {
    // there may be two situations when entering this branch:
    //     1. first check passed, no GC triggered
    //     2. first check failed, triggered Young GC,
    //        second check passed
    // so the shrink has not been completed and it must be valid to shrink
    g1_shrink_without_full_gc(_new_max_heap);
  }
}

void G1_ChangeMaxHeapOp::g1_shrink_without_full_gc(size_t _new_max_heap) {
  G1CollectedHeap* heap      = static_cast<G1CollectedHeap*>(Universe::heap());
  size_t capacity_before_shrink = heap->capacity();
  // _new_max_heap is large enough, do nothing
  if (_new_max_heap >= capacity_before_shrink) {
    return;
  }
  // Capacity too large, compute shrinking size and shrink
  size_t shrink_bytes = capacity_before_shrink - _new_max_heap;
  heap->_verifier->verify_region_sets_optional();
  heap->_hrm.remove_all_free_regions();
  heap->shrink_helper(shrink_bytes);
  heap->rebuild_region_sets(true /* free_list_only */, true /* is_dynamic_max_heap_shrink */);
  heap->_hrm.verify_optional();
  heap->_verifier->verify_region_sets_optional();
  heap->_verifier->verify_after_gc(G1HeapVerifier::G1VerifyFull);

  log_debug(dynamic, heap)("G1_ElasticMaxHeapOp: attempt heap shrinking for dynamic max heap %s "
          "origin capacity " SIZE_FORMAT "K "
          "new capacity " SIZE_FORMAT "K "
          "shrink by " SIZE_FORMAT "K",
           heap->capacity() <= _new_max_heap ? "success" : "fail",
           capacity_before_shrink / K,
           heap->capacity() / K,
           shrink_bytes / K);
}
