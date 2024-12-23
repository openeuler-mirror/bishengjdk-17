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

#ifndef SHARE_JBOLT_JBOLTCONTROLTHREAD_HPP
#define SHARE_JBOLT_JBOLTCONTROLTHREAD_HPP

#include "runtime/thread.hpp"

/**
 * Control JBolt how to run in this thread.
 */
class JBoltControlThread : public AllStatic {
public:
  static const int SIG_NULL            = 0;
  static const int SIG_START_PROFILING = 1;
  static const int SIG_STOP_PROFILING  = 2;

private:
  static JavaThread* volatile _the_java_thread;
  // Can be notified by jcmd JBolt.start, restart a control schedule
  static Monitor* _control_wait_monitor;
  // Can be notified by jcmd JBolt.stop/abort, stop a running JFR
  static Monitor* _sample_wait_monitor;
  static jobject _thread_obj;
  static int volatile _signal;
  static bool volatile _abort;
  static intx volatile _interval;

  static void thread_entry(JavaThread* thread, TRAPS) { thread_run(thread); }
  static void thread_run(TRAPS);

  static intx sample_interval();
  static bool prev_control_schdule(TRAPS);
  static void control_schdule(TRAPS);
  static void post_control_schdule(TRAPS);

public:
  static void init(TRAPS);

  static void start_thread(TRAPS);

  static bool notify_sample_wait(bool abort = false);

  static bool notify_control_wait(intx interval);

  static JavaThread* get_thread();
};

#endif // SHARE_JBOLT_JBOLTCONTROLTHREAD_HPP
