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

import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import java.util.List;

import static jdk.test.lib.Asserts.*;
import jdk.test.lib.process.ProcessTools;
import jdk.test.lib.JDKToolLauncher;
import jdk.test.lib.Platform;
import jdk.test.lib.Utils;

/**
 * A simpile framework that allows you to create a JBooster server or a
 * app easily.
 *
 * @see JcmdBase
 */
public class JBoosterTestBase {
    public static final int WAIT_START_TIME = 2;
    public static final int WAIT_SHORT_TIME = 8;
    public static final int WAIT_EXIT_TIME = 64;

    public static final int SERVER_PORT = 41567;
    public static final String SERVER_PORT_STR = "41567";

    public static final String CLIENT_CACHE_PATH = "jbooster-cache-client";
    public static final String SERVER_CACHE_PATH = "jbooster-cache-server";

    public static final List<String> CLIENT_STANDARD_ARGS = List.of(
            "-XX:+UnlockExperimentalVMOptions",
            "-XX:+UnlockDiagnosticVMOptions",
            "-XX:+UseJBooster",
            "-XX:JBoosterPort=" + SERVER_PORT_STR,
            "-XX:BoostStopAtLevel=4",
            "-XX:JBoosterCachePath=" + CLIENT_CACHE_PATH,
            "-XX:+JBoosterExitIfUnsupported",
            "-XX:+JBoosterCrashIfNoServer",
            "-XX:+SkipSharedClassPathCheck",
            "SimpleClient"
    );

    public static final List<String> CLIENT_OFFLINE_ARGS = List.of(
            "-XX:+UnlockExperimentalVMOptions",
            "-XX:+UseJBooster",
            "-XX:JBoosterPort=" + SERVER_PORT_STR,
            "-XX:JBoosterCachePath=" + CLIENT_CACHE_PATH,
            "SimpleClient"
    );

    public static final List<String> SERVER_STANDARD_ARGS = List.of(
            "--server-port=" + SERVER_PORT_STR,
            "--cache-path=" + SERVER_CACHE_PATH
    );

    private static final ProcessBuilder processBuilder = new ProcessBuilder();

    public static Process jbooster(TestContext ctx, List<String> vmArgs, List<String> jboosterArgs) throws Exception {
        JDKToolLauncher launcher = JDKToolLauncher.createUsingTestJDK("jbooster");
        launcher.addVMArgs(Utils.getTestJavaOpts());
        if (vmArgs != null) {
            for (String vmArg : vmArgs) {
                launcher.addVMArg(vmArg);
            }
        }
        if (jboosterArgs != null) {
            for (String toolArg : jboosterArgs) {
                launcher.addToolArg(toolArg);
            }
        }
        processBuilder.command(launcher.getCommand());
        System.out.println(Arrays.toString(processBuilder.command().toArray()).replace(",", ""));
        return ctx.addProcess(ProcessTools.startProcess("jbooster", processBuilder));
    }

    public static Process java(TestContext ctx, List<String> args) throws Exception {
        JDKToolLauncher launcher = JDKToolLauncher.createUsingTestJDK("java");
        if (args != null) {
            for (String arg : args) {
                launcher.addToolArg(arg);
            }
        }
        processBuilder.command(launcher.getCommand());
        System.out.println(Arrays.toString(processBuilder.command().toArray()).replace(",", ""));
        return ctx.addProcess(ProcessTools.startProcess("java", processBuilder));
    }

    public static void writeToStdin(Process p, String s) throws IOException {
        BufferedWriter stdin = new BufferedWriter(new OutputStreamWriter(p.getOutputStream()));
        stdin.write(s);
        stdin.flush();
    }

    public static void exitNormallyByCommandQ(Process p) throws Exception {
        assertTrue(p.isAlive());
        writeToStdin(p, "d\n");
        writeToStdin(p, "q\n");
        p.waitFor(WAIT_EXIT_TIME, TimeUnit.SECONDS);
        assertTrue(!p.isAlive());
    }

    public static boolean deletePath(String path) {
        return deletePath(new File(path));
    }

    public static boolean deletePath(File path) {
        if (path.isDirectory()) {
            String[] children = path.list();
            if (children != null) {
                for (String child : children) {
                    if (!deletePath(new File(path, child))) {
                        return false;
                    }
                }
            }
        }
        return path.delete();
    }

    public static void testCase(ProcessHandler c) throws Exception {
        TestContext ctx = new TestContext();
        try {
            c.handle(ctx);
        } finally {
            for (Process p : ctx.getProcesses()) {
                if (p.isAlive()) {
                    p.destroyForcibly();
                }
            }
        }
    }

    public static void testCases(ProcessHandler... cases) throws Exception {
        for (ProcessHandler c : cases) {
            testCase(c);
        }
    }

    public static void testCases(Class<? extends JBoosterTestBase> testClazz) throws Exception {
        for (Method method : testClazz.getDeclaredMethods()) {
            if (!method.getName().startsWith("test")) {
                continue;
            }
            if (!method.getReturnType().equals(Void.TYPE)) {
                throw new RuntimeException("The return type of \"" + method.getName() + "\" should be void!");
            }
            if (method.getParameterCount() != 1 || method.getParameterTypes()[0] != TestContext.class) {
                throw new RuntimeException("The parameter type of \"" + method.getName() + "\" should be ArgWrapper!");
            }
            method.setAccessible(true);
            System.out.println("[ \"" + method.getName() + "\" testing ]");
            testCase(pw -> method.invoke(null, pw));
            System.out.println("[ \"" + method.getName() + "\" passed  ]");
        }
    }

    @FunctionalInterface
    public interface ProcessHandler {
        void handle(TestContext ctx) throws Exception;
    }

    public static class TestContext {
        private final ArrayList<Process> processes = new ArrayList<>();

        public ArrayList<Process> getProcesses() {
            return processes;
        }

        public Process addProcess(Process process) {
            processes.add(process);
            return process;
        }
    }
}
