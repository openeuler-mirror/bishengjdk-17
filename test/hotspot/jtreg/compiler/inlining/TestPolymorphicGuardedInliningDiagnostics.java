/*
 * Copyright (c) 2026, Huawei Technologies Co., Ltd. All rights reserved.
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

/**
 * @test
 * @summary Tests AArch64 polymorphic guarded inlining diagnostic output.
 * @requires os.arch == "aarch64" & vm.compiler2.enabled & vm.compMode != "Xcomp"
 * @library /test/lib
 * @build jdk.test.lib.process.ProcessTools
 *        jdk.test.lib.process.OutputAnalyzer
 *        compiler.inlining.PolymorphicGuardedInliningTest
 * @run driver compiler.inlining.TestPolymorphicGuardedInliningDiagnostics
 */

package compiler.inlining;

public class TestPolymorphicGuardedInliningDiagnostics {
    public static void main(String[] args) throws Exception {
        testTracingAndLogCompilation();
    }

    private static void testTracingAndLogCompilation() throws Exception {
        PolymorphicGuardedInliningTest.RunResult result =
                PolymorphicGuardedInliningTest.runWithLog("trace4", "trace4",
                        "-XX:+UnlockDiagnosticVMOptions",
                        "-XX:+PrintInlining",
                        "-XX:+TraceTypeProfile",
                        "-XX:+LogCompilation",
                        "-XX:+UsePolymorphicInlining",
                        "-XX:TypeProfileWidth=4");

        String output = result.output;
        for (int i = 0; i < 4; i++) {
            PolymorphicGuardedInliningTest.requireMatches(output,
                    ".*PolymorphicGuardedInliningTest\\$R" + i + "::apply.*inline.*",
                    "expected R" + i + "::apply to be reported as inlined");
            PolymorphicGuardedInliningTest.requireMatches(output,
                    ".*TypeProfile.*PolymorphicGuardedInliningTest\\$R" + i + ".*",
                    "expected TraceTypeProfile output for R" + i);
        }

        String log = result.log;
        PolymorphicGuardedInliningTest.requireContains(log, "receiver3=",
                "LogCompilation should include receiver3");
        PolymorphicGuardedInliningTest.requireContains(log, "receiver4=",
                "LogCompilation should include receiver4");
        PolymorphicGuardedInliningTest.requireMatches(log,
                ".*call_fallback[^\\n]*morphism='4'[^\\n]*trap='1'.*",
                "stable 4-receiver site should use deopt fallback");
    }
}
