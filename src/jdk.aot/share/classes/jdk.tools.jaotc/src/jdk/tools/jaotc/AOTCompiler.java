/*
 * Copyright (c) 2016, 2023, Oracle and/or its affiliates. All rights reserved.
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



package jdk.tools.jaotc;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.PriorityBlockingQueue;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import org.graalvm.compiler.options.OptionValues;

import jdk.vm.ci.jbooster.JBoosterCompilationContext;
import jdk.vm.ci.meta.ResolvedJavaMethod;

final class AOTCompiler {

    private final Main main;

    private final OptionValues graalOptions;

    private CompileQueue compileQueue;

    private final AOTBackend backend;

    /**
     * Compile queue.
     */
    private static class CompileQueue extends ThreadPoolExecutor {

        /**
         * Time of the start of this queue.
         */
        private final long startTime;

        /**
         * Method counter for successful compilations.
         */
        private final AtomicInteger successfulMethodCount = new AtomicInteger();

        /**
         * Method counter for failed compilations.
         */
        private final AtomicInteger failedMethodCount = new AtomicInteger();

        /**
         * The printer to use.
         */
        private final LogPrinter printer;

        /**
         * Create a compile queue with the given number of threads.
         */
        CompileQueue(LogPrinter printer, final int threads) {
            super(threads, threads, 0L, TimeUnit.MILLISECONDS, new PriorityBlockingQueue<>());
            this.printer = printer;
            startTime = System.currentTimeMillis();
        }

        @Override
        protected void beforeExecute(Thread t, Runnable r) {
            AOTCompilationTask task = (AOTCompilationTask) r;

            JBoosterCompilationContext ctx = task.getMain().getJBoosterCompilationContext();
            if (ctx != null) {
                // Every run() is executed in a new thread in the thread pool.
                // - Some code of JAOTC use global static variables, which makes it impossible to process
                //   multiple AOTCompilationTasks in parallel.
                // - Some code of VM.CI should be patched by JBooster, but it's difficult to pass the ctx
                //   through method calls.
                // So we use the thread-local JBoosterCompilationContext for each thread. And different
                // threads can share the same ctx.
                JBoosterCompilationContext.set(ctx);
            }
        }

        @Override
        protected void afterExecute(Runnable r, Throwable t) {
            AOTCompilationTask task = (AOTCompilationTask) r;

            AtomicInteger successfulMethodCount;
            AtomicInteger failedMethodCount;
            JBoosterCompilationContext ctx = task.getMain().getJBoosterCompilationContext();
            if (ctx != null) {
                successfulMethodCount = ctx.getCompileQueueSuccessfulMethodCount();
                failedMethodCount = ctx.getCompileQueueFailedMethodCount();

                ctx.getAOTCompilerRemainingTaskCount().countDown();
                JBoosterCompilationContext.set(null);
            } else {
                successfulMethodCount = this.successfulMethodCount;
                failedMethodCount = this.failedMethodCount;
            }

            if (task.getResult() != null) {
                final int count = successfulMethodCount.incrementAndGet();
                if (count % 100 == 0) {
                    printer.printInfo(".");
                }
                CompiledMethodInfo result = task.getResult();
                if (result != null) {
                    task.getHolder().addCompiledMethod(result);
                }
            } else {
                failedMethodCount.incrementAndGet();
                printer.printlnVerbose("");
                ResolvedJavaMethod method = task.getMethod();
                printer.printlnVerbose(" failed " + method.getName() + method.getSignature().toMethodDescriptor());
            }
        }

        @Override
        protected void terminated() {
            if (jboosterGlobalCompileQueue != null) {
                printer.printlnInfo("JBooster global AOT compilation queue terminated.");
                return;
            }
            final long endTime = System.currentTimeMillis();
            final int success = successfulMethodCount.get();
            final int failed = failedMethodCount.get();
            printer.printlnInfo("");
            printer.printlnInfo(success + " methods compiled, " + failed + " methods failed (" + (endTime - startTime) + " ms)");
        }

        public void guaranteeStaticNotUsed() {
            if (successfulMethodCount.get() != 0) {
                throw new IllegalStateException("Static successfulMethodCount should be 0 for JBooster!");
            }
            if (failedMethodCount.get() != 0) {
                throw new IllegalStateException("Static failedMethodCount should be 0 for JBooster!");
            }
        }
    }

    /**
     * @param main
     * @param graalOptions
     * @param aotBackend
     * @param threads number of compilation threads
     */
    AOTCompiler(Main main, OptionValues graalOptions, AOTBackend aotBackend, final int threads) {
        this.main = main;
        this.graalOptions = graalOptions;
        this.compileQueue = new CompileQueue(main.printer, threads);
        this.backend = aotBackend;
    }

    /**
     * Compile all methods in all classes passed.
     *
     * @param classes a list of class to compile
     * @throws InterruptedException
     */
    List<AOTCompiledClass> compileClasses(List<AOTCompiledClass> classes) throws InterruptedException {
        main.printer.printlnInfo("Compiling with " + compileQueue.getCorePoolSize() + " threads");
        main.printer.printInfo("."); // Compilation progress indication.

        for (AOTCompiledClass c : classes) {
            for (ResolvedJavaMethod m : c.getMethods()) {
                enqueueMethod(c, m);
            }
        }

        // Shutdown queue and wait for all tasks to complete.
        compileQueue.shutdown();
        compileQueue.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);

        List<AOTCompiledClass> compiledClasses = new ArrayList<>();
        for (AOTCompiledClass compiledClass : classes) {
            if (compiledClass.hasCompiledMethods()) {
                compiledClasses.add(compiledClass);
            }
        }
        return compiledClasses;
    }

    /**
     * Enqueue a method in the {@link #compileQueue}.
     *
     * @param method method to be enqueued
     */
    private void enqueueMethod(AOTCompiledClass aotClass, ResolvedJavaMethod method) {
        AOTCompilationTask task = new AOTCompilationTask(main, graalOptions, aotClass, method, backend);
        try {
            compileQueue.execute(task);
        } catch (RejectedExecutionException e) {
            e.printStackTrace();
        }
    }

    static void logCompilation(String methodName, String message) {
        LogPrinter.writeLog(message + " " + methodName);
    }

    private static CompileQueue jboosterGlobalCompileQueue = null;

    static ThreadPoolExecutor getJBoosterGlobalCompileQueue() {
        return jboosterGlobalCompileQueue;
    }

    /**
     * Compile classes by the jboosterGlobalCompileQueue. So different classes
     * set can use the same thread pool.
     */
    static List<AOTCompiledClass> compileClassesInJBooster(
            Main main, OptionValues options, AOTBackend backend, List<AOTCompiledClass> classes)
            throws InterruptedException {
        main.printer.printlnInfo("Compiling with " + jboosterGlobalCompileQueue.getCorePoolSize() + " threads");
        main.printer.printInfo("."); // Compilation progress indication.

        final long startTime = System.currentTimeMillis();

        JBoosterCompilationContext ctx = main.getJBoosterCompilationContext();
        List<AOTCompilationTask> tasks = getMethodCompilationTasks(main, options, backend, classes);
        ctx.setAOTCompilerTotalTaskCount(tasks.size());
        enqueueMethods(main, tasks);

        ctx.getAOTCompilerRemainingTaskCount().await();

        final long endTime = System.currentTimeMillis();
        final int success = ctx.getCompileQueueSuccessfulMethodCount().get();
        final int failed = ctx.getCompileQueueFailedMethodCount().get();
        main.printer.printlnInfo("");
        main.printer.printlnInfo(success + " methods compiled, " + failed + " methods failed (" + (endTime - startTime) + " ms)");
        jboosterGlobalCompileQueue.guaranteeStaticNotUsed();

        List<AOTCompiledClass> compiledClasses = new ArrayList<>();
        for (AOTCompiledClass compiledClass : classes) {
            if (compiledClass.hasCompiledMethods()) {
                compiledClasses.add(compiledClass);
            }
        }
        return compiledClasses;
    }

    static void initJBoosterGlobalCompileQueue() {
        if (jboosterGlobalCompileQueue != null) {
            throw new InternalError("Init twice?");
        }

        int globalThreads = Integer.max(Runtime.getRuntime().availableProcessors() - 4, 1);
        Main commonMain = new Main();
        commonMain.options.info = true;
        commonMain.options.verbose = false;
        commonMain.options.debug = false;
        jboosterGlobalCompileQueue = new CompileQueue(commonMain.printer, globalThreads);
    }

    private static List<AOTCompilationTask> getMethodCompilationTasks(Main main, OptionValues options,
            AOTBackend backend, List<AOTCompiledClass> classes) {
        List<AOTCompilationTask> list = new ArrayList<>();
        for (AOTCompiledClass aotClass : classes) {
            for (ResolvedJavaMethod method : aotClass.getMethods()) {
                AOTCompilationTask task = new AOTCompilationTask(main, options, aotClass, method, backend);
                list.add(task);
            }
        }
        return list;
    }

    private static void enqueueMethods(Main main, List<AOTCompilationTask> tasks) throws InterruptedException {
        for (AOTCompilationTask task : tasks) {
            try {
                jboosterGlobalCompileQueue.execute(task);
            } catch (RejectedExecutionException e) {
                main.getJBoosterCompilationContext().getAOTCompilerRemainingTaskCount().countDown();
                if (jboosterGlobalCompileQueue.isShutdown()) {
                    throw new InterruptedException();
                } else {
                    e.printStackTrace();
                }
            }
        }
    }
}
