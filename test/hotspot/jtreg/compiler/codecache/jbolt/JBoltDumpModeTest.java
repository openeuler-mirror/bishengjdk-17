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

/*
 * @test
 * @summary Test JBolt dump mode functions.
 * @library /test/lib
 * @requires vm.flagless
 *
 * @run driver compiler.codecache.jbolt.JBoltDumpModeTest
 */

package compiler.codecache.jbolt;

import java.io.File;
import java.io.IOException;
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import jdk.test.lib.Utils;

public class JBoltDumpModeTest {
  public static final String SRC_DIR = Utils.TEST_SRC;
  public static final String ORDER_FILE = SRC_DIR + "/order.log";

  private static void createOrderFile() {
    try {
        File order = new File(ORDER_FILE);
        if (!order.exists()) {
        order.createNewFile();
        }
    }
    catch (IOException e) {
        e.printStackTrace();
    }
  }

  private static void clearOrderFile() {
    File order = new File(ORDER_FILE);
    if (order.exists()) {
      order.delete();
    }
  }

  private static void OrderFileShouldExist() throws Exception {
    File order = new File(ORDER_FILE);
    if (order.exists()) {
      order.delete();
    }
    else {
      throw new RuntimeException(ORDER_FILE + " doesn't exist as expect.");
    }
  }

  private static void OrderFileShouldNotExist() throws Exception {
    File order = new File(ORDER_FILE);
    if (order.exists()) {
      throw new RuntimeException(ORDER_FILE + " exists while expect not.");
    }
  }

  private static void testNormalUse() throws Exception {
    ProcessBuilder pb1 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:JBoltOrderFile=" + ORDER_FILE,
      "-XX:+JBoltDumpMode",
      "-Xlog:jbolt*=trace",
      "--version"
    );

    ProcessBuilder pb2 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:JBoltOrderFile=" + ORDER_FILE,
      "-XX:+JBoltDumpMode",
      "-XX:StartFlightRecording",
      "-Xlog:jbolt*=trace",
      "--version"
    );

    ProcessBuilder pb3 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:JBoltOrderFile=" + ORDER_FILE,
      "-XX:+JBoltDumpMode",
      "-Xlog:jbolt*=trace",
      "--version"
    );

    clearOrderFile();

    String stdout;

    OutputAnalyzer out1 = new OutputAnalyzer(pb1.start());
    stdout = out1.getStdout();
    if (!stdout.contains("JBolt in dump mode now, start a JFR recording named \"jbolt-jfr\".")) {
        throw new RuntimeException(stdout);
    }
    out1.shouldHaveExitValue(0);
    OrderFileShouldExist();

    OutputAnalyzer out2 = new OutputAnalyzer(pb2.start());
    stdout = out2.getStdout();
    if (!stdout.contains("JBolt in dump mode now, start a JFR recording named \"jbolt-jfr\".")) {
        throw new RuntimeException(stdout);
    }
    out2.shouldHaveExitValue(0);
    OrderFileShouldExist();

    createOrderFile();
    OutputAnalyzer out3 = new OutputAnalyzer(pb3.start());
    stdout = out3.getStdout();
    if (!stdout.contains("JBoltOrderFile to dump already exists and will be overwritten:")) {
        throw new RuntimeException(stdout);
    }
    out3.shouldHaveExitValue(0);
    OrderFileShouldExist();
  }

  private static void testUnabletoRun() throws Exception {
    ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:JBoltOrderFile=" + ORDER_FILE,
      "-XX:+JBoltDumpMode",
      "-XX:-FlightRecorder",
      "-Xlog:jbolt*=trace",
      "--version"
    );

    clearOrderFile();

    String stdout;
    OutputAnalyzer out = new OutputAnalyzer(pb.start());

    stdout = out.getStdout();
    if (!stdout.contains("JBolt depends on JFR!")) {
        throw new RuntimeException(stdout);
    }
    OrderFileShouldNotExist();
  }

  private static void testFatalError() throws Exception {
    ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:JBoltOrderFile=" + ORDER_FILE,
      "-XX:+JBoltDumpMode",
      "-XX:foo",
      "-Xlog:jbolt*=trace",
      "--version"
    );

    clearOrderFile();

    OutputAnalyzer out = new OutputAnalyzer(pb.start());

    out.stderrShouldContain("Could not create the Java Virtual Machine");
    OrderFileShouldNotExist();
  }

  public static void main(String[] args) throws Exception {
    testNormalUse();
    testUnabletoRun();
    testFatalError();
  }
}