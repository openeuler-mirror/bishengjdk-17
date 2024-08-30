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

package jdk.jbooster;

import java.lang.System.Logger;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import jdk.vm.ci.hotspot.HotSpotJVMCIRuntime;
import jdk.vm.ci.hotspot.HotSpotJVMCIRuntime.Option;
import sun.misc.Signal;
import sun.misc.SignalHandler;

import static java.lang.System.Logger.Level.ERROR;
import static java.lang.System.Logger.Level.INFO;
import static java.lang.System.Logger.Level.WARNING;

/**
 * The entry of the JBooster server.
 */
public final class JBooster {
    static {
        System.setProperty("java.util.logging.SimpleFormatter.format",
                "[%1$tF %1$tT][%4$-7s][%2$s] %5$s%n");
        System.loadLibrary("jbooster");
    }

    public static final Logger LOGGER = System.getLogger("jbooster");

    private static final Options options = new Options();

    private static final Commands commands = new Commands();

    private static ConnectionPool connectionPool;

    public static void main(String[] args) {
        try {
            options.parseArgs(args);
        } catch (IllegalArgumentException e) {
            if (LOGGER.isLoggable(ERROR)) {
                LOGGER.log(ERROR, "Failed to start because of the bad CMD args:");
                LOGGER.log(ERROR, "{0}", e.getMessage());
            } else {
                System.err.println("Failed to start because of the bad CMD args:");
                System.err.println(e.getMessage());
            }
            Options.printHelp();
            return;
        }

        init();
        LOGGER.log(INFO, "The JBooster server is ready!");

        try {
            if (!options.isInteractive() || !commands.interact()) {
                loop();
            }
        } finally {
            terminateAndJoin();
        }
    }

    static Options getOptions() {
        return options;
    }

    static Commands getCommands() {
        return commands;
    }

    static ConnectionPool getConnectionPool() {
        return connectionPool;
    }

    private static void init() {
        BackgroundScannerSignalHandler.register();
        connectionPool = new ConnectionPool();
        Main.initForJBooster();
        initInVM(options.getServerPort(), options.getConnectionTimeout(),
                options.getCleanupTimeout(), options.getCachePath());
    }

    private static void loop() {
        while (true) {
            try {
                Thread.sleep(10000);
            } catch (InterruptedException e) {
                return;
            }
        }
    }

    private static void terminateAndJoin() {
        final long maxTimeForWaiting = 8000L;
        final long maxTimeForInterrupting = 2000L;

        LOGGER.log(INFO, "Shutting down the connection pools (it may take some time)...",
                maxTimeForWaiting);
        ThreadPoolExecutor pool1 = getConnectionPool().getExecutor();
        ThreadPoolExecutor pool2 = Main.getJBoosterGlobalCompileQueue();
        List<ThreadPoolExecutor> pools = List.of(pool1, pool2);
        try {
            LOGGER.log(INFO, "Waiting (up to {0} ms) for the existing pool tasks to finish.", maxTimeForWaiting);
            for (ThreadPoolExecutor pool : pools) {
                pool.shutdown();
            }
            if (!waitThreadPoolsToTerminate(pools, maxTimeForWaiting)) {
                LOGGER.log(INFO, "Waiting (up to {0} ms) for interrupting pool tasks.", maxTimeForInterrupting);
                for (ThreadPoolExecutor pool : pools) {
                    pool.shutdownNow();
                }
                if (!waitThreadPoolsToTerminate(pools, maxTimeForInterrupting)) {
                    LOGGER.log(WARNING, "Failed to stop the pool properly. Force exit. Bye~");
                    System.exit(1);
                }
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        LOGGER.log(INFO, "Bye~");
    }

    /**
     * Wait for all thread pools to stop working.
     * The total wait time does not exceed waitMillis.
     */
    private static boolean waitThreadPoolsToTerminate(List<ThreadPoolExecutor> pools, long waitMillis)
            throws InterruptedException {
        boolean allTerminated = true;
        long timeBegin = System.currentTimeMillis();
        for (ThreadPoolExecutor pool : pools) {
            if (pool.isTerminated()) {
                continue;
            }
            long timeToWait = Math.max(0, waitMillis - (System.currentTimeMillis() - timeBegin));
            if (!pool.awaitTermination(timeToWait, TimeUnit.MILLISECONDS)) {
                allTerminated = false;
                LOGGER.log(INFO, "The pool {0} is still running after {1} ms.",
                        pool.getClass().getSimpleName(), waitMillis);
            }
        }
        return allTerminated;
    }

    /**
     * This method is invoked only in C++.
     */
    private static boolean receiveConnection(int connectionFd) {
        return connectionPool.execute(() -> handleConnection(connectionFd));
    }

    /**
     * This method is invoked only in C++.
     */
    private static boolean compileClasses(int sessionId, String filePath, Set<Class<?>> classes) {
        return compileMethods(sessionId, filePath, classes, null, null);
    }

    /**
     * This method is invoked only in C++.
     */
    private static boolean compileMethods(int sessionId, String filePath, Set<Class<?>> classes,
                                          Set<String> methodsToCompile, Set<String> methodsNotToCompile) {
        LOGGER.log(INFO, "Compilation task received: classes_to_compile={0}, methods_to_compile={1}, methods_not_compile={2}, session_id={3}.",
                classes.size(),
                (methodsToCompile == null ? "all" : String.valueOf(methodsToCompile.size())),
                (methodsNotToCompile == null ? "none" : String.valueOf(methodsNotToCompile.size())),
                sessionId);
        try {
            // [JBOOSTER TODO] jaotc logic
        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }

    private static native void initInVM(int serverPort, int connectionTimeout, int cleanupTimeout, String cachePath);

    private static native void handleConnection(int connectionFd);

    static native void printStoredClientData(boolean printAll);

    private static final class BackgroundScannerSignalHandler implements SignalHandler {
        public static void register() {
            // The returned value (i.e. the old, default handler) is ignored because
            // the default handler will make the program "stop" instead of "exit".
            // So do not use it.
            Signal.handle(new Signal("TTIN"), new BackgroundScannerSignalHandler());
        }

        private boolean handled;

        private BackgroundScannerSignalHandler() {
            handled = false;
        }

        @Override
        public void handle(Signal sig) {
            if (!handled) {
                handled = true;
                LOGGER.log(ERROR, "A SIGTTIN is detected! Looks like this program is running in background "
                        + "so Scanner is not supported here. Add \"-b\" or \"--background\" to the command "
                        + "line of jbooster to disable interactive commands.");
                System.exit(1);
            }
        }
    }
}
