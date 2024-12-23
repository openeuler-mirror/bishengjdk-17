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
 * @summary Test JBolt VM options.
 * @library /test/lib
 * @requires vm.flagless
 *
 * @run driver compiler.codecache.jbolt.JBoltVMOptionsTest
 */

package compiler.codecache.jbolt;

import java.io.File;
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import jdk.test.lib.Utils;

public class JBoltVMOptionsTest {
  public static final String SRC_DIR = Utils.TEST_SRC;
  public static final String TEMP_FILE = SRC_DIR + "/tmp.log";

  public static void main(String[] args) throws Exception {
    test1();
    test2();
    test3();
    test4();
  }

  private static void clearTmpFile() {
    File tmp = new File(TEMP_FILE);
    if (tmp.exists()) {
      tmp.delete();
    }
  }

  private static void test1() throws Exception {
    ProcessBuilder pb1 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltDumpMode",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pb2 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltLoadMode",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pb3 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltLoadMode",
      "-XX:+JBoltDumpMode",
      "-XX:JBoltOrderFile=" + SRC_DIR + "/o1.log",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pb4 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:JBoltOrderFile=" + TEMP_FILE,
      "-Xlog:jbolt*=trace",
      "--version"
    );

    OutputAnalyzer out1 = new OutputAnalyzer(pb1.start());
    OutputAnalyzer out2 = new OutputAnalyzer(pb2.start());
    OutputAnalyzer out3 = new OutputAnalyzer(pb3.start());
    OutputAnalyzer out4 = new OutputAnalyzer(pb4.start());

    String stdout;

    stdout = out1.getStdout();
    if (!stdout.contains("JBoltOrderFile is not set!")) {
      throw new RuntimeException(stdout);
    }

    stdout = out2.getStdout();
    if (!stdout.contains("JBoltOrderFile is not set!")) {
      throw new RuntimeException(stdout);
    }

    stdout = out3.getStdout();
    if (!stdout.contains("Do not set both JBoltDumpMode and JBoltLoadMode!")) {
      throw new RuntimeException(stdout);
    }

    stdout = out4.getStdout();
    if (!stdout.contains("JBoltOrderFile is ignored because it is in auto mode.")) {
      throw new RuntimeException(stdout);
    }
  }

  private static void test2() throws Exception {
    ProcessBuilder pb1 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+PrintFlagsFinal",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pb2 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltDumpMode",
      "-XX:JBoltOrderFile=" + TEMP_FILE,
      "-XX:+PrintFlagsFinal",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pb3 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltLoadMode",
      "-XX:JBoltOrderFile=" + SRC_DIR + "/o1.log",
      "-XX:+PrintFlagsFinal",
      "-Xlog:jbolt*=trace",
      "--version"
    );

    OutputAnalyzer out1 = new OutputAnalyzer(pb1.start());
    OutputAnalyzer out2 = new OutputAnalyzer(pb2.start());
    OutputAnalyzer out3 = new OutputAnalyzer(pb3.start());

    String stdout;

    stdout = out1.getStdout().replaceAll(" +", "");
    if (!stdout.contains("JBoltDumpMode=false") || !stdout.contains("JBoltLoadMode=false")) {
      throw new RuntimeException(stdout);
    }

    stdout = out2.getStdout().replaceAll(" +", "");
    if (!stdout.contains("JBoltDumpMode=true") || !stdout.contains("JBoltLoadMode=false")) {
      throw new RuntimeException(stdout);
    }

    clearTmpFile();
    
    stdout = out3.getStdout().replaceAll(" +", "");
    if (!stdout.contains("JBoltDumpMode=false") || !stdout.contains("JBoltLoadMode=true")) {
      throw new RuntimeException(stdout);
    }
  }

  private static void test3() throws Exception {
    ProcessBuilder pbF0 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltLoadMode",
      "-XX:JBoltOrderFile=" + TEMP_FILE,
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pbF1 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltLoadMode",
      "-XX:JBoltOrderFile=" + SRC_DIR + "/o1.log",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pbF2 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltLoadMode",
      "-XX:JBoltOrderFile=" + SRC_DIR + "/o2.log",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pbF3 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltLoadMode",
      "-XX:JBoltOrderFile=" + SRC_DIR + "/o3.log",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pbF4 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+UseJBolt",
      "-XX:+JBoltLoadMode",
      "-XX:JBoltOrderFile=" + SRC_DIR + "/o4.log",
      "-Xlog:jbolt*=trace",
      "--version"
    );

    OutputAnalyzer outF0 = new OutputAnalyzer(pbF0.start());
    OutputAnalyzer outF1 = new OutputAnalyzer(pbF1.start());
    OutputAnalyzer outF2 = new OutputAnalyzer(pbF2.start());
    OutputAnalyzer outF3 = new OutputAnalyzer(pbF3.start());
    OutputAnalyzer outF4 = new OutputAnalyzer(pbF4.start());

    String stdout;

    stdout = outF0.getStdout();
    if (!stdout.contains("JBoltOrderFile does not exist or cannot be accessed!")) {
      throw new RuntimeException(stdout);
    }

    stdout = outF1.getStdout();
    if (!stdout.contains("Wrong format of JBolt order line! line=\"X 123 aa bb cc\".")) {
      throw new RuntimeException(stdout);
    }

    stdout = outF2.getStdout();
    if (!stdout.contains("Wrong format of JBolt order line! line=\"M aa/bb/C dd ()V\".")) {
      throw new RuntimeException(stdout);
    }

    stdout = outF3.getStdout();
    if (!stdout.contains("Duplicated method: {aa/bb/CC dd ()V}!")) {
      throw new RuntimeException(stdout);
    }

    stdout = outF4.getStdout();
    if (stdout.contains("Error occurred during initialization of VM")) {
      throw new RuntimeException(stdout);
    }
    outF4.shouldHaveExitValue(0);

    clearTmpFile();
  }

  private static void test4() throws Exception {
    ProcessBuilder pb1 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+JBoltDumpMode",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pb2 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:+JBoltLoadMode",
      "-Xlog:jbolt*=trace",
      "--version"
    );
    ProcessBuilder pb3 = ProcessTools.createJavaProcessBuilder(
      "-XX:+UnlockExperimentalVMOptions",
      "-XX:JBoltOrderFile=" + TEMP_FILE,
      "-Xlog:jbolt*=trace",
      "--version"
    );

    OutputAnalyzer out1 = new OutputAnalyzer(pb1.start());
    OutputAnalyzer out2 = new OutputAnalyzer(pb2.start());
    OutputAnalyzer out3 = new OutputAnalyzer(pb3.start());

    String stdout;

    stdout = out1.getStdout();
    if (!stdout.contains("Do not set VM option JBoltDumpMode without UseJBolt enabled.")) {
      throw new RuntimeException(stdout);
    }

    stdout = out2.getStdout();
    if (!stdout.contains("Do not set VM option JBoltLoadMode without UseJBolt enabled.")) {
      throw new RuntimeException(stdout);
    }

    stdout = out3.getStdout();
    if (!stdout.contains("Do not set VM option JBoltOrderFile without UseJBolt enabled.")) {
      throw new RuntimeException(stdout);
    }

    clearTmpFile();
  }
}
