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
 * @summary Test Basic Elastic Max Direct Memory resize
 * @requires (os.family == "linux") & (os.arch == "aarch64")
 * @library /test/lib
 * @build gc.dynamicmaxheap.TestBase
 * @compile test_classes/NotActiveDirectMemory.java
 * @run driver gc.dynamicmaxheap.DirectMemoryBasicTest
 */

package gc.dynamicmaxheap;

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.JDKToolFinder;
import jdk.test.lib.process.ProcessTools;
import jdk.test.lib.Asserts;

public class DirectMemoryBasicTest extends TestBase {
    public static void main(String[] args) throws Exception {
        test("-XX:+UseParallelGC");
        test("-XX:+UseG1GC");
    }

    private static void test(String heap_type) throws Exception {
        String architecture = System.getProperty("os.arch");
        ProcessBuilder pb = ProcessTools.createLimitedTestJavaProcessBuilder(heap_type,
                                                                  "-XX:+ElasticMaxDirectMemory",
                                                                  "-Xms100M",
                                                                  "-Xmx100M",
                                                                  "-XX:MaxDirectMemorySize=200M",
                                                                  "NotActiveDirectMemory");
        Process p = pb.start();
        long pid;
        try {
            pid = p.pid();
            System.out.println(pid);

            // NotActiveDirectMemory will alloc 100M direct memory
            // expand to 300M should be fine for any GC
            String[] contains1 = {
                "GC.elastic_max_direct_memory (",
                "GC.elastic_max_direct_memory success"
            };
            resizeAndCheck(pid, "300M", contains1, null, "GC.elastic_max_direct_memory");

            // shrink to 50M should be fail for any GC,
            String[] contains2 = {
                "below current reserved direct memory",
                "GC.elastic_max_direct_memory fail"
            };
            resizeAndCheck(pid, "50M", contains2, null, "GC.elastic_max_direct_memory");
        } finally {
            p.destroy();
        }
    }
}
