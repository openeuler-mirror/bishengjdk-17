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

package org.openjdk.bench.java.lang;

import java.util.Random;
import java.util.concurrent.TimeUnit;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OperationsPerInvocation;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Warmup;

/**
 * Measures Math.pow with ordinary finite inputs. KML should be enabled or
 * disabled with JMH fork VM arguments so that both runs use the same JAR.
 */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@State(Scope.Thread)
@Warmup(iterations = 5, time = 1, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 5, time = 1, timeUnit = TimeUnit.SECONDS)
@Fork(3)
public class KMLPow {

    private static final int INPUT_SIZE = 1024;

    private double[] bases;
    private double[] exponents;

    @Setup
    public void setup() {
        Random random = new Random(0x4b4d4cL);
        bases = new double[INPUT_SIZE];
        exponents = new double[INPUT_SIZE];

        for (int i = 0; i < INPUT_SIZE; i++) {
            bases[i] = 0.125 + random.nextDouble() * 7.875;
            double magnitude = 0.125 + random.nextDouble() * 7.875;
            exponents[i] = (i & 1) == 0 ? magnitude : -magnitude;
        }
    }

    @Benchmark
    @OperationsPerInvocation(INPUT_SIZE)
    public double mathPow() {
        double result = 0.0;
        double[] localBases = bases;
        double[] localExponents = exponents;

        for (int i = 0; i < INPUT_SIZE; i++) {
            result += Math.pow(localBases[i], localExponents[i]);
        }
        return result;
    }
}
