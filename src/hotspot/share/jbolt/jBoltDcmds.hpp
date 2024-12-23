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

#ifndef SHARE_JBOLT_JBOLTDCMDS_HPP
#define SHARE_JBOLT_JBOLTDCMDS_HPP

#include "services/diagnosticCommand.hpp"

class JBoltStartDCmd : public DCmdWithParser {
 protected:
  DCmdArgument<jlong> _duration;
 public:
  JBoltStartDCmd(outputStream* output, bool heap);

  static const char* name() {
    return "JBolt.start";
  }
  static const char* description() {
    return "Starts a new JBolt sample schedule(fail if sampling)";
  }
  static const char* impact() {
    return "Medium: Depending on JFR that JBolt rely on, the impact can range from low to high.";
  }
  static const JavaPermission permission() {
    JavaPermission p = {"java.lang.management.ManagementPermission", "control", NULL};
    return p;
  }
  static int num_arguments();
  virtual void execute(DCmdSource source, TRAPS);
  virtual void print_help(const char* name) const;
};

class JBoltStopDCmd : public DCmd {
 public:
  JBoltStopDCmd(outputStream* output, bool heap) : DCmd(output, heap) {}

  static const char* name() {
    return "JBolt.stop";
  }
  static const char* description() {
    return "Stop a running JBolt sample schedule and reorder immediately(fail if not sampling)";
  }
  static const char* impact() {
    return "Low";
  }
  static const JavaPermission permission() {
    JavaPermission p = {"java.lang.management.ManagementPermission", "control", NULL};
    return p;
  }
  static int num_arguments() {
    return 0;
  }

  virtual void execute(DCmdSource source, TRAPS);
  virtual void print_help(const char* name) const;
};

class JBoltAbortDCmd : public DCmd {
 public:
  JBoltAbortDCmd(outputStream* output, bool heap) : DCmd(output, heap) {}

  static const char* name() {
    return "JBolt.abort";
  }
  static const char* description() {
    return "Stop a running JBolt sample schedule but don't reorder(fail if not sampling)";
  }
  static const char* impact() {
    return "Low";
  }
  static const JavaPermission permission() {
    JavaPermission p = {"java.lang.management.ManagementPermission", "monitor", NULL};
    return p;
  }
  static int num_arguments() {
    return 0;
  }

  virtual void execute(DCmdSource source, TRAPS);
  virtual void print_help(const char* name) const;
};

class JBoltDumpDCmd : public DCmdWithParser {
 protected:
  DCmdArgument<char*> _filename;
 public:
  JBoltDumpDCmd(outputStream* output, bool heap);

  static const char* name() {
    return "JBolt.dump";
  }
  static const char* description() {
    return "dump an effective order to file(fail if no order)";
  }
  static const char* impact() {
    return "Low";
  }
  static const JavaPermission permission() {
    JavaPermission p = {"java.lang.management.ManagementPermission", "monitor", NULL};
    return p;
  }
  static int num_arguments();
  virtual void execute(DCmdSource source, TRAPS);
  virtual void print_help(const char* name) const;
};

bool register_jbolt_dcmds();

#endif // SHARE_JBOLT_JBOLTDCMDS_HPP