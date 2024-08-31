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

#ifndef SHARE_JBOOSTER_UTILITIES_DEBUGUTILS_HPP
#define SHARE_JBOOSTER_UTILITIES_DEBUGUTILS_HPP

#include "logging/log.hpp"
#include "memory/allStatic.hpp"
#include "utilities/exceptions.hpp"
#include "utilities/globalDefinitions.hpp"

class DebugUtils: public AllStatic {
private:
#if defined(__GNUC__) && (__GNUC__ >= 3)
#define PLATFORM_FUNC_SIG __PRETTY_FUNCTION__
  static const int _func_sig_prefix_len = 53; // "static const char* DebugUtils::type_name() [with T = "
  static const int _func_sig_suffix_len = 1;  // "]"
#elif defined(_MSC_VER)
#define PLATFORM_FUNC_SIG __FUNCSIG__
  static const int _func_sig_prefix_len = 0;  // not done yet
  static const int _func_sig_suffix_len = 1;  // not done yet
#else
#define PLATFORM_FUNC_SIG __func__
  static const int _func_sig_prefix_len = 0;  // not done yet
  static const int _func_sig_suffix_len = 0;  // not done yet
#endif

public:
  static void assert_thread_in_vm() NOT_DEBUG_RETURN;
  static void assert_thread_in_native() NOT_DEBUG_RETURN;
  static void assert_thread_nonjava_or_in_native() NOT_DEBUG_RETURN;

  template <typename T>
  static const char* type_name();

  template <LogLevelType level, LogTagType T0, LogTagType T1 = LogTag::__NO_TAG, LogTagType T2 = LogTag::__NO_TAG,
            LogTagType T3 = LogTag::__NO_TAG, LogTagType T4 = LogTag::__NO_TAG, LogTagType GuardTag = LogTag::__NO_TAG>
  static void clear_java_exception_and_print_stack_trace(LogTargetImpl<level, T0, T1, T2, T3, T4, GuardTag>& lt, TRAPS);
};

#endif // SHARE_JBOOSTER_UTILITIES_DEBUGUTILS_HPP
