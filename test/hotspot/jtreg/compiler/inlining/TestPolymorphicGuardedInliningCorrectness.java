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
 * @summary Tests AArch64 polymorphic guarded inlining correctness and flags.
 * @requires os.arch == "aarch64" & vm.compiler2.enabled
 * @library /test/lib
 * @build jdk.test.lib.process.ProcessTools
 *        jdk.test.lib.process.OutputAnalyzer
 *        compiler.inlining.PolymorphicGuardedInliningTest
 * @run driver compiler.inlining.TestPolymorphicGuardedInliningCorrectness
 */

package compiler.inlining;

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

public class TestPolymorphicGuardedInliningCorrectness {
    public static void main(String[] args) throws Exception {
        testDefaultFlag();
        testCorrectness();
        testParameterCombinations();
    }

    private static void testDefaultFlag() throws Exception {
        OutputAnalyzer output = ProcessTools.executeTestJvm(
                "-XX:+PrintFlagsFinal", "-version");
        output.shouldHaveExitValue(0);
        PolymorphicGuardedInliningTest.requireMatches(output.getOutput(),
                ".*UsePolymorphicInlining\\s*=\\s*false.*",
                "UsePolymorphicInlining should be disabled by default");
    }

    private static void testCorrectness() throws Exception {
        for (int receivers = 1; receivers <= 6; receivers++) {
            String mode = "interface" + receivers;
            long baseline = PolymorphicGuardedInliningTest.runAndReadResult(
                    mode, "-XX:-UsePolymorphicInlining");
            long optimized = PolymorphicGuardedInliningTest.runAndReadResult(
                    mode,
                    "-XX:+UsePolymorphicInlining",
                    "-XX:TypeProfileWidth=" + Math.min(Math.max(receivers, 2), 4));
            PolymorphicGuardedInliningTest.assertEquals(baseline, optimized, mode);
        }
    }

    private static void testParameterCombinations() throws Exception {
        PolymorphicGuardedInliningTest.compareWithBaseline("interface4",
                "-XX:+UsePolymorphicInlining",
                "-XX:TypeProfileWidth=2");
        PolymorphicGuardedInliningTest.compareWithBaseline("interface4",
                "-XX:+UsePolymorphicInlining",
                "-XX:TypeProfileWidth=4");
        for (int percent : new int[] { 0, 20, 30, 100 }) {
            PolymorphicGuardedInliningTest.compareWithBaseline("skewed4",
                    "-XX:+UsePolymorphicInlining",
                    "-XX:TypeProfileWidth=4",
                    "-XX:TypeProfileMinimumReceiverPercent=" + percent);
        }
        PolymorphicGuardedInliningTest.compareWithBaseline("virtualLarge",
                "-XX:+UsePolymorphicInlining",
                "-XX:TypeProfileWidth=3",
                "-XX:+UseOnlyInlinedPolymorphic",
                "-XX:MaxInlineSize=1",
                "-XX:FreqInlineSize=1");
        PolymorphicGuardedInliningTest.compareWithBaseline("virtualLarge",
                "-XX:+UsePolymorphicInlining",
                "-XX:TypeProfileWidth=3",
                "-XX:-UseOnlyInlinedPolymorphic",
                "-XX:MaxInlineSize=1",
                "-XX:FreqInlineSize=1");
        PolymorphicGuardedInliningTest.compareWithBaseline("interface4",
                "-XX:+UsePolymorphicInlining",
                "-XX:TypeProfileWidth=4",
                "-XX:-UseTypeProfile");
    }
}
