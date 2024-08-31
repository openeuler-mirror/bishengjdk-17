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
import java.util.ArrayList;
import java.util.List;

import static jdk.test.lib.Asserts.*;

/*
 * jbooster testing.
 * @test
 * @summary Test jbooster server
 * @library ../lib
 *          /test/lib
 * @modules jdk.jbooster
 * @build SimpleClient
 * @run main/othervm/timeout=300 JBoosterNetTest
 */
public class JBoosterNetTest extends JBoosterTestBase {

    private static void testGood(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of(), SERVER_STANDARD_ARGS);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        for (int i = 0; i < 4; ++i) {
            Process p = java(ctx, CLIENT_STANDARD_ARGS);
            p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
            assertEquals(p.exitValue(), 0, "good");
        }

        exitNormallyByCommandQ(server);
    }

    private static void testCachePath(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of("-Xlog:jbooster*=trace"), SERVER_STANDARD_ARGS);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS);
        clientArgs.add(clientArgs.size() - 1, "-XX:JBoosterProgramName=simple");
        clientArgs.add(clientArgs.size() - 1, "-XX:+PrintAllClassInfo");
        clientArgs.add(clientArgs.size() - 1, "-Xlog:jbooster*=trace");

        for (int i = 0; i < 2; ++i) {
            Process p = java(ctx, clientArgs);
            p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
            assertEquals(p.exitValue(), 0, "good");
        }

        deletePath(CLIENT_CACHE_PATH);

        for (int i = 0; i < 2; ++i) {
            Process p = java(ctx, clientArgs);
            p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
            assertEquals(p.exitValue(), 0, "good after delete client cache");
        }

        deletePath(SERVER_CACHE_PATH);

        for (int i = 0; i < 2; ++i) {
            Process p = java(ctx, clientArgs);
            p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
            assertEquals(p.exitValue(), 0, "good after delete server cache");
        }

        exitNormallyByCommandQ(server);
    }

    private static void testUsedPortServer(TestContext ctx) throws Exception {
        try (ServerSocket ignored = new ServerSocket(SERVER_PORT)) {
            Process p = jbooster(ctx, List.of(), SERVER_STANDARD_ARGS);
            p.waitFor(WAIT_SHORT_TIME, TimeUnit.SECONDS);
            assertEquals(p.exitValue(), 1, "port blocked");
        }
    }

    private static void testUsedPortClient(TestContext ctx) throws Exception {
        try (ServerSocket ignored = new ServerSocket(SERVER_PORT)) {
            Process p = java(ctx, CLIENT_STANDARD_ARGS);
            p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
            assertNotEquals(p.exitValue(), 0, "port blocked");
        }
    }

    private static void testConcurrentConnection1(TestContext ctx) throws Exception {
        for (int i = 0; i < 6; ++i) {
            System.out.println("Try " + (i + 1) + ":");
            Process server = jbooster(ctx, List.of(), SERVER_STANDARD_ARGS);
            server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

            List<Process> apps = new ArrayList<>();
            for (int j = 0; j < 4; ++j) {
                Process p = java(ctx, CLIENT_STANDARD_ARGS);
                apps.add(p);
            }
            for (Process p : apps) {
                p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
                assertEquals(p.exitValue(), 0, "good");
            }

            exitNormallyByCommandQ(server);
        }
    }

    private static void testConcurrentConnection2(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of(), SERVER_STANDARD_ARGS);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        for (int i = 0; i < 12; ++i) {
            System.out.println("Try " + (i + 1) + ":");
            ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS);
            clientArgs.add(clientArgs.size() - 1, "-XX:JBoosterProgramName=simple-" + i);

            List<Process> apps = new ArrayList<>();
            for (int j = 0; j < 4; ++j) {
                Process p = java(ctx, clientArgs);
                apps.add(p);
            }
            for (Process p : apps) {
                p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
                assertEquals(p.exitValue(), 0, "good");
            }
        }

        exitNormallyByCommandQ(server);
    }

    public static void main(String[] args) throws Exception {
        testCases(JBoosterNetTest.class);
    }
}
