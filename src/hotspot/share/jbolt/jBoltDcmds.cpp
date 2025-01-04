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

#include "jbolt/jBoltDcmds.hpp"
#include "jbolt/jBoltControlThread.hpp"
#include "jbolt/jBoltManager.hpp"

bool register_jbolt_dcmds() {
  uint32_t full_export = DCmd_Source_Internal | DCmd_Source_AttachAPI | DCmd_Source_MBean;
  DCmdFactory::register_DCmdFactory(new DCmdFactoryImpl<JBoltStartDCmd>(full_export, true, false));
  DCmdFactory::register_DCmdFactory(new DCmdFactoryImpl<JBoltStopDCmd>(full_export, true, false));
  DCmdFactory::register_DCmdFactory(new DCmdFactoryImpl<JBoltAbortDCmd>(full_export, true, false));
  DCmdFactory::register_DCmdFactory(new DCmdFactoryImpl<JBoltDumpDCmd>(full_export, true, false));
  return true;
}

JBoltStartDCmd::JBoltStartDCmd(outputStream* output, bool heap) : DCmdWithParser(output, heap),
  _duration("duration", "Duration of time(second) in this sample.", "INT", false, "600") {
  _dcmdparser.add_dcmd_option(&_duration);
}

int JBoltStartDCmd::num_arguments() {
  ResourceMark rm;
  JBoltStartDCmd* dcmd = new JBoltStartDCmd(NULL, false);
  if (dcmd != NULL) {
    DCmdMark mark(dcmd);
    return dcmd->_dcmdparser.num_arguments();
  } else {
    return 0;
  }
}

void JBoltStartDCmd::execute(DCmdSource source, TRAPS) {
  if (!UseJBolt) {
    output()->print_cr("Unable to execute because \"UseJBolt\" is disabled.");
    return;
  }

  if (!JBoltManager::auto_mode()) {
    output()->print_cr("JBolt JCMD can only be used in auto mode.");
    return;
  }

  if (!JBoltManager::reorder_phase_is_available()) {
    output()->print_cr("Unable to start because it's working now. Stop it first.");
    return;
  }

  intx interval = _duration.is_set() ? _duration.value() : JBoltSampleInterval;

  if (interval < 0) {
    output()->print_cr("duration is set to %ld which is above range, should be in [0, %d]", interval, max_jint);
    return;
  }

  if (JBoltControlThread::notify_control_wait(interval)) {
    output()->print_cr("OK. Start a new JBolt schedule, duration=%lds.", interval);
  }
  else {
    output()->print_cr("It's busy now. Please try again later...");
  }
}

void JBoltStartDCmd::print_help(const char* name) const {
  output()->print_cr(
              "Syntax : %s [options]\n"
              "\n"
              "Options:\n"
              "\n"
              "   duration  (Optional) Duration of time(second) in this sample. (INT, default value=600)\n"
              "\n"
              "Options must be specified using the <key> or <key>=<value> syntax.\n"
              "\n"
              "Example usage:\n"
              "  $ jcmd <pid> JBolt.start\n"
              "  $ jcmd <pid> JBolt.start duration=900", name);
}

void JBoltStopDCmd::execute(DCmdSource source, TRAPS) {
  if (!UseJBolt) {
    output()->print_cr("Unable to execute because \"UseJBolt\" is disabled.");
    return;
  }

  if (!JBoltManager::auto_mode()) {
    output()->print_cr("JBolt JCMD can only be used in auto mode.");
    return;
  }

  if (!JBoltManager::reorder_phase_is_profiling()) {
    output()->print_cr("Unable to stop because it's not sampling now.");
    return;
  }

  if (JBoltControlThread::notify_sample_wait()) {
    output()->print_cr("OK.\"jbolt-jfr\" would be stopped and turn to reorder.");
  } else {
    output()->print_cr("It's busy now. Please try again later...");
  }
}

void JBoltStopDCmd::print_help(const char* name) const {
  output()->print_cr(
              "Syntax : %s\n"
              "\n"
              "Example usage:\n"
              "  $ jcmd <pid> JBolt.stop", name);
}

void JBoltAbortDCmd::execute(DCmdSource source, TRAPS) {
  if (!UseJBolt) {
    output()->print_cr("Unable to execute because \"UseJBolt\" is disabled.");
    return;
  }

  if (!JBoltManager::auto_mode()) {
    output()->print_cr("JBolt JCMD can only be used in auto mode.");
    return;
  }

  if (!JBoltManager::reorder_phase_is_profiling()) {
    output()->print_cr("Unable to abort because it's not sampling now.");
    return;
  }

  if (JBoltControlThread::notify_sample_wait(true)) {
    output()->print_cr("OK.\"jbolt-jfr\" would be aborted.");
  } else {
    output()->print_cr("It's busy now. Please try again later...");
  }
}

void JBoltAbortDCmd::print_help(const char* name) const {
  output()->print_cr(
              "Syntax : %s\n"
              "\n"
              "Example usage:\n"
              "  $ jcmd <pid> JBolt.abort", name);
}

JBoltDumpDCmd::JBoltDumpDCmd(outputStream* output, bool heap) : DCmdWithParser(output, heap),
  _filename("filename", "Name of the file to which the flight recording data is dumped", "STRING", true, NULL) {
  _dcmdparser.add_dcmd_option(&_filename);
}

int JBoltDumpDCmd::num_arguments() {
  ResourceMark rm;
  JBoltDumpDCmd* dcmd = new JBoltDumpDCmd(NULL, false);
  if (dcmd != NULL) {
    DCmdMark mark(dcmd);
    return dcmd->_dcmdparser.num_arguments();
  } else {
    return 0;
  }
}

void JBoltDumpDCmd::execute(DCmdSource source, TRAPS) {
  if (!UseJBolt) {
    output()->print_cr("Unable to execute because \"UseJBolt\" is disabled.");
    return;
  }

  if (!JBoltManager::auto_mode()) {
    output()->print_cr("JBolt JCMD can only be used in auto mode.");
    return;
  }

  const char* path = _filename.value();
  char buffer[PATH_MAX];
  char* rp = NULL;

  JBoltErrorCode ec = JBoltManager::dump_order_in_jcmd(path);
  switch (ec) {
    case JBoltOrderNULL:
      output()->print_cr("Failed: No order applied by JBolt now.");
      break;
    case JBoltOpenFileError:
      output()->print_cr("Failed: File open error or NULL: %s", path);
      break;
    case JBoltOK:
      rp = realpath(path, buffer);
      output()->print_cr("Successful: Dump to %s", buffer);
      break;
    default:
      ShouldNotReachHere();
  }
}

void JBoltDumpDCmd::print_help(const char* name) const {
  output()->print_cr(
              "Syntax : %s [options]\n"
              "\n"
              "Options:\n"
              "\n"
              "   filename  Name of the file to which the flight recording data is dumped. (STRING, no default value)\n"
              "\n"
              "Options must be specified using the <key> or <key>=<value> syntax.\n"
              "\n"
              "Example usage:\n"
              "  $ jcmd <pid> JBolt.dump filename=order.log", name);
}