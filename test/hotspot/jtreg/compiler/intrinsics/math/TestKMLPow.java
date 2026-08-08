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

/*
 * @test
 * @summary Test normal and positive-overflow Math.pow results with KML enabled
 * @library /test/lib
 * @requires os.family == "linux" & os.arch == "aarch64" & vm.compiler2.enabled
 * @run driver compiler.intrinsics.math.TestKMLPow
 */

package compiler.intrinsics.math;

import java.nio.file.Files;
import java.nio.file.Path;

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import jtreg.SkippedException;

public class TestKMLPow {
    private static final String KML_LIBRARY_PATH_PROPERTY = "test.kml.lib.path";
    private static final String RESULT_PREFIX = "KML_POW_RESULT ";

    public static void main(String[] args) throws Exception {
        String propertyValue = System.getProperty(KML_LIBRARY_PATH_PROPERTY);
        if (propertyValue == null || propertyValue.isBlank()) {
            throw new SkippedException("Specify the KML library directory with -D" +
                    KML_LIBRARY_PATH_PROPERTY + "=<absolute-directory>");
        }

        Path libraryDirectory = Path.of(propertyValue).normalize();
        if (!libraryDirectory.isAbsolute()) {
            throw new IllegalArgumentException(KML_LIBRARY_PATH_PROPERTY +
                    " must name an absolute directory: " + propertyValue);
        }
        if (!Files.isDirectory(libraryDirectory)) {
            throw new IllegalArgumentException("KML library directory does not exist: " +
                    libraryDirectory);
        }

        OutputAnalyzer enabled = runWorker(libraryDirectory, true);
        enabled.shouldHaveExitValue(0);
        enabled.shouldContain("Loaded KML library " + libraryDirectory +
                "/libkm.so,");

        OutputAnalyzer disabled = runWorker(libraryDirectory, false);
        disabled.shouldHaveExitValue(0);
        disabled.shouldNotContain("Loaded KML library");

        Results enabledResults = parseResults(enabled);
        Results disabledResults = parseResults(disabled);
        assertWithinUlps("enabled/disabled normal result",
                         enabledResults.normal(), disabledResults.normal(), 4.0);
        if (enabledResults.overflow() != Double.POSITIVE_INFINITY ||
            disabledResults.overflow() != Double.POSITIVE_INFINITY) {
            throw new AssertionError("Positive overflow differs with UseKMLPow: enabled=" +
                    enabledResults.overflow() + ", disabled=" + disabledResults.overflow());
        }
    }

    private static OutputAnalyzer runWorker(Path libraryDirectory,
                                            boolean useKMLPow) throws Exception {
        return ProcessTools.executeTestJava(
                "-Xbatch",
                "-XX:-TieredCompilation",
                "-XX:CompileCommand=quiet",
                "-XX:CompileCommand=compileonly," + Worker.class.getName() + "::pow",
                "-XX:" + (useKMLPow ? "+" : "-") + "UseKMLPow",
                "-XX:KMLLibraryPath=" + libraryDirectory,
                "-Xlog:library=info",
                Worker.class.getName());
    }

    private static Results parseResults(OutputAnalyzer output) {
        return output.asLines().stream()
                .filter(line -> line.startsWith(RESULT_PREFIX))
                .map(line -> line.substring(RESULT_PREFIX.length()).trim().split(" +"))
                .map(parts -> {
                    if (parts.length != 2) {
                        throw new AssertionError("Malformed worker result: " +
                                String.join(" ", parts));
                    }
                    return new Results(parseDoubleBits(parts[0]),
                                       parseDoubleBits(parts[1]));
                })
                .findFirst()
                .orElseThrow(() -> new AssertionError("Worker produced no result line\n" +
                        output.getOutput()));
    }

    private static double parseDoubleBits(String value) {
        return Double.longBitsToDouble(Long.parseUnsignedLong(value, 16));
    }

    private static void assertWithinUlps(String description, double first,
                                         double second, double ulps) {
        double tolerance = ulps * Math.max(Math.ulp(first), Math.ulp(second));
        if (!Double.isFinite(first) || !Double.isFinite(second) ||
            Math.abs(first - second) > tolerance) {
            throw new AssertionError(description + ": " + first + " vs " + second +
                    ", tolerance=" + tolerance);
        }
    }

    private record Results(double normal, double overflow) { }

    public static class Worker {
        // Exponent 3 takes the ordinary KML path (rather than the adapter's
        // special cases for zero, one, two, and one half) and has an exactly
        // representable result, keeping this test independent of approximation
        // tolerance while still exercising the library call.
        private static final double NORMAL_BASE = 10.0;
        private static final double NORMAL_EXPONENT = 3.0;
        private static final double OVERFLOW_BASE = 1.0e308;
        private static final double OVERFLOW_EXPONENT = 1.5;
        private static final int WARMUP_ITERATIONS = 20_000;

        private static double pow(double base, double exponent) {
            return Math.pow(base, exponent);
        }

        public static void main(String[] args) {
            double sink = 0.0;
            for (int i = 0; i < WARMUP_ITERATIONS; i++) {
                sink += pow(NORMAL_BASE, NORMAL_EXPONENT);
            }
            if (!Double.isFinite(sink)) {
                throw new AssertionError("Unexpected warmup result: " + sink);
            }

            double normal = pow(NORMAL_BASE, NORMAL_EXPONENT);
            double expectedNormal = StrictMath.pow(NORMAL_BASE, NORMAL_EXPONENT);
            assertWithinUlps("normal result", normal, expectedNormal, 4.0);

            double overflow = pow(OVERFLOW_BASE, OVERFLOW_EXPONENT);
            if (overflow != Double.POSITIVE_INFINITY) {
                throw new AssertionError("Expected positive overflow, got " + overflow);
            }

            System.out.println(RESULT_PREFIX +
                    Long.toUnsignedString(Double.doubleToRawLongBits(normal), 16) + " " +
                    Long.toUnsignedString(Double.doubleToRawLongBits(overflow), 16));
        }
    }
}
