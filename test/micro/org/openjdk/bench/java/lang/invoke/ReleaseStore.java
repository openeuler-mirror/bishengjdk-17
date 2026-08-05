/*
 * Copyright (c) 2026, Huawei Technologies Co., Ltd. All rights reserved.
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
 */
package org.openjdk.bench.java.lang.invoke;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.util.concurrent.TimeUnit;

import jdk.internal.misc.Unsafe;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OperationsPerInvocation;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;
import org.openjdk.jmh.infra.Blackhole;

@BenchmarkMode(Mode.Throughput)
@OutputTimeUnit(TimeUnit.SECONDS)
@Warmup(iterations = 5, time = 1)
@Measurement(iterations = 10, time = 1)
@Fork(3)
@State(Scope.Thread)
public class ReleaseStore {
    private static final int STORE_COUNT = 2_000;
    private static final int ARRAY_MASK = 4_096 - 1;
    private static final Unsafe UNSAFE = Unsafe.getUnsafe();
    private static final VarHandle INT_ARRAY_HANDLE = MethodHandles.arrayElementVarHandle(int[].class);
    private static final VarHandle LONG_ARRAY_HANDLE = MethodHandles.arrayElementVarHandle(long[].class);
    private static final VarHandle REFERENCE_ARRAY_HANDLE = MethodHandles.arrayElementVarHandle(Object[].class);
    private static final long INT_ARRAY_BASE = Unsafe.ARRAY_INT_BASE_OFFSET;
    private static final long LONG_ARRAY_BASE = Unsafe.ARRAY_LONG_BASE_OFFSET;
    private static final long REFERENCE_ARRAY_BASE = Unsafe.ARRAY_OBJECT_BASE_OFFSET;
    private static final int INT_ARRAY_SHIFT = Integer.numberOfTrailingZeros(Unsafe.ARRAY_INT_INDEX_SCALE);
    private static final int LONG_ARRAY_SHIFT = Integer.numberOfTrailingZeros(Unsafe.ARRAY_LONG_INDEX_SCALE);
    private static final int REFERENCE_ARRAY_SHIFT = Integer.numberOfTrailingZeros(Unsafe.ARRAY_OBJECT_INDEX_SCALE);

    private final int[] intValues = new int[ARRAY_MASK + 1];
    private final long[] longValues = new long[ARRAY_MASK + 1];
    private final Object[] referenceValues = new Object[ARRAY_MASK + 1];
    private int randomState;

    @Benchmark
    @OperationsPerInvocation(STORE_COUNT)
    public void unsafePutIntRelease(Blackhole blackhole) {
        int random = randomState;
        for (int i = 0; i < STORE_COUNT; i++) {
            random = random * 1_664_525 + 1_013_904_223;
            UNSAFE.putIntRelease(intValues, INT_ARRAY_BASE + ((long) (random & ARRAY_MASK) << INT_ARRAY_SHIFT), random);
        }
        randomState = random;
        blackhole.consume(random);
    }

    @Benchmark
    @OperationsPerInvocation(STORE_COUNT)
    public void unsafePutLongRelease(Blackhole blackhole) {
        int random = randomState;
        for (int i = 0; i < STORE_COUNT; i++) {
            random = random * 1_664_525 + 1_013_904_223;
            UNSAFE.putLongRelease(longValues,
                    LONG_ARRAY_BASE + ((long) (random & ARRAY_MASK) << LONG_ARRAY_SHIFT),
                    random);
        }
        randomState = random;
        blackhole.consume(random);
    }

    @Benchmark
    @OperationsPerInvocation(STORE_COUNT)
    public void unsafePutReferenceRelease(Blackhole blackhole) {
        int random = randomState;
        for (int i = 0; i < STORE_COUNT; i++) {
            random = random * 1_664_525 + 1_013_904_223;
            UNSAFE.putReferenceRelease(referenceValues,
                    REFERENCE_ARRAY_BASE + ((long) (random & ARRAY_MASK) << REFERENCE_ARRAY_SHIFT),
                    this);
        }
        randomState = random;
        blackhole.consume(random);
    }

    @Benchmark
    @OperationsPerInvocation(STORE_COUNT)
    public void varHandleSetReleaseInt(Blackhole blackhole) {
        int random = randomState;
        for (int i = 0; i < STORE_COUNT; i++) {
            random = random * 1_664_525 + 1_013_904_223;
            INT_ARRAY_HANDLE.setRelease(intValues, random & ARRAY_MASK, random);
        }
        randomState = random;
        blackhole.consume(random);
    }

    @Benchmark
    @OperationsPerInvocation(STORE_COUNT)
    public void varHandleSetReleaseLong(Blackhole blackhole) {
        int random = randomState;
        for (int i = 0; i < STORE_COUNT; i++) {
            random = random * 1_664_525 + 1_013_904_223;
            LONG_ARRAY_HANDLE.setRelease(longValues, random & ARRAY_MASK, (long) random);
        }
        randomState = random;
        blackhole.consume(random);
    }

    @Benchmark
    @OperationsPerInvocation(STORE_COUNT)
    public void varHandleSetReleaseReference(Blackhole blackhole) {
        int random = randomState;
        for (int i = 0; i < STORE_COUNT; i++) {
            random = random * 1_664_525 + 1_013_904_223;
            REFERENCE_ARRAY_HANDLE.setRelease(referenceValues, random & ARRAY_MASK, this);
        }
        randomState = random;
        blackhole.consume(random);
    }
}
