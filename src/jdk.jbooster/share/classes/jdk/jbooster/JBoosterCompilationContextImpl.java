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

package jdk.jbooster;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;

import jdk.vm.ci.jbooster.JBoosterCompilationContext;

/**
 * The only implement of JBoosterCompilationContext.
 */
public class JBoosterCompilationContextImpl implements JBoosterCompilationContext {
    // These values are related to JBooster compilation.
    private final int sessionId;
    private final String filePath;
    private final Set<Class<?>> classesToCompile;
    private final Set<String> methodsToCompile;
    private final Set<String> methodsNotToCompile;
    private final boolean usePGO;
    private final boolean resolveExtraKlasses;

    // These values are used to replace the static values in AOT classes.
    private final AtomicInteger compiledMethodInfoMethodsCount = new AtomicInteger(0);
    private Object aotCompiledClassAOTDynamicTypeStore = null;
    private final AtomicInteger aotCompiledClassClassesCount = new AtomicInteger(0);
    private final HashMap<String, ?> aotCompiledClassKlassData = new HashMap<>();
    private final StringBuilder elfSectionSectNameTab = new StringBuilder();
    private final AtomicInteger elfSectionShStrTabNrOfBytes = new AtomicInteger(0);
    private final AtomicInteger compileQueueSuccessfulMethodCount = new AtomicInteger(0);
    private final AtomicInteger compileQueueFailedMethodCount = new AtomicInteger(0);
    private final Set<Long> installedCodeBlobs = Collections.synchronizedSet(new HashSet<>());;

    private CountDownLatch aotCompilerRemainingTaskCount = null;

    public JBoosterCompilationContextImpl(
            int sessionId,
            String filePath,
            Set<Class<?>> classesToCompile,
            Set<String> methodsToCompile,
            Set<String> methodsNotCompile,
            boolean usePGO,
            boolean resolveExtraKlasses) {
        this.sessionId = sessionId;
        this.filePath = filePath;
        this.classesToCompile = classesToCompile;
        this.methodsToCompile = methodsToCompile;
        this.methodsNotToCompile = methodsNotCompile;
        this.usePGO = usePGO;
        this.resolveExtraKlasses = resolveExtraKlasses;
    }

    @Override
    public int getSessionId() {
        return sessionId;
    }

    @Override
    public String getFilePath() {
        return filePath;
    }

    @Override
    public Set<Class<?>> getClassesToCompile() {
        return classesToCompile;
    }

    @Override
    public Set<String> getMethodsToCompile() {
        return methodsToCompile;
    }

    @Override
    public Set<String> getMethodsNotToCompile() {
        return methodsNotToCompile;
    }

    @Override
    public boolean usePGO() {
        return usePGO;
    }

    @Override
    public boolean resolveExtraKlasses() {
        return resolveExtraKlasses;
    }

    @Override
    public AtomicInteger getCompiledMethodInfoMethodsCount() {
        return compiledMethodInfoMethodsCount;
    }

    @Override
    public Object getAOTCompiledClassAOTDynamicTypeStore() {
        return aotCompiledClassAOTDynamicTypeStore;
    }

    @Override
    public void setAotCompiledClassAOTDynamicTypeStore(Object dynoStore) {
        this.aotCompiledClassAOTDynamicTypeStore = dynoStore;
    }

    @Override
    public AtomicInteger getAOTCompiledClassClassesCount() {
        return aotCompiledClassClassesCount;
    }

    @Override
    public HashMap<String, ?> getAOTCompiledClassKlassData() {
        return aotCompiledClassKlassData;
    }

    @Override
    public StringBuilder getElfSectionSectNameTab() {
        return elfSectionSectNameTab;
    }

    @Override
    public AtomicInteger getElfSectionShStrTabNrOfBytes() {
        return elfSectionShStrTabNrOfBytes;
    }

    @Override
    public CountDownLatch getAOTCompilerRemainingTaskCount() {
        return aotCompilerRemainingTaskCount;
    }

    @Override
    public void setAOTCompilerTotalTaskCount(int taskCnt) {
        aotCompilerRemainingTaskCount = new CountDownLatch(taskCnt);
    }

    @Override
    public AtomicInteger getCompileQueueSuccessfulMethodCount() {
        return compileQueueSuccessfulMethodCount;
    }

    @Override
    public AtomicInteger getCompileQueueFailedMethodCount() {
        return compileQueueFailedMethodCount;
    }

    @Override
    public boolean isInlineExcluded(String methodName) {
        return methodsNotToCompile.contains(methodName);
    }

    @Override
    public long getMetaspaceMethodData(long metaspaceMethod) {
        return getMetaspaceMethodData(sessionId, metaspaceMethod);
    }

    @Override
    public void recordJBoosterInstalledCodeBlobs(long address) {
        installedCodeBlobs.add(address);
    }

    @Override
    public void clear() {
        long[] blobs = new long[installedCodeBlobs.size()];
        int index = 0;
        for (long blob : installedCodeBlobs) {
            blobs[index++] = blob;
        }
        freeUnusedCodeBlobs(blobs);
    }

    private static native long getMetaspaceMethodData(int sessionId, long metaspaceMethod);

    private static native long freeUnusedCodeBlobs(long[] blobs);
}
