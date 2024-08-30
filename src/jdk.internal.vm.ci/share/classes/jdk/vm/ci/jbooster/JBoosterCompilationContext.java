/*
 * Copyright (c) 2023, Huawei Technologies Co., Ltd. All rights reserved.
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

package jdk.vm.ci.jbooster;

import java.util.HashMap;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Compilation context interface for JBooster.
 * Defined in jdk.vm.ci for all modules to access.
 */
public interface JBoosterCompilationContext {
    /**
     * The context is thread-local.
     * Please access it through get() & set().
     */
    ThreadLocal<JBoosterCompilationContext> CTX = new ThreadLocal<>();

    /**
     * Get thread-local context.
     *
     * @return JBoosterCompilationContext
     */
    static JBoosterCompilationContext get() {
        return CTX.get();
    }

    /**
     * Set thread-local context.
     *
     * @param ctx JBoosterCompilationContext
     */
    static void set(JBoosterCompilationContext ctx) {
        CTX.set(ctx);
    }

    /**
     * Get the ID of the client session.
     */
    int getSessionId();

    /**
     * Get the file path to store the compiled AOT lib.
     */
    String getFilePath();

    /**
     * Get the classes to compile.
     */
    Set<Class<?>> getClassesToCompile();

    /**
     * Get the methods to compile.
     * The methods should be in getClassesToCompile().
     *
     * @return names & signatures of the methods
     */
    Set<String> getMethodsToCompile();

    /**
     * Get the methods that should never be compiled.
     * These methods have special invoke handles in them so they are not
     * compatible with PGO.
     *
     * @return names & signatures of the methods
     */
    Set<String> getMethodsNotToCompile();

    /**
     * return true if this context use PGO
     */
    boolean usePGO();

    /**
     * Get methodCount of CompiledMethodInfo.
     * (To support multi-task concurrent compilation of AOT)
     */
    AtomicInteger getCompiledMethodInfoMethodsCount();

    /**
     * Get dynoStore of AOTCompiledClass.
     * (To support multi-task concurrent compilation of AOT)
     *
     * @return a AOTDynamicTypeStore
     *     (AOTDynamicTypeStore is not visible here)
     */
    Object getAOTCompiledClassAOTDynamicTypeStore();

    /**
     * Set dynoStore of AOTCompiledClass.
     * (To support multi-task concurrent compilation of AOT)
     */
    void setAotCompiledClassAOTDynamicTypeStore(Object dynoStore);

    /**
     * Get classesCount of AOTCompiledClass.
     * (To support multi-task concurrent compilation of AOT)
     */
    AtomicInteger getAOTCompiledClassClassesCount();

    /**
     * Get klassData of AOTCompiledClass.
     * (To support multi-task concurrent compilation of AOT)
     *
     * @return a HashMap<String, AOTKlassData>
     *     (AOTKlassData is not visible here)
     */
    HashMap<String, ?> getAOTCompiledClassKlassData();

    /**
     * Get sectNameTab of ElfSection.
     * (To support multi-task concurrent compilation of AOT)
     */
    StringBuilder getElfSectionSectNameTab();

    /**
     * Get shStrTabNrOfBytes of ElfSection.
     * (To support multi-task concurrent compilation of AOT)
     */
    AtomicInteger getElfSectionShStrTabNrOfBytes();

    /**
     * Get the number of remaining tasks that have not been compiled.
     * (To support concurrent compilation for different tasks)
     */
    CountDownLatch getAOTCompilerRemainingTaskCount();

    /**
     * Set the number of tasks that are successfully enqueued.
     * (To support concurrent compilation for different tasks)
     */
    void setAOTCompilerTotalTaskCount(int taskCnt);

    /**
     * Get successfulMethodCount of AOTCompiler#CompileQueue.
     * (To support concurrent compilation for different tasks)
     */
    AtomicInteger getCompileQueueSuccessfulMethodCount();

    /**
     * Get failedMethodCount of AOTCompiler#CompileQueue.
     * (To support concurrent compilation for different tasks)
     */
    AtomicInteger getCompileQueueFailedMethodCount();

    /**
     * Record JBooster Installed CodeBlobs, only save address.
     * (To support multiple compilations in single process with graal compiler)
     */
    void recordJBoosterInstalledCodeBlobs(long address);

    /**
     * Do some cleanup after the current compilation is complete.
     * (To support multiple compilations in single process with graal compiler)
     */
    void clear();

    /**
     * Should the method be excluded for inline.
     * (To support PGO)
     *
     * @param methodName method name
     * @return is excluded for inline
     */
    boolean isInlineExcluded(String methodName);

    /**
     * Get the real metaspace method data of client session.
     * (To support PGO)
     *
     * @param metaspaceMethod the metaspace method
     * @return the address of method data
     */
    long getMetaspaceMethodData(long metaspaceMethod);
}
