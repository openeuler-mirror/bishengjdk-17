/*
 * Copyright (c) 2024, Huawei Technologies Co., Ltd. All rights reserved.
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

import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Threads;
import org.openjdk.jmh.annotations.Warmup;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import java.util.concurrent.TimeUnit;
import java.util.Random;

@BenchmarkMode(Mode.SampleTime)
@OutputTimeUnit(TimeUnit.MILLISECONDS)
@Warmup(iterations = 5, time = 5, timeUnit = TimeUnit.MILLISECONDS)
@Measurement(iterations = 5, time = 10, timeUnit = TimeUnit.MILLISECONDS)
@Fork(jvmArgsPrepend = {"-Xms1G", "-Xmx1G", "-XX:+AlwaysPreTouch"}, value = 1)
@Threads(1)
@State(Scope.Benchmark)
public class GzipKAEBenchmark {

    private byte[] srcBytes;
    private byte[] dstBytes;

    Random rnd = new Random(8192);
    ByteArrayOutputStream srcBAOS = new ByteArrayOutputStream();
    ByteArrayOutputStream dstBAOS = new ByteArrayOutputStream();

    @Setup
    public void setup() throws IOException {
        Random rnd = new Random(8192);
        ByteArrayOutputStream srcBAOS = new ByteArrayOutputStream();
        ByteArrayOutputStream dstBAOS = new ByteArrayOutputStream();
        for (int j = 0; j < 8192; j++) {
            byte[] src = new byte[rnd.nextInt(8192) + 1];
            rnd.nextBytes(src);
            srcBAOS.write(src);
        }
        srcBytes = srcBAOS.toByteArray();
        try (GZIPOutputStream gzos = new GZIPOutputStream(dstBAOS)) {
            gzos.write(srcBytes);
        }
        dstBytes = dstBAOS.toByteArray();
    }

    @Param({"512", "2048", "10240", "51200", "204800"})
    private int BuffSize;

    @Benchmark
    public void GzipDeflateTest() throws IOException{
        ByteArrayOutputStream byteArrayInputStream = new ByteArrayOutputStream();
        try (GZIPOutputStream gzipOutputStream = new GZIPOutputStream(byteArrayInputStream, BuffSize)) {
            gzipOutputStream.write(srcBytes);
        }
    }

    @Benchmark
    public void GzipInflateTest() {
        ByteArrayInputStream bais = new ByteArrayInputStream(dstBytes);
        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        try (GZIPInputStream gzipInputStream = new GZIPInputStream(bais, BuffSize)) {
            byte[] buffer = new byte[1024*1024];
            int len;
            while ((len = gzipInputStream.read(buffer)) > 0) {
                byteArrayOutputStream.write(buffer, 0, len);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}