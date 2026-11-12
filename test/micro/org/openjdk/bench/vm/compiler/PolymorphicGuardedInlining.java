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
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
package org.openjdk.bench.vm.compiler;

import java.util.Arrays;
import java.util.concurrent.TimeUnit;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.CompilerControl;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OperationsPerInvocation;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;

@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@Warmup(iterations = 5, time = 1)
@Measurement(iterations = 5, time = 1)
@State(Scope.Thread)
public class PolymorphicGuardedInlining {
    private static final int OPS = 4096;

    @Param({
            "THREE_UNIFORM",
            "THREE_90_5_5",
            "THREE_70_20_10",
            "FOUR_UNIFORM",
            "FOUR_40_30_20_10"
    })
    private String shape;

    private Op[] receivers;

    @Setup
    public void setup() {
        receivers = makeReceivers(shape);
    }

    @Benchmark
    @OperationsPerInvocation(OPS)
    @Fork(value = 1, jvmArgsAppend = {
            "-Xbatch",
            "-XX:+IgnoreUnrecognizedVMOptions",
            "-XX:-UsePolymorphicInlining",
            "-XX:TypeProfileWidth=4"
    })
    public int baseline() {
        return dispatch(receivers);
    }

    @Benchmark
    @OperationsPerInvocation(OPS)
    @Fork(value = 1, jvmArgsAppend = {
            "-Xbatch",
            "-XX:+IgnoreUnrecognizedVMOptions",
            "-XX:+UsePolymorphicInlining",
            "-XX:TypeProfileWidth=4"
    })
    public int polymorphicInlining() {
        return dispatch(receivers);
    }

    @CompilerControl(CompilerControl.Mode.DONT_INLINE)
    private static int dispatch(Op[] ops) {
        int result = 0;
        for (int i = 0; i < OPS; i++) {
            result += ops[i & (ops.length - 1)].apply(i);
        }
        return result;
    }

    private static Op[] makeReceivers(String shape) {
        Op[] ops = new Op[1024];
        if ("THREE_UNIFORM".equals(shape)) {
            fill(ops, new int[] { 341, 341, 342 }, new Op[] { new R0(), new R1(), new R2() });
        } else if ("THREE_90_5_5".equals(shape)) {
            fill(ops, new int[] { 922, 51, 51 }, new Op[] { new R0(), new R1(), new R2() });
        } else if ("THREE_70_20_10".equals(shape)) {
            fill(ops, new int[] { 717, 205, 102 }, new Op[] { new R0(), new R1(), new R2() });
        } else if ("FOUR_UNIFORM".equals(shape)) {
            fill(ops, new int[] { 256, 256, 256, 256 }, new Op[] { new R0(), new R1(), new R2(), new R3() });
        } else if ("FOUR_40_30_20_10".equals(shape)) {
            fill(ops, new int[] { 410, 307, 205, 102 }, new Op[] { new R0(), new R1(), new R2(), new R3() });
        } else {
            throw new IllegalArgumentException("Unknown shape: " + shape);
        }
        return ops;
    }

    private static void fill(Op[] ops, int[] counts, Op[] receivers) {
        int index = 0;
        for (int i = 0; i < counts.length; i++) {
            Arrays.fill(ops, index, index + counts[i], receivers[i]);
            index += counts[i];
        }
        if (index != ops.length) {
            throw new IllegalStateException("bad receiver distribution");
        }
    }

    private interface Op {
        int apply(int x);
    }

    private static final class R0 implements Op {
        @CompilerControl(CompilerControl.Mode.INLINE)
        public int apply(int x) {
            return x + 1;
        }
    }

    private static final class R1 implements Op {
        @CompilerControl(CompilerControl.Mode.INLINE)
        public int apply(int x) {
            return x + 3;
        }
    }

    private static final class R2 implements Op {
        @CompilerControl(CompilerControl.Mode.INLINE)
        public int apply(int x) {
            return x + 5;
        }
    }

    private static final class R3 implements Op {
        @CompilerControl(CompilerControl.Mode.INLINE)
        public int apply(int x) {
            return x + 7;
        }
    }
}
