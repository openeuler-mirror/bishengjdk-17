/*
 * Copyright (c) 2020, 2023, Huawei Technologies Co., Ltd. All rights reserved.
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

import java.net.ServerSocket;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import java.util.ArrayList;
import java.util.List;

import jdk.test.lib.process.OutputAnalyzer;

import static jdk.test.lib.Asserts.*;

/*
 * jbooster testing.
 * @test
 * @summary Test jbooster server
 * @library ../lib
 *          /test/lib
 * @modules jdk.jbooster
 * @build SimpleClient
 * @run main/othervm/timeout=5000 JBoosterConcurrentTest
 */
public class JBoosterConcurrentTest extends JBoosterTestBase {
    public static final List<String> SERVER_FAST_CLEANUP_ARGS = Stream.concat(SERVER_STANDARD_ARGS.stream(), Stream.of(
        "--unused-cleanup-timeout=1"
    )).collect(Collectors.toList());

    private static void testProgramDataCleanup(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of(), SERVER_FAST_CLEANUP_ARGS);
        OutputAnalyzer out = new OutputAnalyzer(server);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        List<Process> apps = new ArrayList<>();
        final int rounds = 100;
        final int concurrent = 5;
        for (int r = 0; r < rounds; ++r) {
            if (r % 10 == 0) {
                System.out.println("Try " + (r + 1) + "round:");
            }

            ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS);
            clientArgs.add(clientArgs.size() - 1, "-XX:JBoosterProgramName=simple-" + r);

            for (int j = 0; j < concurrent; ++j) {
                Process p = java(ctx, clientArgs);
                apps.add(p);
            }

            if (r % 10 == 0) {
                for (Process p : apps) {
                    p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
                    assertEquals(p.exitValue(), 0, "good");
                }
                apps.clear();
            }
        }

        System.out.println("Get data 1:");
        writeToStdin(server, "d\n");

        for (Process p : apps) {
            p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
            assertEquals(p.exitValue(), 0, "good");
        }
        apps.clear();

        System.out.println("Get data 2:");
        writeToStdin(server, "d\n");

        Thread.sleep(5 * 60 * 1000);

        System.out.println("Get data 3:");
        writeToStdin(server, "d\n");
        writeToStdin(server, "d\n");
        writeToStdin(server, "d\n");
        exitNormallyByCommandQ(server);
        out.shouldContain("JClientProgramData:\n  -");
        out.shouldContain("JClientSessionData:\n\nJClientProgramData:\n\n".repeat(4));
    }

    public static void main(String[] args) throws Exception {
        testCases(JBoosterConcurrentTest.class);
    }
}
