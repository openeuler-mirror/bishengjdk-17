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
 * @summary AutoSharedArchivePath isolates and reuses both archive variants by user and launch target.
 * @requires (os.family == "linux") & (os.arch == "aarch64")
 * @run main/timeout=360 AutoSharedArchivePathIdentity
 */

import java.nio.charset.StandardCharsets;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.FileTime;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

public class AutoSharedArchivePathIdentity {
    private static final String APP_ONE = "com.huawei.autocds.IdentityAppOne";
    private static final String APP_TWO = "com.huawei.autocds.IdentityAppTwo";

    private static Path work;
    private static String effectiveUserIdentity;
    private static int commandSequence;

    public static void main(String[] args) throws Exception {
        System.out.println("===  main ===");
        work = Files.createTempDirectory("auto-appcds-identity").toAbsolutePath();
        effectiveUserIdentity = normalize(Files.getOwner(work).getName());
        Path classes = work.resolve("classes");
        Files.createDirectories(classes);
        compileApplication(classes, APP_ONE);
        compileApplication(classes, APP_TWO);
        String longMainClassOne = longMainClass("VeryLongIdentityApplicationOne");
        String longMainClassTwo = longMainClass("VeryLongIdentityApplicationTwo");
        compileApplication(classes, longMainClassOne);
        compileApplication(classes, longMainClassTwo);

        verifyLegacyNames(classes);
        verifyVersionLaunch();

        Path identityDirectory = work.resolve("identity-cds");
        Files.createDirectories(identityDirectory);
        ArchivePair first = verifyIdentityAndReuse(classes, identityDirectory, APP_ONE);
        ArchivePair second = verifyIdentityAndReuse(classes, identityDirectory, APP_TWO);
        if (first.coop.equals(second.coop) || first.nocoop.equals(second.nocoop)) {
            throw new RuntimeException("Different main classes resolved to the same archives: "
                    + first.coop + ", " + second.coop + ", "
                    + first.nocoop + ", " + second.nocoop);
        }
        assertNoInternalDumpIdentity(identityDirectory);
        Path firstLongIdentity = verifyLongIdentity(classes, longMainClassOne);
        Path secondLongIdentity = verifyLongIdentity(classes, longMainClassTwo);
        if (firstLongIdentity.equals(secondLongIdentity)) {
            throw new RuntimeException("Long main classes with different tails collided: "
                    + firstLongIdentity);
        }
        verifyLongBaseIdentity(classes, longMainClassOne);
    }

    private static void verifyLegacyNames(Path classes) throws Exception {
        Path cds = work.resolve("legacy-cds");
        Files.createDirectories(cds);
        Path classList = cds.resolve("appcds.lst");
        ArchivePair archives = new ArchivePair(
                cds.resolve("appcds_coop.jsa"),
                cds.resolve("appcds_nocoop.jsa"));

        String first = runJava(classes, cds, APP_ONE, false, Boolean.TRUE);
        assertSelectedPaths(first, classList, archives.coop);
        waitForFile(classList, 30);

        String second = runJava(classes, cds, APP_ONE, false, Boolean.TRUE);
        assertSelectedPaths(second, classList, archives.coop);
        waitForFile(archives.coop, 60);
        waitForFile(archives.nocoop, 60);
        FileTime coopTime = Files.getLastModifiedTime(archives.coop);
        FileTime nocoopTime = Files.getLastModifiedTime(archives.nocoop);

        Thread.sleep(1200);
        String coopReuse = runJava(classes, cds, APP_ONE, false, Boolean.TRUE);
        assertSelectedPaths(coopReuse, classList, archives.coop);
        assertContains(coopReuse, " use AppCDS jsa.");
        assertUnchanged(archives.coop, coopTime);
        assertUnchanged(archives.nocoop, nocoopTime);

        String nocoopReuse = runJava(classes, cds, APP_ONE, false, Boolean.FALSE);
        assertSelectedPaths(nocoopReuse, classList, archives.nocoop);
        assertContains(nocoopReuse, " use AppCDS jsa.");
        assertUnchanged(archives.coop, coopTime);
        assertUnchanged(archives.nocoop, nocoopTime);
    }

