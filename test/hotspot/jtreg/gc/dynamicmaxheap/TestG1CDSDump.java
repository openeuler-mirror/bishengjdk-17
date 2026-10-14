/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

package gc.dynamicmaxheap;

/*
 * @test
 * @summary Keep CDS heap archives within the current dynamic heap cap
 * @requires (os.family == "linux") & (os.arch == "aarch64")
 * @requires vm.cds.write.archived.java.heap
 * @library /test/lib
 * @run driver gc.dynamicmaxheap.TestG1CDSDump
 */

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import jtreg.SkippedException;

public class TestG1CDSDump {
    public static void main(String[] args) throws Exception {
        // Only one more 2 MiB region can be committed. Closed archive allocation
        // must use [126, 128) MiB, then open archive allocation must reuse a
        // committed region below it. Starting at the reservation's 512 MiB end
        // would split the archive and relocate its open region outside the heap.
        Path partialCap = dump("g1-dynamic-partial-cap.jsa", true, "126m", "128m", "2m");
        load(partialCap, 128, true, "128m", "2m", null);

        // With plenty of commit budget, allocation must still stay within the
        // current cap instead of starting at the end of the larger reservation.
        Path spareCap = dump("g1-dynamic-spare-cap.jsa", true, "32m", "128m", "1m");
        load(spareCap, 128, true, "128m", "1m", null);

        // Xms == Xmx exhausts the current cap, including after the pre-dump GC.
        // The larger reservation leaves regions above the cap uncommitted.
        // Archive allocation must stay below the cap and reuse a committed
        // free region instead of dereferencing a null HeapRegion.
        Path archive = dump("g1-dynamic-cap.jsa", true, "128m", "1m");

        // A successful -version alone is insufficient: CDS can silently fall
        // back to using only archived classes when heap mapping fails.
        for (String heapSize : new String[] {"128m", "256m"}) {
            load(archive, 128, true, heapSize, "1m", null);
        }
        load(archive, 128, true, "128m", "2m", null);

        // Archived references must remain usable when the runtime uses a
        // different shift or a non-zero oop base.
        load(archive, 128, true, "128m", "1m", "4g");
        load(archive, 128, true, "128m", "1m", "32g");

        // The loader must also handle the low archive placement when the
        // dynamic heap feature is disabled at run time.
        load(archive, 128, false, "128m", "1m", null);

        // Loading into a smaller current heap must move the archive down;
        // leaving it at its original addresses can require commits beyond the cap.
        Path largerHeap = dump("g1-dynamic-cap-256m.jsa", true, "256m", "1m");
        load(largerHeap, 256, true, "128m", "1m", null);

        Path largeRegions = dump("g1-dynamic-cap-2m.jsa", true, "128m", "2m");
        load(largeRegions, 128, true, "128m", "1m", null);

        // An ordinary archive still needs relocation into the committed low
        // portion of a larger dynamic heap reservation.
        Path ordinary = dump("g1-ordinary.jsa", false, "128m", "1m");
        load(ordinary, 128, true, "128m", "1m", null);
        load(ordinary, 128, false, "128m", "1m", null);
    }

    private static List<String> options(boolean dynamicHeap, String initialHeapSize,
                                        String heapSize, String regionSize) {
        List<String> options = new ArrayList<>(Arrays.asList(
                "-XX:+UseG1GC",
                "-Xms" + initialHeapSize, "-Xmx" + heapSize,
                "-XX:G1HeapRegionSize=" + regionSize,
                "-XX:+UseCompressedOops",
                "-XX:+UseCompressedClassPointers",
                "-Xlog:cds=info"));
        if (dynamicHeap) {
            options.add("-XX:+ElasticMaxHeap");
            options.add("-XX:ElasticMaxHeapSize=512m");
        } else {
            options.add("-XX:-ElasticMaxHeap");
        }
        return options;
    }

    private static OutputAnalyzer execute(List<String> options) throws Exception {
        return ProcessTools.executeProcess(
                ProcessTools.createLimitedTestJavaProcessBuilder(
                        options.toArray(new String[0])));
    }

    private static Path dump(String name, boolean dynamicHeap, String heapSize, String regionSize)
            throws Exception {
        return dump(name, dynamicHeap, heapSize, heapSize, regionSize);
    }

    private static Path dump(String name, boolean dynamicHeap, String initialHeapSize,
                             String heapSize, String regionSize) throws Exception {
        Path archive = Path.of(name).toAbsolutePath();
        List<String> options = options(dynamicHeap, initialHeapSize, heapSize, regionSize);
        options.add("-Xshare:dump");
        options.add("-XX:SharedArchiveFile=" + archive);
        OutputAnalyzer output = execute(options);

        output.shouldHaveExitValue(0);
        if (output.getOutput().contains("can only be assigned on HiSi now")) {
            throw new SkippedException("Dynamic max heap requires a supported HiSi CPU");
        }
        output.shouldNotContain("automatically disabled");
        output.shouldContain("Dumping objects to closed archive heap region");
        if (!Files.isRegularFile(archive) || Files.size(archive) == 0) {
            throw new RuntimeException("CDS dump did not create a non-empty archive: " + archive);
        }
        return archive;
    }

    private static void load(Path archive, int dumpHeapEndOffsetMB, boolean dynamicHeap,
                             String heapSize, String regionSize, String heapBase)
            throws Exception {
        System.out.println("Loading " + archive.getFileName() + ": dynamic=" + dynamicHeap
                + ", heap=" + heapSize + ", region=" + regionSize + ", base=" + heapBase);
        List<String> options = options(dynamicHeap, heapSize, heapSize, regionSize);
        if (heapBase != null) {
            options.add("-XX:HeapBaseMinAddress=" + heapBase);
        }
        options.add("-Xshare:on");
        options.add("-XX:SharedArchiveFile=" + archive);
        options.add("-version");
        OutputAnalyzer output = execute(options);
        output.shouldHaveExitValue(0);
        output.shouldNotContain("Unable to allocate region");
        output.shouldContain("full module graph: enabled");
        if (heapBase != null) {
            output.shouldContain("incompatible oop encoding mode");
        }

        // The first heap range in the CDS log comes from the archive header.
        // Its upper bound must describe the actual archive placement below
        // the end of the larger reservation, even with spare commit budget.
        Matcher range = Pattern.compile(
                "heap range = \\[0x([0-9a-fA-F]+) - 0x([0-9a-fA-F]+)\\]")
                .matcher(output.getOutput());
        if (!range.find()) {
            throw new RuntimeException("Missing dump-time heap range in CDS log");
        }
        long begin = Long.parseUnsignedLong(range.group(1), 16);
        long end = Long.parseUnsignedLong(range.group(2), 16);
        if (end - begin != dumpHeapEndOffsetMB * 1024L * 1024) {
            throw new RuntimeException("Unexpected dump-time heap relocation anchor: "
                    + range.group());
        }
    }
}
