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
 * @test TestNUMANodes
 * @summary Test NUMANodes parameter with different node specifications
 * @library /test/lib
 * @requires os.family == "linux" & os.arch == "aarch64"
 * @run driver TestNUMANodes
 */
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import java.io.File;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class TestNUMANodes {

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

    public static void main(String[] args) throws Exception {
        int nodeCount = getNumaNodeCount();
        if (nodeCount <= 1) {
            System.out.println("SKIP: Need multi-node NUMA system");
            return;
        }

        // Test Case 1: Single-node "0"
        testSingleNode();

        // Test Case 2: Node range "0-1"
        testNodeRange();

        // Test Case 3: Node enumeration "0,2"
        if (nodeCount >= 3) {
            testNodeList();
        }

        // Test Case 4: Mixed format "0-1,3"
        if (nodeCount >= 4) {
            testMixedFormat();
        }

        // Test Case 5: Invalid node range
        testInvalidNodeRange();

        System.out.println("All NUMANodes tests passed!");
    }

    private static void testSingleNode() throws Exception {
        System.out.println("\n=== Test Case 1: NUMANodes=0 ===");
        
        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodes=0",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldContain("NUMANodes parameter specified: 0");
        output.shouldContain("User specified node 0 is allowed");

        System.out.println("PASS: Single node specification works");
    }

    private static void testNodeRange() throws Exception {
        System.out.println("\n=== Test Case 2: NUMANodes=0-1 ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodes=0-1",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldContain("NUMANodes parameter specified: 0-1");
        output.shouldContain("User specified node 0 is allowed");
        output.shouldContain("User specified node 1 is allowed");
        
        System.out.println("PASS: Node range specification works");
    }

    private static void testNodeList() throws Exception {
        System.out.println("\n=== Test Case 3: NUMANodes=0,2 ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodes=0,2",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldContain("NUMANodes parameter specified: 0,2");

        System.out.println("PASS: Node list specification works");
    }

    private static void testMixedFormat() throws Exception {
        System.out.println("\n=== Test Case 4: NUMANodes=0-1,3 ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodes=0-1,3",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        output.shouldContain("NUMANodes parameter specified: 0-1,3");

        System.out.println("PASS: Mixed format specification works");
    }
    
    private static void testInvalidNodeRange() throws Exception {
        System.out.println("\n=== Test Case 5: Invalid NUMANodes=99 ===");

        OutputAnalyzer output = ProcessTools.executeTestJava(
            "-XX:+UseNUMA",
            "-XX:NUMANodes=999",
            "-XX:+LogNUMANodes",
            "-version"
        );

        output.shouldHaveExitValue(0);
        // There should be warnings but no core dump
        output.shouldMatch("Failed to parse NUMANodes|No available NUMA nodes");

        System.out.println("PASS: Invalid node range handled gracefully");
    }
}
