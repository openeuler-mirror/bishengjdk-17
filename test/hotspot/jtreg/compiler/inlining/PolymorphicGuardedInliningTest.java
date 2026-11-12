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

package compiler.inlining;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Stream;

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

public class PolymorphicGuardedInliningTest {
    private static final int WARMUP = 60_000;
    private static final int CHECK = 20_000;
    private static final String RESULT_PREFIX = "RESULT ";
    private static final String[] BASE_VM_OPTIONS = {
        "-server",
        "-XX:-TieredCompilation",
        "-Xbatch",
        "-XX:-UseOnStackReplacement",
        "-XX:CompileThreshold=1000"
    };

    private static volatile long sink;

    public static void main(String[] args) throws Exception {
        if (args.length != 1) {
            throw new IllegalArgumentException("Missing payload mode");
        }
        runPayload(args[0]);
    }

    static void compareWithBaseline(String mode, String... options)
            throws Exception {
        long baseline = runAndReadResult(mode, "-XX:-UsePolymorphicInlining");
        long tested = runAndReadResult(mode, options);
        assertEquals(baseline, tested, mode + " " + Arrays.toString(options));
    }

    static long runAndReadResult(String mode, String... options)
            throws Exception {
        OutputAnalyzer output = runChild(mode, null, options);
        return parseResult(output.getOutput());
    }

    static RunResult runWithLog(String logName, String mode,
                                String... options) throws Exception {
        Path log = Paths.get(System.getProperty("test.classes"),
                             logName + "-%p.xml").toAbsolutePath();
        deleteLogs(log);
        OutputAnalyzer output = runChild(mode, log, options);
        String content = readLog(log);
        return new RunResult(output.getOutput(), content);
    }

    private static OutputAnalyzer runChild(String mode, Path log,
                                           String... options) throws Exception {
        List<String> command = new ArrayList<>();
        command.addAll(Arrays.asList(BASE_VM_OPTIONS));
        command.addAll(Arrays.asList(options));
        if (log != null) {
            command.add("-XX:LogFile=" + log);
        }
        command.add(PolymorphicGuardedInliningTest.class.getName());
        command.add(mode);
        OutputAnalyzer output =
                ProcessTools.executeTestJvm(command.toArray(new String[0]));
        output.shouldHaveExitValue(0);
        parseResult(output.getOutput());
        return output;
    }

    private static String readLog(Path pattern) throws IOException {
        Path dir = pattern.getParent();
        String fileName = pattern.getFileName().toString();
        String prefix = fileName.substring(0, fileName.indexOf("%p"));
        try (Stream<Path> paths = Files.list(dir)) {
            return paths
                    .filter(p -> p.getFileName().toString().startsWith(prefix))
                    .map(PolymorphicGuardedInliningTest::readAll)
                    .findFirst()
                    .orElseThrow(() ->
                            new IOException("Missing LogCompilation file " + pattern));
        }
    }

    private static void deleteLogs(Path pattern) throws IOException {
        Path dir = pattern.getParent();
        String fileName = pattern.getFileName().toString();
        String prefix = fileName.substring(0, fileName.indexOf("%p"));
        try (Stream<Path> paths = Files.list(dir)) {
            paths.filter(p -> p.getFileName().toString().startsWith(prefix))
                 .forEach(PolymorphicGuardedInliningTest::delete);
        }
    }

