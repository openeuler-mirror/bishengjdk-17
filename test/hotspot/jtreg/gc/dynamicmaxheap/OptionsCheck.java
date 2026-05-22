/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

package gc.dynamicmaxheap;

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.JDKToolFinder;
import jdk.test.lib.process.ProcessTools;
import jdk.test.lib.Asserts;

/**
 * @test OptionsCheck
 * @summary test invalid options combinations with elastic max heap
 * @requires (os.family == "linux") & (os.arch == "aarch64")
 * @library /test/lib
 * @build gc.dynamicmaxheap.TestBase
 * @run driver gc.dynamicmaxheap.OptionsCheck
 */
public class OptionsCheck extends TestBase {
    public static void main(String[] args) throws Exception {
        String[] key_output = {
            "can not be used with",
        };
        String[] key_output2 = {
            "should be used with",
        };
        LaunchAndCheck(key_output, null, "-XX:+ElasticMaxHeap", "-Xmn200M", "-version");
        LaunchAndCheck(key_output, null, "-XX:+ElasticMaxHeap", "-XX:MaxNewSize=300M", "-version");
        LaunchAndCheck(key_output, null, "-XX:+ElasticMaxHeap", "-XX:OldSize=1G", "-version");
        LaunchAndCheck(key_output2, null, "-XX:+ElasticMaxHeap", "-XX:-UseAdaptiveSizePolicy", "-version");
        String[] contains1 = {
            "-XX:ElasticMaxHeapSize should be used together with -Xmx/-XX:MaxHeapSize"
        };
        LaunchAndCheck(contains1, null, "-XX:+ElasticMaxHeap", "-XX:ElasticMaxHeapSize=100M", "-version");
        String[] contains2 = {
            "-XX:ElasticMaxHeapSize should be larger than -Xmx/-XX:MaxHeapSize"
        };
        LaunchAndCheck(contains2, null, "-XX:+ElasticMaxHeap", "-XX:ElasticMaxHeapSize=1G", "-Xmx2G", "-version");
    }

    public static void LaunchAndCheck(String[] contains, String[] not_contains, String... command) throws Exception {
        ProcessBuilder pb = ProcessTools.createLimitedTestJavaProcessBuilder(command);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        if (contains != null) {
            for (String s : contains) {
                output.shouldContain(s);
            }
        }
        if (not_contains != null) {
            for (String s : contains) {
                output.shouldNotContain(s);
            }
        }
    }
}
