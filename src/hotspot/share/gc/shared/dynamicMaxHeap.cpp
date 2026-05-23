/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
 */

#include "precompiled.hpp"
#include "dynamicMaxHeap.hpp"
#include "runtime/globals_extension.hpp"
#include "os_linux.hpp"
#include "logging/logConfiguration.hpp"

size_t DynamicMaxHeapConfig::_initial_max_heap_size = 0;

VM_ChangeMaxHeapOp::VM_ChangeMaxHeapOp(size_t new_max_heap) :
  VM_GC_Operation(0, GCCause::_change_max_heap, 0, true) {
  _new_max_heap = new_max_heap;
  _resize_success = false;
}

bool VM_ChangeMaxHeapOp::skip_operation() const {
  return false;
}

/*
 * validity check
 * new current max heap must be:
 * 1. >= min_heap_byte_size
 * 2. <= max_heap_byte_size
 * 3. not equal with current_max_heap_size
 *
*/
bool CollectedHeap::check_new_max_heap_validity(size_t new_size, outputStream* st) {
#ifdef AARCH64
  if (new_size > DynamicMaxHeapSizeLimit) {
    st->print_cr("%s " SIZE_FORMAT "K exceeds maximum limit " SIZE_FORMAT "K",
                Universe::dynamic_max_heap_dcmd_name(),
                (new_size / K),
                (DynamicMaxHeapSizeLimit / K));
    return false;
  }
#endif
  if (new_size < MinHeapSize) {
    st->print_cr("%s " SIZE_FORMAT "K below minimum limit " SIZE_FORMAT "K",
                Universe::dynamic_max_heap_dcmd_name(),
                (new_size / K),
                (MinHeapSize / K));
    return false;
  }
  // don't print log if it is init shrink triggered by DynamicMaxHeapSizeLimit
  if (new_size == current_max_heap_size()) {
    st->print_cr("%s " SIZE_FORMAT "K same with current max heap size " SIZE_FORMAT "K",
                Universe::dynamic_max_heap_dcmd_name(),
                (new_size / K),
                (current_max_heap_size() / K));
    return false;
  }
  return true;
}

/*
  common check for Dynamic Max Heap
  1. DynamicMaxHeapSizeLimit/ElasticMaxHeapSize should be used together with Xmx
  2. only linux aarch hisi
  3. can not fix new/old size
  4. must support UseAdaptiveSizePolicy, otherwise all size fixed
  5. only G1GC/PSGC implemented now
  6. should larger than Xmx
*/
bool DynamicMaxHeapChecker::common_check() {
#ifdef AARCH64
  if (!FLAG_IS_CMDLINE(DynamicMaxHeapSizeLimit) && !FLAG_IS_CMDLINE(ElasticMaxHeapSize) && !ElasticMaxHeap) {
        return false;
  }
  if ((FLAG_IS_CMDLINE(DynamicMaxHeapSizeLimit) || FLAG_IS_CMDLINE(ElasticMaxHeapSize)) && !FLAG_IS_CMDLINE(MaxHeapSize)) {
        warning_and_disable("should be used together with -Xmx/-XX:MaxHeapSize");
        return false;
  }
#endif
#if !defined(LINUX) || !defined(AARCH64)
  warning_and_disable("can only be assigned on Linux aarch64");
  return false;
#endif
#ifdef AARCH64
  VM_Version::get_cpu_model();
  if (!VM_Version::is_hisi_enabled()) {
    warning_and_disable("can only be assigned on HiSi now");
    return false;
  }
#endif
  if (FLAG_IS_CMDLINE(OldSize) || FLAG_IS_CMDLINE(NewSize) || FLAG_IS_CMDLINE(MaxNewSize)) {
    warning_and_disable("can not be used with -XX:OldSize/-XX:NewSize/-XX:MaxNewSize");
    return false;
  }
  if (!UseAdaptiveSizePolicy) {
    warning_and_disable("should be used with -XX:+UseAdaptiveSizePolicy");
    return false;
  }
  if (!UseG1GC && !UseParallelGC) {
    warning_and_disable("should be used with -XX:+UseG1GC/-XX:+UseParallelGC now");
    return false;
  }
#ifdef AARCH64
  if ((FLAG_IS_CMDLINE(DynamicMaxHeapSizeLimit) || FLAG_IS_CMDLINE(ElasticMaxHeapSize)) && DynamicMaxHeapSizeLimit <= MaxHeapSize) {
    warning_and_disable("should be larger than -Xmx/-XX:MaxHeapSize");
    return false;
  }
#endif
  return true;
}

bool DynamicMaxHeapChecker::check_dynamic_max_heap_size_limit() {
#ifdef AARCH64
  if (TraceElasticMaxHeap) {
    LogConfiguration::configure_stdout(LogLevel::Debug, false, LOG_TAGS(dynamic, heap));
  }
  if (FLAG_IS_CMDLINE(ElasticMaxHeapSize)) {
    FLAG_SET_ERGO(DynamicMaxHeapSizeLimit, ElasticMaxHeapSize);
  }
  if (FLAG_IS_CMDLINE(ElasticMaxHeapShrinkMinFreeRatio)) {
    FLAG_SET_ERGO(DynamicMaxHeapShrinkMinFreeRatio, ElasticMaxHeapShrinkMinFreeRatio);
  }
#endif
  return common_check();
}

void DynamicMaxHeapChecker::warning_and_disable(const char *reason) {
#ifdef AARCH64
  warning("%s feature are not available for reason -XX:%s %s, automatically disabled",
          Universe::dynamic_max_heap_option_name(),
          Universe::dynamic_max_heap_size_limit_option_name(),
          reason);
  FLAG_SET_DEFAULT(DynamicMaxHeapSizeLimit, ScaleForWordSize(DynamicMaxHeapChecker::_default_dynamic_max_heap_size_limit * M));
  Universe::set_dynamic_max_heap_enable(false);
#endif
}
