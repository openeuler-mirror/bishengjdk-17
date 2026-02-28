/*
 * Copyright (c) 2020, 2025, Huawei Technologies Co., Ltd. All rights reserved.
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
* @summary Test NUMA bind policy
* @requires os.arch == "aarch64" & os.family == "linux"
* @library /test/lib
* @build Args
* @run main/othervm/timeout=5000 TestNUMABindPolicy
*/

import jdk.test.lib.process.ProcessTools;
import jdk.test.lib.process.OutputAnalyzer;

public class TestNUMABindPolicy {
    static void runAndCheckWithEqual(int mem, String policy, String prefix, boolean isSuccess, String expectedOut) throws Exception {
        int cpu = 2;

        OutputAnalyzer out = ProcessTools.executeTestJava(
                "-XX:+UseNUMA",
                "-XX:+LogNUMANodes",
                "-XX:NUMANodesRandom=" + cpu,
                "-XX:NUMAMemNodesRandom=" + mem,
                "-XX:NUMABindPolicy=" + policy,
                "Args",
                "--prefix" + prefix
        );

        if (isSuccess) {
            out.shouldHaveExitValue(0);
            out.shouldMatch(".*Successfully bound cpu to " + cpu + " node\\(s\\): ([0-9]+,){" + (cpu - 1) + "}[0-9]+");
            if (cpu > mem) {
                out.shouldMatch(".*Successfully bound mem to " + cpu + " node\\(s\\): ([0-9]+,){" + (cpu - 1) + "}[0-9]+");
            } else {
                out.shouldMatch(".*Successfully bound mem to " + mem + " node\\(s\\): ([0-9]+,){" + (mem - 1) + "}[0-9]+");
            }
        } else {
            out.shouldHaveExitValue(0);
            out.shouldContain(expectedOut);
        }
    }

    static void runAndCheckWithoutEqual() throws Exception {
        OutputAnalyzer out = ProcessTools.executeTestJava(
                "-XX:+UseNUMA",
                "-XX:+LogNUMANodes",
                "-XX:NUMANodesRandom=" + 2,
                "-XX:NUMAMemNodesRandom=" +2,
                "-XX:NUMABindPolicy=prefix=--prefix,div=1",
                "Args",
                "--prefix",
                "-1"
        );

        out.shouldHaveExitValue(0);
        out.shouldContain("Invalid prefix id, feature disabled.");
    }

    public static void main(String[] args) throws Exception {
        // test nodes bound to mem is no less than that to cpu
        runAndCheckWithEqual(2, "prefix=--prefix,div=1", "=1", true, "");
        runAndCheckWithEqual(3, "prefix=--prefix,div=1", "=1", true, "");

        // test nodes bound to mem is less than that to cpu
        runAndCheckWithEqual(1, "prefix=--prefix,div=1", "=1", true, "");

        // test NUMABindPolicy is imcomplete
        runAndCheckWithEqual(3, "prefix=--prefix,abc=1", "=1", false, "Lack of prefix/div. Feature disabled.");

        // test with invalid prefix
        runAndCheckWithEqual(3, "prefix=,div=1", "=1", false, "NUMABindPolicy: prefix cannot be empty.");
        runAndCheckWithEqual(3, "prefix=abc,div=1", "=1", false, "Cannot find corresponding process according to prefix. Feature disabled.");
        runAndCheckWithEqual(3, "prefix=--prefix,div=1", "", false, "Cannot find corresponding process according to prefix. Feature disabled.");
        runAndCheckWithEqual(3, "prefix=--prefix,div=1", "=-1", false, "Invalid prefix id, feature disabled.");
        runAndCheckWithoutEqual();

        // test with invalid div
        runAndCheckWithEqual(3, "prefix=--prefix,div=0", "=1", false, "NUMABindPolicy: div must be a positive integer, got '0'");
        runAndCheckWithEqual(3, "prefix=--prefix,div=abc", "=1", false, "NUMABindPolicy: div must be a positive integer, got 'abc'");

    }
}