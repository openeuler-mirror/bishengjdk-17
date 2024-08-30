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

#ifndef SHARE_JBOOSTER_UTILITIES_DEBUGUTILS_INLINE_HPP
#define SHARE_JBOOSTER_UTILITIES_DEBUGUTILS_INLINE_HPP

#include "classfile/javaClasses.inline.hpp"
#include "jbooster/utilities/debugUtils.hpp"
#include "logging/logStream.hpp"
#include "memory/allocation.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/vmError.hpp"

/**
 * Attention: the type name string is allocated on the heap!
 */
template <typename T>
const char* DebugUtils::type_name() {
  const char* sig = PLATFORM_FUNC_SIG;
  const int sig_len = strlen(sig);
  const int res_len = sig_len - _func_sig_prefix_len - _func_sig_suffix_len;
  char* res = NEW_C_HEAP_ARRAY(char, res_len + 1, mtJBooster);
  memcpy(res, sig + _func_sig_prefix_len, res_len);
  res[res_len] = '\0';
  return res;
}

template <LogLevelType level, LogTagType T0, LogTagType T1 = LogTag::__NO_TAG, LogTagType T2 = LogTag::__NO_TAG,
          LogTagType T3 = LogTag::__NO_TAG, LogTagType T4 = LogTag::__NO_TAG, LogTagType GuardTag = LogTag::__NO_TAG>
void DebugUtils::clear_java_exception_and_print_stack_trace(LogTargetImpl<level, T0, T1, T2, T3, T4, GuardTag>& lt, TRAPS) {
  if (!HAS_PENDING_EXCEPTION) return;

  if (lt.is_enabled()) {
    ResourceMark rm(THREAD);
    Handle ex(THREAD, THREAD->pending_exception());
    CLEAR_PENDING_EXCEPTION;
    LogStream ls(lt);
    assert(THREAD->thread_state() == _thread_in_vm, "sanity");
    java_lang_Throwable::print_stack_trace(ex, &ls);
  } else {
    CLEAR_PENDING_EXCEPTION;
  }
}

#endif // SHARE_JBOOSTER_UTILITIES_DEBUGUTILS_INLINE_HPP
