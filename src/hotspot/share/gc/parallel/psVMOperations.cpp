/*
 * Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved.
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
#include "gc/parallel/parallelScavengeHeap.inline.hpp"
#include "gc/parallel/psScavenge.hpp"
#include "gc/parallel/psVMOperations.hpp"
#include "gc/shared/gcLocker.hpp"
#include "gc/shared/genArguments.hpp"
#include "logging/log.hpp"
#include "logging/logStream.hpp"
#include "memory/universe.hpp"
#include "runtime/os.hpp"
#include "utilities/dtrace.hpp"

// The following methods are used by the parallel scavenge collector
VM_ParallelGCFailedAllocation::VM_ParallelGCFailedAllocation(size_t word_size,
                                                             uint gc_count) :
    VM_CollectForAllocation(word_size, gc_count, GCCause::_allocation_failure) {
  assert(word_size != 0, "An allocation should always be requested with this operation.");
}

void VM_ParallelGCFailedAllocation::doit() {
  SvcGCMarker sgcm(SvcGCMarker::MINOR);

  ParallelScavengeHeap* heap = ParallelScavengeHeap::heap();

  GCCauseSetter gccs(heap, _gc_cause);
  _result = heap->failed_mem_allocate(_word_size);

  if (_result == NULL && GCLocker::is_active_and_needs_gc()) {
    set_gc_locked();
  }
}

static bool is_cause_full(GCCause::Cause cause) {
  return (cause != GCCause::_gc_locker) && (cause != GCCause::_wb_young_gc)
         DEBUG_ONLY(&& (cause != GCCause::_scavenge_alot));
}

// Only used for System.gc() calls
VM_ParallelGCSystemGC::VM_ParallelGCSystemGC(uint gc_count,
                                             uint full_gc_count,
                                             GCCause::Cause gc_cause) :
  VM_GC_Operation(gc_count, gc_cause, full_gc_count, is_cause_full(gc_cause))
{
}

void VM_ParallelGCSystemGC::doit() {
  SvcGCMarker sgcm(SvcGCMarker::FULL);

  ParallelScavengeHeap* heap = ParallelScavengeHeap::heap();

  GCCauseSetter gccs(heap, _gc_cause);
  if (!_full) {
    // If (and only if) the scavenge fails, this will invoke a full gc.
    heap->invoke_scavenge();
  } else {
    heap->do_full_collection(false);
  }
}

PS_ChangeMaxHeapOp::PS_ChangeMaxHeapOp(size_t new_max_heap) :
  VM_ChangeMaxHeapOp(new_max_heap)
{}

bool DynamicMaxHeap_PsOldGenCanShrink(size_t _new_max_heap, size_t old_used_bytes, double min_heap_free_ration, size_t alignment) {
    double ratio = min_heap_free_ration / 100.0;
    double ratio_inverse = 1.0 - ratio;
    double tmp = old_used_bytes * ratio;
    size_t min_free = static_cast<size_t>(tmp / ratio_inverse);
    // align_up(min_free, alignment) alignment "must be a power of 2
    min_free = (min_free + alignment - 1) & ~(alignment - 1);
    bool can_shrink = (_new_max_heap >= (old_used_bytes + min_free));
    return can_shrink;
}

/*
 * 1. calculate new young/old gen limit size.
 * 2. trigger Full GC if necessary
 * 3. check and reset new limitation
 */
