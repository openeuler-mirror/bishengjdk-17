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
 * @test TestNUMANodesRandom
 * @summary Test NUMANodesRandom parameter with different values
 * @library /test/lib
 * @requires os.family == "linux" & os.arch == "aarch64"
 * @run driver TestNUMANodesRandom
 */
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import jdk.test.lib.Platform;
import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class TestNUMANodesRandom {

    private static int getNumaNodeCount() throws Exception {
        // Read /sys/devices/system/node/possible for node number
        File nodeFile = new File("/sys/devices/system/node/possible");
        if (!nodeFile.exists()) {
            return -1; // NUMA is unavailable
        }

        ProcessBuilder pb = new ProcessBuilder("numactl", "-H");
        Process p = pb.start();
        p.waitFor();

        // Simply Check if numactl is available
        if (p.exitValue() != 0) {
            return -1;
        }

        // Determine the number of nodes through the output of numactl-H
        String output = new String(p.getInputStream().readAllBytes());
        Pattern pattern = Pattern.compile("available: (\\d+) nodes");
        Matcher matcher = pattern.matcher(output);
        if (matcher.find()) {
            return Integer.parseInt(matcher.group(1));
        }
        return -1;
    }

    private static boolean hasLibnuma() {
        try {
            ProcessBuilder pb = new ProcessBuilder("ldconfig", "-p");
            Process p = pb.start();
            String output = new String(p.getInputStream().readAllBytes());
            return output.contains("libnuma.so");
        } catch (Exception e) {
            return false;
        }
    }

    public static void main(String[] args) throws Exception {
        // Check the preconditions
        if (!hasLibnuma()) {
            System.out.println("SKIP: libnuma not available");
            return;
        }

        int nodeCount = getNumaNodeCount();
        if (nodeCount <= 0) {
            System.out.println("SKIP: NUMA not available or single node system");
            return;
        }

        System.out.println("Detected " + nodeCount + " NUMA nodes");
        
        // Test Case 1: Select one random node
        testSingleNodeRandom();

        // Test Case 2: Select multiple random nodes
        if (nodeCount > 2) {
            testMultipleNodesRandom(2);
        }

        // Test Case 3: NUMANodesRandom exceeds the number of nodes
        testRandomExceedingNodeCount(nodeCount);

        // Test Case 4: NUMANodesRandom=0 (Not enabled)
        testRandomZero();

        // Test Case 5: Verify the environment variable tags
        testEnvVarPreventsReexecution();

        System.out.println("All tests passed!");
    }

    private static void testSingleNodeRandom() throws Exception {
        System.out.println("\n=== Test Case 1: NUMANodesRandom=1 ===");
        
        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodesRandom=1",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldContain("will select 1 nodes from");
        output.shouldContain("Successfully bound cpu to 1 node(s)");

        System.out.println("PASS: Single node random selection works");
    }

    private static void testMultipleNodesRandom(int count) throws Exception {
        System.out.println("\n=== Test Case 2: NUMANodesRandom=" + count + " ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodesRandom=" + count,
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldContain("will select " + count + " nodes from");
        output.shouldContain("Distance from node");
        output.shouldContain("Successfully bound cpu to " + count + " node(s)");

        System.out.println("PASS: Multiple nodes random selection works");
    }

    private static void testRandomExceedingNodeCount(int nodeCount) throws Exception {
        System.out.println("\n=== Test Case 3: NUMANodesRandom exceeds node count ===");

        int exceedingCount = nodeCount + 5;
        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodesRandom=" + exceedingCount,
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        // All available nodes should be used
        output.shouldContain("will select " + nodeCount + " nodes from " + nodeCount);

        System.out.println("PASS: Exceeding count handled correctly");
    }

    private static void testRandomZero() throws Exception {
        System.out.println("\n=== Test Case 4: NUMANodesRandom=0 ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodesRandom=0",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldNotContain("Successfully bound to");

        System.out.println("PASS: NUMANodesRandom=0 disables binding");
    }

    private static void testEnvVarPreventsReexecution() throws Exception {
        System.out.println("\n=== Test Case 5: Environment variable prevents re-execution ===");

        ProcessBuilder pb = ProcessTools.createTestJavaProcessBuilder(
            "-XX:+UseNUMA",
            "-XX:NUMANodesRandom=1",
            "-XX:+LogNUMANodes",
            "-version"
        );

        pb.environment().put("_JVM_NUMA_BINDING_DONE", "1");

        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldHaveExitValue(0);
        output.shouldContain("NUMA binding already done");
        output.shouldNotContain("Successfully bound to");

        System.out.println("PASS: Environment variable prevents re-execution");
    }
}