    private static void delete(Path path) {
        try {
            Files.delete(path);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static String readAll(Path path) {
        try {
            return new String(Files.readAllBytes(path), StandardCharsets.UTF_8);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static long parseResult(String output) {
        Matcher matcher = Pattern.compile("(?m)^" + RESULT_PREFIX + "(-?\\d+)$")
                                 .matcher(output);
        if (!matcher.find()) {
            throw new AssertionError("Missing result line in output:\n" + output);
        }
        return Long.parseLong(matcher.group(1));
    }

    static void assertEquals(long expected, long actual, String label) {
        if (expected != actual) {
            throw new AssertionError(label + ": expected " + expected +
                                     ", got " + actual);
        }
    }

    static void requireContains(String text, String expected,
                                String message) {
        if (!text.contains(expected)) {
            throw new AssertionError(message + "\nMissing: " + expected +
                                     "\nText:\n" + text);
        }
    }

    static void requireMatches(String text, String regex, String message) {
        if (!Pattern.compile(regex, Pattern.DOTALL).matcher(text).matches()) {
            throw new AssertionError(message + "\nRegex: " + regex +
                                     "\nText:\n" + text);
        }
    }

    private static void runPayload(String mode) {
        long result;
        if (mode.startsWith("interface")) {
            int receivers = Integer.parseInt(mode.substring("interface".length()));
            result = exerciseInterface(makeInterfaceReceivers(receivers), CHECK);
        } else if (mode.equals("trace4")) {
            result = exerciseInterface(makeInterfaceReceivers(4), WARMUP);
        } else if (mode.equals("miss4")) {
            for (int i = 0; i < 8; i++) {
                sink = exerciseInterface(makeInterfaceReceivers(3), WARMUP);
            }
            result = exerciseInterface(makeInterfaceReceivers(4), CHECK);
        } else if (mode.equals("mega6")) {
            result = exerciseInterface(makeInterfaceReceivers(6), WARMUP);
        } else if (mode.equals("skewed4")) {
            result = exerciseInterface(makeSkewedInterfaceReceivers(), WARMUP);
        } else if (mode.equals("virtualLarge")) {
            result = exerciseVirtual(makeLargeVirtualReceivers(), WARMUP);
        } else {
            throw new IllegalArgumentException("Unknown mode: " + mode);
        }
        sink = result;
        System.out.println(RESULT_PREFIX + result);
    }

    private static long exerciseInterface(Op[] ops, int iterations) {
        long result = 0;
        for (int round = 0; round < 6; round++) {
            result += runInterface(ops, iterations);
        }
        return result;
    }

    private static long exerciseVirtual(VOp[] ops, int iterations) {
        long result = 0;
        for (int round = 0; round < 6; round++) {
            result += runVirtual(ops, iterations);
        }
        return result;
    }

    private static int runInterface(Op[] ops, int iterations) {
        int result = 0;
        for (int i = 0; i < iterations; i++) {
            result += ops[i % ops.length].apply(i);
        }
        return result;
    }

    private static int runVirtual(VOp[] ops, int iterations) {
        int result = 0;
        for (int i = 0; i < iterations; i++) {
            result += ops[i % ops.length].apply(i);
        }
        return result;
    }

    private static Op[] makeInterfaceReceivers(int count) {
        Op[] all = { new R0(), new R1(), new R2(), new R3(), new R4(), new R5() };
        return Arrays.copyOf(all, count);
    }

    private static Op[] makeSkewedInterfaceReceivers() {
        Op[] ops = new Op[100];
        Arrays.fill(ops, 0, 40, new R0());
        Arrays.fill(ops, 40, 70, new R1());
        Arrays.fill(ops, 70, 90, new R2());
        Arrays.fill(ops, 90, 100, new R3());
        return ops;
    }

    private static VOp[] makeLargeVirtualReceivers() {
        VOp[] ops = new VOp[100];
        Arrays.fill(ops, 0, 90, new V0());
        Arrays.fill(ops, 90, 95, new V1());
        Arrays.fill(ops, 95, 100, new V2());
        return ops;
    }

    private interface Op {
        int apply(int x);
    }

    private static final class R0 implements Op {
        public int apply(int x) { return x + 1; }
    }

    private static final class R1 implements Op {
        public int apply(int x) { return x + 3; }
    }

    private static final class R2 implements Op {
        public int apply(int x) { return x + 5; }
    }

    private static final class R3 implements Op {
        public int apply(int x) { return x + 7; }
    }

    private static final class R4 implements Op {
        public int apply(int x) { return x + 11; }
    }

    private static final class R5 implements Op {
        public int apply(int x) { return x + 13; }
    }

    private abstract static class VOp {
        abstract int apply(int x);
    }

    private static final class V0 extends VOp {
        int apply(int x) { return large(x, 1); }
    }

    private static final class V1 extends VOp {
        int apply(int x) { return large(x, 3); }
    }

    private static final class V2 extends VOp {
        int apply(int x) { return large(x, 5); }
    }

    private static int large(int x, int salt) {
        int r = x + salt;
        r = (r * 33) ^ (r >>> 3);
        r = (r * 33) ^ (r >>> 4);
        r = (r * 33) ^ (r >>> 5);
        r = (r * 33) ^ (r >>> 6);
        r = (r * 33) ^ (r >>> 7);
        r = (r * 33) ^ (r >>> 8);
        r = (r * 33) ^ (r >>> 9);
        r = (r * 33) ^ (r >>> 10);
        r = (r * 33) ^ (r >>> 11);
        r = (r * 33) ^ (r >>> 12);
        return r;
    }

    static final class RunResult {
        final String output;
        final String log;

        RunResult(String output, String log) {
            this.output = output;
            this.log = log;
        }
    }
}