    private static void verifyVersionLaunch() throws Exception {
        Path cds = work.resolve("version-cds");
        Files.createDirectories(cds);
        String stem = identityStem("java");
        Path classList = cds.resolve(stem + ".lst");
        ArchivePair archives = new ArchivePair(
                cds.resolve(stem + "_coop.jsa"),
                cds.resolve(stem + "_nocoop.jsa"));

        String first = runVersion(cds, true);
        assertSelectedPaths(first, classList, archives.coop);
        waitForFile(classList, 30);

        String second = runVersion(cds, true);
        assertSelectedPaths(second, classList, archives.coop);
        waitForFile(archives.coop, 60);
        waitForFile(archives.nocoop, 60);
        FileTime coopTime = Files.getLastModifiedTime(archives.coop);
        FileTime nocoopTime = Files.getLastModifiedTime(archives.nocoop);

        Thread.sleep(1200);
        String coopReuse = runVersion(cds, true);
        assertSelectedPaths(coopReuse, classList, archives.coop);
        assertContains(coopReuse, " use AppCDS jsa.");
        assertUnchanged(archives.coop, coopTime);
        assertUnchanged(archives.nocoop, nocoopTime);

        String nocoopReuse = runVersion(cds, false);
        assertSelectedPaths(nocoopReuse, classList, archives.nocoop);
        assertContains(nocoopReuse, " use AppCDS jsa.");
        assertUnchanged(archives.coop, coopTime);
        assertUnchanged(archives.nocoop, nocoopTime);
    }

    private static ArchivePair verifyIdentityAndReuse(Path classes, Path cds,
                                                       String mainClass) throws Exception {
        String stem = identityStem(mainClass);
        Path classList = cds.resolve(stem + ".lst");
        ArchivePair archives = new ArchivePair(
                cds.resolve(stem + "_coop.jsa"),
                cds.resolve(stem + "_nocoop.jsa"));

        String first = runJava(classes, cds, mainClass, true, Boolean.TRUE);
        assertSelectedPaths(first, classList, archives.coop);
        waitForFile(classList, 30);

        String second = runJava(classes, cds, mainClass, true, Boolean.TRUE);
        assertSelectedPaths(second, classList, archives.coop);
        waitForFile(archives.coop, 60);
        waitForFile(archives.nocoop, 60);
        FileTime coopTime = Files.getLastModifiedTime(archives.coop);
        FileTime nocoopTime = Files.getLastModifiedTime(archives.nocoop);

        Thread.sleep(1200);
        String coopReuse = runJava(classes, cds, mainClass, true, Boolean.TRUE);
        assertSelectedPaths(coopReuse, classList, archives.coop);
        assertContains(coopReuse, " use AppCDS jsa.");
        assertUnchanged(archives.coop, coopTime);
        assertUnchanged(archives.nocoop, nocoopTime);

        String nocoopReuse = runJava(classes, cds, mainClass, true, Boolean.FALSE);
        assertSelectedPaths(nocoopReuse, classList, archives.nocoop);
        assertContains(nocoopReuse, " use AppCDS jsa.");
        assertUnchanged(archives.coop, coopTime);
        assertUnchanged(archives.nocoop, nocoopTime);
        return archives;
    }

    private static void assertNoInternalDumpIdentity(Path cds) throws Exception {
        String stem = identityStem("java");
        Path classList = cds.resolve(stem + ".lst");
        Path coop = cds.resolve(stem + "_coop.jsa");
        Path nocoop = cds.resolve(stem + "_nocoop.jsa");
        Thread.sleep(1000);
        if (Files.exists(classList) || Files.exists(coop) || Files.exists(nocoop)) {
            throw new RuntimeException("Internal dump JVM created fallback identity files: "
                    + classList + ", " + coop + ", " + nocoop);
        }
    }

    private static String longMainClass(String simpleName) {
        StringBuilder name = new StringBuilder();
        for (int i = 0; i < 8; i++) {
            if (name.length() > 0) {
                name.append('.');
            }
            name.append("segment").append(i)
                    .append("abcdefghijklmnopqrstuvwxyz");
        }
        return name.append('.').append(simpleName).toString();
    }

