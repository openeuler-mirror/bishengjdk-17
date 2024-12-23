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
 * @run main/othervm/timeout=5000 JBoosterCmdTest
 */
public class JBoosterCmdTest extends JBoosterTestBase {

    private static void testServerPort1(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of());
        p.waitFor(WAIT_SHORT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 0, "port not set");
    }

    private static void testServerPort2(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("-p", SERVER_PORT_STR));
        writeToStdin(p, "d\n");
        writeToStdin(p, "q\n");
        p.waitFor(WAIT_SHORT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 0, "usable port");
    }

    private static void testServerPort3(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("-p", "1"));
        p.waitFor(WAIT_SHORT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 0, "port < 1024");
    }

    private static void testServerPort4(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("-p", "456716"));
        p.waitFor(WAIT_SHORT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 0, "port > 65535");
    }

    private static void testServerArgFormat1(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("-p", SERVER_PORT_STR, "-t"));
        p.waitFor(WAIT_SHORT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 0, "no arg for -t");
    }

    private static void testServerArgFormat2(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("-p", SERVER_PORT_STR, "-t", "16000"));
        p.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);
        exitNormallyByCommandQ(p);
    }

    private static void testServerArgFormat3(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("-p", SERVER_PORT_STR, "-t=16000"));
        p.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);
        exitNormallyByCommandQ(p);
    }

    private static void testServerArgFormat4(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("-p", SERVER_PORT_STR, "--connection-timeout", "16000"));
        p.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);
        exitNormallyByCommandQ(p);
    }

    private static void testServerArgFormat5(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("-p", SERVER_PORT_STR, "--connection-timeout=16000"));
        p.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);
        exitNormallyByCommandQ(p);
    }

    private static void testServerArgFormat6(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("-p", "-t", "12345"));
        p.waitFor(WAIT_SHORT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 0, "no arg for -p");
    }

    private static void testServerArgFormat7(TestContext ctx) throws Exception {
        Process p = jbooster(ctx, List.of(), List.of("--help=123"));
        p.waitFor(WAIT_SHORT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 0, "--help do not need arg");
    }

    private static void testClientArg1(TestContext ctx) throws Exception {
        Process p = java(ctx, List.of("-XX:+UnlockExperimentalVMOptions", "-XX:+UseJBooster", "SimpleClient"));
        p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 1, "no port");
    }

    private static void testClientArg2(TestContext ctx) throws Exception {
        Process p = java(ctx, CLIENT_OFFLINE_ARGS);
        p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 0, "good");
    }

    private static void testClientArg3(TestContext ctx) throws Exception {
        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_OFFLINE_ARGS);
        clientArgs.add(clientArgs.size() - 1, "-XX:JBoosterProgramName=custom");
        Process p = java(ctx, clientArgs);
        p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
        assertEquals(p.exitValue(), 0, "good");
    }

    public static void main(String[] args) throws Exception {
        testCases(JBoosterCmdTest.class);
    }
}
