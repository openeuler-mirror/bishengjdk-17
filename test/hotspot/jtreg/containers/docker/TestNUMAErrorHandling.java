/*
 * Copyright (c) 2025, Huawei Technologies Co., Ltd. All rights reserved.
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
 * @test TestNUMAErrorHandling
 * @summary Test error handling and edge cases
 * @library /test/lib
 * @requires os.family == "linux" & os.arch == "aarch64"
 * @run driver TestNUMAErrorHandling
 */
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

public class TestNUMAErrorHandling {

    public static void main(String[] args) throws Exception {
        // Test Case 1: Without UseNUMA
        testWithoutUseNUMA();

        // Test Case 2: Negative parameter
        testNegativeParameter();

        // Test Case 3: Without NUMANodes & NUMANodesRandom
        testNoParametersSet();

        // Test Case 4: Invalid NUMANodes
        testInvalidNodeString();

        System.out.println("All error handling tests passed!");
    }

    private static void testWithoutUseNUMA() throws Exception {
        System.out.println("\n=== Test Case 1: Without UseNUMA ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:NUMANodesRandom=1",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldNotContain("Successfully bound to");

        System.out.println("PASS: No binding without UseNUMA flag");
    }

    private static void testNegativeParameter() throws Exception {
        System.out.println("\n=== Test Case 2: Negative parameter ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodesRandom=-1",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldNotHaveExitValue(0);
        output.shouldContain("Improperly specified VM option");

        System.out.println("PASS: Negative parameter handled");
    }

    private static void testNoParametersSet() throws Exception {
        System.out.println("\n=== Test Case 3: No parameters set ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldNotContain("will select");

        System.out.println("PASS: No binding without parameters");
    }

    private static void testInvalidNodeString() throws Exception {
        System.out.println("\n=== Test Case 4: Invalid NUMANodes format ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodes=invalid",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldMatch("Failed to parse NUMANodes|<invalid> is invalid");

        System.out.println("PASS: Invalid format handled gracefully");
    }
}