    private static Path verifyLongIdentity(Path classes, String mainClass)
            throws Exception {
        Path cds = work.resolve("long-identity-cds");
        Files.createDirectories(cds);

        String firstOutput = runJava(classes, cds, mainClass, true, Boolean.TRUE);
        Path firstClassList = extractPath(firstOutput, "classlist file : ");
        Path firstCoop = extractPath(firstOutput, "appcds jsa file : ");
        Path firstNocoop = nocoopPath(firstCoop);
        assertBoundedHashName(firstClassList, ".lst");
        assertBoundedHashName(firstCoop, "_coop.jsa");
        assertBoundedHashName(firstNocoop, "_nocoop.jsa");
        assertCommonStem(firstClassList, firstCoop, firstNocoop);
        waitForFile(firstClassList, 30);

        String secondOutput = runJava(classes, cds, mainClass, true, Boolean.TRUE);
        Path secondClassList = extractPath(secondOutput, "classlist file : ");
        Path secondCoop = extractPath(secondOutput, "appcds jsa file : ");
        Path secondNocoop = nocoopPath(secondCoop);
        if (!firstClassList.equals(secondClassList)
                || !firstCoop.equals(secondCoop)
                || !firstNocoop.equals(secondNocoop)) {
            throw new RuntimeException("Long identity names are not stable: "
                    + firstClassList + ", " + secondClassList + ", "
                    + firstCoop + ", " + secondCoop + ", "
                    + firstNocoop + ", " + secondNocoop);
        }
        waitForFile(secondCoop, 60);
        waitForFile(secondNocoop, 60);
        return firstClassList;
    }

    private static void verifyLongBaseIdentity(Path classes, String mainClass)
            throws Exception {
        // HotSpot's os::open() on Linux rejects any path longer than MAX_PATH
        // (2*K = 2048 bytes) before invoking the underlying syscall -- a limit
        // stricter than the OS PATH_MAX (4096). Both the CDS class list read
        // and the archive write are routed through os::open(), so keep the
        // total path (base + '/' + the NAME_MAX-bounded identity file name)
        // strictly below MAX_PATH; otherwise the dump child cannot even read
        // the class list, no matter how short the file name is.
        final int hotspotMaxPath = 2048; // os::open MAX_PATH on Linux (2*K)
        final int nameMax = 255;        // _PC_NAME_MAX; bound enforced by build_identity_stem
        // 2048 - 1('/') - 255(NAME_MAX) - 16(safety slack) = 1776
        Path cds = directoryWithLength(hotspotMaxPath - 1 - nameMax - 16);

        runJava(classes, cds, mainClass, true, Boolean.TRUE);
        Path classList = waitForSingleFile(cds, "*.lst", 30);
        String stem = removeSuffix(classList.getFileName().toString(), ".lst");
        Path coop = cds.resolve(stem + "_coop.jsa");
        Path nocoop = cds.resolve(stem + "_nocoop.jsa");

        runJava(classes, cds, mainClass, true, Boolean.TRUE);
        waitForFile(coop, 30);
        waitForFile(nocoop, 30);
    }

    private static Path directoryWithLength(int targetLength) throws Exception {
        Path current = work.resolve("deep-base");
        int remaining = targetLength - current.toString().length();
        int componentCount = (remaining + 200) / 201;
        int characterCount = remaining - componentCount;
        if (componentCount <= 0 || characterCount < componentCount) {
            throw new RuntimeException("Cannot build directory of length " + targetLength
                    + " from " + current);
        }

        for (int i = 0; i < componentCount; i++) {
            int componentLength = characterCount / componentCount
                    + (i < characterCount % componentCount ? 1 : 0);
            char[] name = new char[componentLength];
            Arrays.fill(name, 'd');
            current = current.resolve(new String(name));
        }
        if (current.toString().length() != targetLength) {
            throw new RuntimeException("Unexpected deep directory length: "
                    + current.toString().length() + ", expected " + targetLength);
        }
        Files.createDirectories(current);
        return current;
    }

    private static Path waitForSingleFile(Path directory, String glob,
                                          long timeoutSeconds) throws Exception {
        long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(timeoutSeconds);
        while (System.nanoTime() < deadline) {
            Path match = null;
            try (DirectoryStream<Path> files = Files.newDirectoryStream(directory, glob)) {
                for (Path file : files) {
                    if (match != null) {
                        throw new RuntimeException("Multiple files matched " + glob
                                + " in " + directory);
                    }
                    match = file;
                }
            }
            if (match != null) {
                waitForFile(match, timeoutSeconds);
                return match;
            }
            Thread.sleep(200);
        }
        throw new RuntimeException("Timed out waiting for " + glob + " in " + directory);
    }

