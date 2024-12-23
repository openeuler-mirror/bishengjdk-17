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
  * @run main/othervm/timeout=5000 JBoosterSharedCacheTest
  */
 public class JBoosterSharedCacheTest extends JBoosterTestBase {

    public static boolean checkFileNumAndCacheType(String cacheType, int num) throws Exception {
        File dir = new File(SERVER_CACHE_PATH);
        File[] fileList = dir.listFiles();
        if (fileList.length != num) {
            return false;
        }

        if (fileList != null){
            for (File subFile : fileList) {
                if (subFile.isFile() && subFile.getName().endsWith(cacheType)){
                    return true;
                }
            }
        }
        return false;
    }

    public static boolean checkCacheFilesSameProgram() throws Exception {
        File dir = new File(SERVER_CACHE_PATH);
        File[] fileList = dir.listFiles();
        String hash = null;
        for (File subFile : fileList) {
            String file_hash = subFile.getName().split("-")[2];
            if (hash == null) {
                hash = file_hash;
            } else {
                if (!hash.equals(file_hash)) {
                    return false;
                }
            }
        }
        return true;
    }

    private static void testCacheAtLevel1(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of("-Xlog:jbooster*=trace"), SERVER_STANDARD_ARGS);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS_WITHOUT_LEVEL);
        clientArgs.add(4, "-XX:BoostStopAtLevel=1");
        Process p = java(ctx, clientArgs);
        p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        assertEquals(checkFileNumAndCacheType("clr.log", 1), true, "clr not generated.");
        exitNormallyByCommandQ(server);
    }

    private static void testCacheAtLevel2(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of("-Xlog:jbooster*=trace"), SERVER_STANDARD_ARGS);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS_WITHOUT_LEVEL);
        clientArgs.add(4, "-XX:BoostStopAtLevel=2");
        Process p = java(ctx, clientArgs);
        p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        assertEquals(checkFileNumAndCacheType("cds-agg.jsa", 2), true, "cds-agg not generated.");
        exitNormallyByCommandQ(server);
    }

    private static void testCacheAtLevel3(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of("-Xlog:jbooster*=trace"), SERVER_STANDARD_ARGS);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS_WITHOUT_LEVEL);
        clientArgs.add(4, "-XX:BoostStopAtLevel=3");
        Process p = java(ctx, clientArgs);
        p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        Thread.currentThread().sleep(5000);
        assertEquals(checkFileNumAndCacheType("aot-static.so", 3), true, "aot-static not generated.");
        exitNormallyByCommandQ(server);
    }

    private static void testCacheAtLevel4(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of("-Xlog:jbooster*=trace"), SERVER_STANDARD_ARGS);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS_WITHOUT_LEVEL);
        clientArgs.add(4, "-XX:BoostStopAtLevel=4");
        Process p = java(ctx, clientArgs);
        p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        Thread.currentThread().sleep(5000);
        assertEquals(checkFileNumAndCacheType("aot-pgo.so", 4), true, "aot-pgo not generated.");
        exitNormallyByCommandQ(server);
    }

    private static void testCacheWithDyCDS(TestContext ctx) throws Exception {
        Process server = jbooster(ctx, List.of("-Xlog:jbooster*=trace"), SERVER_STANDARD_ARGS);
        server.waitFor(WAIT_START_TIME, TimeUnit.SECONDS);

        ArrayList<String> clientArgs = new ArrayList<>(CLIENT_STANDARD_ARGS_WITHOUT_LEVEL);
        clientArgs.add(4, "-XX:BoostStopAtLevel=4");
        clientArgs.add(5, "-XX:-UseAggressiveCDS");
        Process p = java(ctx, clientArgs);
        p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);

        assertEquals(checkFileNumAndCacheType("cds-dy.jsa", 5), true, "cds-dy not generated.");
        assertEquals(checkCacheFilesSameProgram(), true, "hash not same.");
        exitNormallyByCommandQ(server);
    }

    public static void main(String[] args) throws Exception {
        testCases(JBoosterSharedCacheTest.class);
    }
 }
