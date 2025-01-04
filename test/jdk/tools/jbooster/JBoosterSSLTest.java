/*
 * Copyright (c) 2020, 2024, Huawei Technologies Co., Ltd. All rights reserved.
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

 import java.io.File;
 import java.util.List;
 import java.util.ArrayList;
 import java.util.concurrent.TimeUnit;
 import java.util.regex.Pattern;

import static jdk.test.lib.Asserts.*;

/*
* jbooster testing.
* @test
* @summary Test jbooster server
* @library ../lib
*          /test/lib
* @modules jdk.jbooster
* @build SimpleClient
* @run main/othervm/timeout=5000 JBoosterSSLTest
*/
public class JBoosterSSLTest extends JBoosterTestBase {
    private static void testSSLCorrectArgs(TestContext ctx) throws Exception {
        ArrayList<String> serverArgs = new ArrayList<>(SERVER_STANDARD_ARGS);

        final String sep = File.separator;
        String testSrc = System.getProperty("test.src", "") + sep;

        serverArgs.add(serverArgs.size() - 1, String.format("--ssl-cert=%sserver-cert.pem", testSrc));
        serverArgs.add(serverArgs.size() - 1, String.format("--ssl-key=%sserver-key.pem", testSrc));
        Process server = jbooster(ctx, List.of(), serverArgs);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS);
        clientArgs.add(4, String.format("-XX:JBoosterServerSSLRootCerts=%sserver-cert.pem", testSrc));
        Process client = java(ctx, clientArgs);
        client.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        assertEquals(client.exitValue(), 0, "good");
        exitNormallyByCommandQ(server);
    }

    private static void testSSLNoArgs(TestContext ctx) throws Exception {
        ArrayList<String> serverArgs = new ArrayList<>(SERVER_STANDARD_ARGS);
        Process server = jbooster(ctx, List.of(), SERVER_STANDARD_ARGS);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS);
        Process client = java(ctx, CLIENT_STANDARD_ARGS);
        client.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        assertEquals(client.exitValue(), 0, "good");
        exitNormallyByCommandQ(server);
    }

    private static void testSSLDifferentCert(TestContext ctx) throws Exception {
        ArrayList<String> serverArgs = new ArrayList<>(SERVER_STANDARD_ARGS);

        final String sep = File.separator;
        String testSrc = System.getProperty("test.src", "") + sep;

        serverArgs.add(serverArgs.size() - 1, String.format("--ssl-cert=%sserver-cert.pem", testSrc));
        serverArgs.add(serverArgs.size() - 1, String.format("--ssl-key=%sserver-key.pem", testSrc));
        Process server = jbooster(ctx, List.of(), serverArgs);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS);
        clientArgs.add(4, String.format("-XX:JBoosterServerSSLRootCerts=%sunrelated-cert.pem", testSrc));
        Process client = java(ctx, clientArgs);
        client.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        assertNotEquals(client.exitValue(), 0, "Both certs must be the same.");
        exitNormallyByCommandQ(server);
    }

    private static void testSSLClientNoArg(TestContext ctx) throws Exception {
        ArrayList<String> serverArgs = new ArrayList<>(SERVER_STANDARD_ARGS);

        final String sep = File.separator;
        String testSrc = System.getProperty("test.src", "") + sep;

        serverArgs.add(serverArgs.size() - 1, String.format("--ssl-cert=%sserver-cert.pem", testSrc));
        serverArgs.add(serverArgs.size() - 1, String.format("--ssl-key=%sserver-key.pem", testSrc));
        Process server = jbooster(ctx, List.of(), serverArgs);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        Process client = java(ctx, CLIENT_STANDARD_ARGS);
        client.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        assertNotEquals(client.exitValue(), 0, "Client choose not to use SSL.");
        exitNormallyByCommandQ(server);
    }

    private static void testSSLServerNoArg(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of(), SERVER_STANDARD_ARGS);

        final String sep = File.separator;
        String testSrc = System.getProperty("test.src", "") + sep;

        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS);
        clientArgs.add(4, String.format("-XX:JBoosterServerSSLRootCerts=%sserver-cert.pem", testSrc));
        Process client = java(ctx, clientArgs);
        client.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        assertNotEquals(client.exitValue(), 0, "Server choose not to use SSL.");
        exitNormallyByCommandQ(server);
    }

    private static void testSSLServerOneArg(TestContext ctx) throws Exception {
        ArrayList<String> serverArgs1 = new ArrayList<>(SERVER_STANDARD_ARGS);

        final String sep = File.separator;
        String testSrc = System.getProperty("test.src", "") + sep;

        serverArgs1.add(serverArgs1.size() - 1, String.format("--ssl-cert=%sserver-cert.pem", testSrc));
        Process server1 = jbooster(ctx, List.of(), serverArgs1);
        server1.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);
        assertEquals(server1.exitValue(), 1, "Missing Arg --ssl-key.");

        ArrayList<String> serverArgs2 = new ArrayList<>(SERVER_STANDARD_ARGS);
        serverArgs2.add(serverArgs2.size() - 1, String.format("--ssl-key=%sserver-key.pem", testSrc));
        Process server2 = jbooster(ctx, List.of(), serverArgs2);
        server2.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);
        assertEquals(server2.exitValue(), 1, "Missing Arg --ssl-cert.");
    }

    public static void main(String[] args) throws Exception {
        testCases(JBoosterSSLTest.class);
    }
}