    private static Path nocoopPath(Path coop) {
        String name = coop.getFileName().toString();
        if (!name.endsWith("_coop.jsa")) {
            throw new RuntimeException("Not a compressed-oops archive: " + coop);
        }
        String nocoop = name.substring(0, name.length() - "_coop.jsa".length())
                + "_nocoop.jsa";
        return coop.resolveSibling(nocoop);
    }

    private static void assertCommonStem(Path classList, Path coop, Path nocoop) {
        String listStem = removeSuffix(classList.getFileName().toString(), ".lst");
        String coopStem = removeSuffix(coop.getFileName().toString(), "_coop.jsa");
        String nocoopStem = removeSuffix(nocoop.getFileName().toString(), "_nocoop.jsa");
        if (!listStem.equals(coopStem) || !listStem.equals(nocoopStem)) {
            throw new RuntimeException("Identity files do not share one stem: "
                    + classList + ", " + coop + ", " + nocoop);
        }
    }

    private static String removeSuffix(String value, String suffix) {
        if (!value.endsWith(suffix)) {
            throw new RuntimeException("Missing suffix " + suffix + ": " + value);
        }
        return value.substring(0, value.length() - suffix.length());
    }

    private static void assertBoundedHashName(Path path, String suffix) {
        String fileName = path.getFileName().toString();
        int byteLength = fileName.getBytes(StandardCharsets.UTF_8).length;
        if (byteLength > 255) {
            throw new RuntimeException("File name exceeds Linux fallback limit: "
                    + byteLength + " bytes: " + fileName);
        }
        if (!fileName.matches("appcds_.*_h[0-9a-f]{16}.*")
                || !fileName.endsWith(suffix)) {
            throw new RuntimeException("Missing stable hash suffix: " + fileName);
        }
    }

    private static void assertSelectedPaths(String output, Path classList, Path archive) {
        assertContains(output, "classlist file : " + classList);
        assertContains(output, "appcds jsa file : " + archive);
    }

    private static Path extractPath(String output, String marker) {
        int start = output.indexOf(marker);
        if (start < 0) {
            throw new RuntimeException("Missing path marker: " + marker
                    + System.lineSeparator() + output);
        }
        start += marker.length();
        int end = output.indexOf('\n', start);
        String value = end < 0 ? output.substring(start) : output.substring(start, end);
        return Paths.get(value.trim());
    }

    private static void compileApplication(Path classes, String className) throws Exception {
        int separator = className.lastIndexOf('.');
        String packageName = className.substring(0, separator);
        String simpleName = className.substring(separator + 1);
        Path source = work.resolve("src")
                .resolve(className.replace('.', '/') + ".java");
        Files.createDirectories(source.getParent());
        Files.write(source, Arrays.asList(
                "package " + packageName + ";",
                "public class " + simpleName + " {",
                "    public static void main(String[] args) {",
                "        System.out.println(\"" + simpleName + "\");",
                "    }",
                "}"), StandardCharsets.UTF_8);

        ProcessBuilder javac = new ProcessBuilder(
                jdkTool("javac"),
                "-d", classes.toString(),
                source.toString());
        execute(javac, "javac-" + simpleName, 30);
    }

    private static String runJava(Path classes, Path cds, String mainClass,
                                  boolean identityNames, Boolean compressedOops)
            throws Exception {
        List<String> command = commonJavaCommand(cds, identityNames, compressedOops);
        command.add("-cp");
        command.add(classes.toString());
        command.add(mainClass);

        ProcessBuilder java = new ProcessBuilder(command);
        java.directory(work.toFile());
        return execute(java, "java-" + mainClass.substring(mainClass.lastIndexOf('.') + 1),
                60);
    }

    private static String runVersion(Path cds, boolean compressedOops) throws Exception {
        List<String> command = commonJavaCommand(cds, true,
                Boolean.valueOf(compressedOops));
        command.add("-version");
        ProcessBuilder java = new ProcessBuilder(command);
        java.directory(work.toFile());
        return execute(java, "java-version-" + (compressedOops ? "coop" : "nocoop"), 60);
    }

