/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Copyright (C) 2023, 2024 THL A29 Limited, a Tencent company. All rights reserved.
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
 */

/*
 * @test
 * @summary Test max direct memory can take effect after resize
 * @requires (os.family == "linux") & (os.arch == "aarch64")
 * @library /test/lib
 * @build gc.dynamicmaxheap.TestBase
 * @compile test_classes/LimitDirectMemoryTestBasic.java
 * @run driver gc.dynamicmaxheap.LimitDirectMemoryTest
 */

package gc.dynamicmaxheap;

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

public class LimitDirectMemoryTest extends TestBase {
    public static void main(String[] args) throws Exception {
        // Test1
        // init max direct memory is 200M
        // expand to 300M and alloc 300M direct memory should be fine
        String[] contains1 = {
            "allocation finish!"
        };
        String[] not_contains1 = {
            "java.lang.OutOfMemoryError: Cannot reserve "
        };
        Test("-XX:+UseParallelGC", "300M", "300", contains1, not_contains1);
        Test("-XX:+UseG1GC", "300M", "300", contains1, not_contains1);

        // Test2
        // init max direct memory is 200M
        // shrink to 50M and alloc 100M direct memory should oom
        String[] contains2 = {
            "java.lang.OutOfMemoryError: Cannot reserve "
        };
        String[] not_contains2 = {
            "allocation finish!"
        };
        Test("-XX:+UseParallelGC", "50M", "100", contains2, not_contains2);
        Test("-XX:+UseG1GC", "50M", "100", contains2, not_contains2);
    }

    private static void Test(String heap_type, String new_size, String alloc_size, String[] contains, String[] not_contains) throws Exception {
        ProcessBuilder pb = ProcessTools.createLimitedTestJavaProcessBuilder(heap_type,
                                                                  "-Dtest.jdk=" + System.getProperty("test.jdk"),
                                                                  "-XX:+ElasticMaxDirectMemory",
                                                                  "-XX:MaxDirectMemorySize=200M",
                                                                  "-Xms100M",
                                                                  "-Xmx100M",
                                                                  "LimitDirectMemoryTestBasic",
                                                                  new_size,
                                                                  alloc_size);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        CheckOutput(output, contains, not_contains);
    }

    public static void CheckOutput(OutputAnalyzer output, String[] contains, String[] not_contains) throws Exception {
        System.out.println(output.getOutput());
        if (contains != null) {
            for (String s : contains) {
                output.shouldContain(s);
            }
        }
        if (not_contains != null) {
            for (String s : not_contains) {
                output.shouldNotContain(s);
            }
        }
    }
}