void PS_ChangeMaxHeapOp::doit() {
  ParallelScavengeHeap* heap = static_cast<ParallelScavengeHeap*>(Universe::heap());
  assert(heap->kind() == CollectedHeap::Parallel, "must be a ParallelScavengeHeap");

  // step 1
  PSOldGen* old_gen      = heap->old_gen();
  PSYoungGen* young_gen  = heap->young_gen();
  size_t cur_heap_limit  = heap->current_max_heap_size();
  size_t cur_old_limit   = old_gen->max_gen_size();
  size_t cur_young_limit = young_gen->max_gen_size();
  bool is_shrink         = _new_max_heap < cur_heap_limit;

  const size_t young_reserved_size = young_gen->reserved().byte_size();
  const size_t young_min_size = young_gen->min_gen_size();
  const size_t old_reserved_size = old_gen->reserved().byte_size();
  const size_t old_min_size = old_gen->min_gen_size();

  guarantee(cur_old_limit + cur_young_limit == cur_heap_limit, "must be");

  // fix with young gen size limitation
  size_t new_young_limit = GenArguments::scale_by_NewRatio_aligned(_new_max_heap, GenAlignment);
  new_young_limit = MIN2(new_young_limit, young_reserved_size);
  new_young_limit = MAX2(new_young_limit, young_min_size);
  // align shrink/expand direction
  if ((is_shrink && (new_young_limit > cur_young_limit)) ||
      (!is_shrink && (new_young_limit < cur_young_limit))) {
    new_young_limit = cur_young_limit;
  }
  size_t new_old_limit = _new_max_heap - new_young_limit;

  if (new_old_limit > old_reserved_size) {
    new_old_limit = old_reserved_size;
    new_young_limit = _new_max_heap - new_old_limit;
  }

  // keep the new_old_limit aligned with shrink/expand direction
  if ((is_shrink && (new_old_limit > cur_old_limit)) ||
      (!is_shrink && (new_old_limit < cur_old_limit))) {
    new_old_limit = cur_old_limit;
    new_young_limit = _new_max_heap - new_old_limit;
  }

  // After the final calcuation, check the leagle limit
  if ((new_old_limit < old_min_size) ||
      (new_old_limit > old_reserved_size) ||
      (new_young_limit < young_min_size) ||
      (new_young_limit > young_reserved_size)) {
    log_debug(dynamic, heap)("PS_ElasticMaxHeapOp abort: can not calculate new legal limit:"
             " new_old_limit: " SIZE_FORMAT "K, " "old gen min size: " SIZE_FORMAT "K, old gen reserved size: " SIZE_FORMAT "K"
             " new_young_limit: " SIZE_FORMAT "K, " "young gen min size: " SIZE_FORMAT "K, young gen reserved size: " SIZE_FORMAT "K" ,
            (new_old_limit / K), (old_min_size / K), (old_reserved_size / K),
            (new_young_limit / K), (young_min_size / K), (young_reserved_size / K));
    return;
  }

  log_debug(dynamic, heap)("PS_ElasticMaxHeapOp plan: "
    "desired young gen size (" SIZE_FORMAT "K" "->" SIZE_FORMAT "K), "
    "desired old gen size (" SIZE_FORMAT "K" "->" SIZE_FORMAT "K)",
    (cur_young_limit / K),
    (new_young_limit / K),
    (cur_old_limit / K),
    (new_old_limit / K));
  if (is_shrink) {
    guarantee(new_old_limit <= cur_old_limit && new_young_limit <= cur_young_limit, "must be");
  } else {
    guarantee(new_old_limit >= cur_old_limit && new_young_limit >= cur_young_limit, "must be");
  }

  // step2
  // Check resize legality
  if (is_shrink) {
    // check whether old/young can be resized, trigger full gc as needed
    double min_heap_free_ration = MinHeapFreeRatio;
#ifdef AARCH64
    if (min_heap_free_ration == 0) {
      min_heap_free_ration = DynamicMaxHeapShrinkMinFreeRatio;
    }
#endif //AARCH64
    bool can_shrink = DynamicMaxHeap_PsOldGenCanShrink(new_old_limit,
                                                       heap->old_gen()->used_in_bytes(),
                                                       min_heap_free_ration,
                                                       heap->old_gen()->virtual_space()->alignment());
    if (can_shrink) {
      can_shrink = (new_young_limit >= heap->young_gen()->virtual_space()->committed_size());
    }
    if (!can_shrink) {
      GCCauseSetter gccs(heap, _gc_cause);
      heap->do_full_collection(true);
      log_debug(dynamic, heap)("PS_ElasticMaxHeapOp heap after Full GC");
      LogTarget(Debug, dynamic, heap) lt;
      if (lt.is_enabled()) {
        LogStream ls(lt);
        heap->print_on(&ls);
      }
      if (young_gen->used_in_bytes() != 0) {
        log_debug(dynamic, heap)("PS_ElasticMaxHeapOp abort: young is not empty after full gc");
        return;
      }
    }

    can_shrink = DynamicMaxHeap_PsOldGenCanShrink(new_old_limit,
                                                  heap->old_gen()->used_in_bytes(),
                                                  min_heap_free_ration,
                                                  heap->old_gen()->virtual_space()->alignment());
    if (!can_shrink) {
      log_debug(dynamic, heap)("PS_ElasticMaxHeapOp abort: not enough old free for shrink");
      return;
    }

    if (old_gen->capacity_in_bytes() > new_old_limit) {
      size_t desired_free = new_old_limit - old_gen->used_in_bytes();
      char* old_high = old_gen->virtual_space()->committed_high_addr();
      old_gen->resize(desired_free);
      char* new_old_high = old_gen->virtual_space()->committed_high_addr();
      if (old_gen->capacity_in_bytes() > new_old_limit) {
        log_debug(dynamic, heap)("PS_ElasticMaxHeapOp abort: resize old fail " SIZE_FORMAT "K",
                old_gen->capacity_in_bytes() / K);
        return;
      }
      log_debug(dynamic, heap)("PS_ElasticMaxHeapOp continue: shrink old success " SIZE_FORMAT "K",
              old_gen->capacity_in_bytes() / K);
      if (old_high > new_old_high) {
        // shrink is caused by dynamic max heap, free physical memory
        size_t shrink_bytes = old_high - new_old_high;
        guarantee((shrink_bytes > 0) && (shrink_bytes % os::vm_page_size() == 0), "should be");
        bool result = os::free_heap_physical_memory(new_old_high, shrink_bytes);
        guarantee(result, "free heap physical memory should be successful");
      }
    }

    if (young_gen->virtual_space()->committed_size() > new_young_limit) {
      // entering this branch means full gc must have been triggered
      guarantee(young_gen->eden_space()->is_empty() &&
                young_gen->to_space()->is_empty() &&
                young_gen->from_space()->is_empty(),
                "must be empty");

      char* young_high = young_gen->virtual_space()->committed_high_addr();
      if (young_gen->shrink_after_full_gc(new_young_limit) == false) {
        log_debug(dynamic, heap)("PS_ElasticMaxHeapOp abort: shrink young fail");
        return;
      }
      char* new_young_high = young_gen->virtual_space()->committed_high_addr();
      log_debug(dynamic, heap)("PS_ElasticMaxHeapOp continue: shrink young success " SIZE_FORMAT "K",
              young_gen->virtual_space()->committed_size() / K);
      if (young_high > new_young_high) {
        // shrink is caused by dynamic max heap, free physical memory
        size_t shrink_bytes = young_high - new_young_high;
        guarantee((shrink_bytes > 0) && (shrink_bytes % os::vm_page_size() == 0), "should be");
        bool result = os::free_heap_physical_memory(new_young_high, shrink_bytes);
        guarantee(result, "free heap physical memory should be successful");
      }
    }
  }
  // update young/old gen limit, avoid further expand
  old_gen->set_cur_max_gen_size(new_old_limit);
  young_gen->set_cur_max_gen_size(new_young_limit);
  heap->set_current_max_heap_size(_new_max_heap);
  _resize_success = true;
  log_debug(dynamic, heap)("PS_ElasticMaxHeapOp success");
}