    private static List<String> commonJavaCommand(Path cds, boolean identityNames,
                                                  Boolean compressedOops) {
        List<String> command = new ArrayList<String>();
        command.add(jdkTool("java"));
        command.add("-Xms128m");
        command.add("-Xmx256m");
        command.add("-XX:AutoSharedArchivePath=" + cds);
        if (identityNames) {
            command.add("-XX:+UseAutoAppCDSIdentity");
        }
        command.add("-XX:+PrintAutoAppCDS");
        if (compressedOops != null) {
            command.add(compressedOops.booleanValue()
                    ? "-XX:+UseCompressedOops" : "-XX:-UseCompressedOops");
        }
        return command;
    }

    private static String execute(ProcessBuilder processBuilder, String label,
                                  long timeoutSeconds) throws Exception {
        Path logs = work.resolve("logs");
        Files.createDirectories(logs);
        Path log = logs.resolve(String.format("%03d-%s.log", ++commandSequence, label));
        processBuilder.redirectErrorStream(true);
        processBuilder.redirectOutput(log.toFile());
        Process process = processBuilder.start();
        if (!process.waitFor(timeoutSeconds, TimeUnit.SECONDS)) {
            process.destroyForcibly();
            throw new RuntimeException("Timed out waiting for " + processBuilder.command());
        }
        String output = new String(Files.readAllBytes(log), StandardCharsets.UTF_8);
        if (process.exitValue() != 0) {
            throw new RuntimeException("Unexpected exit value " + process.exitValue()
                    + " from " + processBuilder.command() + System.lineSeparator() + output);
        }
        return output;
    }

    private static void waitForFile(Path file, long timeoutSeconds) throws Exception {
        long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(timeoutSeconds);
        long previousSize = -1;
        int stableSamples = 0;
        while (System.nanoTime() < deadline) {
            if (Files.exists(file)) {
                long size = Files.size(file);
                if (size > 0 && size == previousSize) {
                    stableSamples++;
                    if (stableSamples >= 3) {
                        return;
                    }
                } else {
                    stableSamples = 0;
                    previousSize = size;
                }
            }
            Thread.sleep(200);
        }
        throw new RuntimeException("Timed out waiting for stable file: " + file);
    }

    private static void assertUnchanged(Path archive, FileTime expected) throws Exception {
        FileTime actual = Files.getLastModifiedTime(archive);
        if (!expected.equals(actual)) {
            throw new RuntimeException("Archive was regenerated instead of reused: "
                    + archive + ", before=" + expected + ", after=" + actual);
        }
    }

    private static String identityStem(String target) {
        return "appcds_" + effectiveUserIdentity + "_" + target;
    }

    private static String normalize(String value) {
        StringBuilder result = new StringBuilder(value.length());
        for (int i = 0; i < value.length(); i++) {
            char ch = value.charAt(i);
            if (ch == '/' || ch <= 0x1f || ch == 0x7f) {
                result.append('_');
            } else {
                result.append(ch);
            }
        }
        return result.toString();
    }

    private static void assertContains(String output, String expected) {
        if (!output.contains(expected)) {
            throw new RuntimeException("Missing expected output: " + expected
                    + System.lineSeparator() + output);
        }
    }

    private static String jdkTool(String tool) {
        String testJdk = System.getProperty("test.jdk");
        if (testJdk == null || testJdk.length() == 0) {
            testJdk = System.getProperty("compile.jdk");
        }
        if (testJdk == null || testJdk.length() == 0) {
            testJdk = System.getProperty("java.home");
        }
        Path path = Paths.get(testJdk, "bin", tool);
        if (!Files.exists(path) && "jre".equals(Paths.get(testJdk).getFileName().toString())) {
            path = Paths.get(testJdk).getParent().resolve("bin").resolve(tool);
        }
        if (!Files.exists(path)) {
            throw new RuntimeException("Could not find JDK tool: " + path);
        }
        return path.toString();
    }

    private static final class ArchivePair {
        private final Path coop;
        private final Path nocoop;

        private ArchivePair(Path coop, Path nocoop) {
            this.coop = coop;
            this.nocoop = nocoop;
        }
    }
}