// Resize for DynamicHeapSize, shrink to new_size
bool PSYoungGen::shrink_after_full_gc(size_t new_size) {
  const size_t alignment = virtual_space()->alignment();
  ParallelScavengeHeap* heap = static_cast<ParallelScavengeHeap*>(Universe::heap());
  size_t orig_size = virtual_space()->committed_size();
  guarantee(eden_space()->is_empty() && to_space()->is_empty() && from_space()->is_empty(), "must be empty");
  guarantee(new_size % alignment == 0, "must be");
  guarantee(new_size < orig_size, "must be");

  // shrink virtual space
  size_t shrink_bytes = virtual_space()->committed_size() - new_size;
  bool success = virtual_space()->shrink_by(shrink_bytes);
  log_debug(dynamic, heap)("PSYoungGen::shrink_after_full_gc: shrink virtual space %s "
          "orig committed " SIZE_FORMAT "K "
          "current committed " SIZE_FORMAT "K "
          "shrink by " SIZE_FORMAT "K",
          success ? "success" : "fail",
          orig_size / K,
          virtual_space()->committed_size() / K,
          shrink_bytes / K);

  if (!success) {
    return false;
  }

  // caculate new eden/survivor size
  // shrink with same ratio, let size policy adjust later
  size_t current_survivor_ratio = eden_space()->capacity_in_bytes() / from_space()->capacity_in_bytes();
  current_survivor_ratio = MAX2(current_survivor_ratio, static_cast<size_t>(1));
  size_t new_survivor_size = new_size / (current_survivor_ratio + 2);
  new_survivor_size = align_down(new_survivor_size, SpaceAlignment);
  new_survivor_size = MAX2(new_survivor_size, SpaceAlignment);
  size_t new_eden_size = new_size - 2 * new_survivor_size;

  guarantee(new_eden_size % SpaceAlignment == 0, "must be");
  log_debug(dynamic, heap)("PSYoungGen::shrink_after_full_gc: "
          "new eden size " SIZE_FORMAT "K "
          "new survivor size " SIZE_FORMAT "K "
          "new young gen size " SIZE_FORMAT "K",
          new_eden_size / K,
          new_survivor_size / K,
          new_size / K);

  // setup new eden/survivor space
  set_space_boundaries(new_eden_size, new_survivor_size);
  post_resize();
  LogTarget(Debug, dynamic, heap) lt;
  if (lt.is_enabled()) {
    LogStream ls(lt);
    print_on(&ls);
  }
  return true;
}
