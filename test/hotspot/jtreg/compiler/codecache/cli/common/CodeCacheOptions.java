/*
 * Copyright (c) 2014, 2022, Oracle and/or its affiliates. All rights reserved.
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
package compiler.codecache.cli.common;

import jdk.test.lib.cli.CommandLineOptionTest;
import jdk.test.whitebox.code.BlobType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;

public class CodeCacheOptions {
    public static final String SEGMENTED_CODE_CACHE = "SegmentedCodeCache";

    public static final EnumSet<BlobType> NON_SEGMENTED_HEAPS
            = EnumSet.of(BlobType.All);
    public static final EnumSet<BlobType> JBOLT_HEAPS
            = EnumSet.of(BlobType.MethodJBoltHot, BlobType.MethodJBoltTmp);
    public static final EnumSet<BlobType> ALL_SEGMENTED_HEAPS
            = EnumSet.complementOf(union(NON_SEGMENTED_HEAPS, JBOLT_HEAPS));
    public static final EnumSet<BlobType> ALL_SEGMENTED_HEAPS_WITH_JBOLT
            = union(ALL_SEGMENTED_HEAPS, JBOLT_HEAPS);
    public static final EnumSet<BlobType> SEGMENTED_HEAPS_WO_PROFILED
            = EnumSet.of(BlobType.NonNMethod, BlobType.MethodNonProfiled);
    public static final EnumSet<BlobType> ONLY_NON_METHODS_HEAP
            = EnumSet.of(BlobType.NonNMethod);

    public final long reserved;
    public final long nonNmethods;
    public final long nonProfiled;
    public final long profiled;
    public final long jboltHot;
    public final long jboltTmp;
    public final boolean segmented;
    public final boolean useJBolt;

    public static long mB(long val) {
        return CodeCacheOptions.kB(val) * 1024L;
    }

    public static long kB(long val) {
        return val * 1024L;
    }

    public static <E extends Enum<E>> EnumSet<E> union(EnumSet<E> e1, EnumSet<E> e2) {
        EnumSet<E> res = EnumSet.copyOf(e1);
        res.addAll(e2);
        return res;
    }

    public CodeCacheOptions(long reserved) {
        this.reserved = reserved;
        this.nonNmethods = 0;
        this.nonProfiled = 0;
        this.profiled = 0;
        this.jboltHot = 0;
        this.jboltTmp = 0;
        this.segmented = false;
        this.useJBolt = false;
    }

    public CodeCacheOptions(long reserved, long nonNmethods, long nonProfiled,
            long profiled) {
        this.reserved = reserved;
        this.nonNmethods = nonNmethods;
        this.nonProfiled = nonProfiled;
        this.profiled = profiled;
        this.jboltHot = 0;
        this.jboltTmp = 0;
        this.segmented = true;
        this.useJBolt = false;
    }

    /**
     * No tests for JBolt yet as the related VM options are experimental now.
     */
    public CodeCacheOptions(long reserved, long nonNmethods, long nonProfiled,
            long profiled, long jboltHot, long jboltTmp) {
        this.reserved = reserved;
        this.nonNmethods = nonNmethods;
        this.nonProfiled = nonProfiled;
        this.profiled = profiled;
        this.jboltHot = jboltHot;
        this.jboltTmp = jboltTmp;
        this.segmented = true;
        this.useJBolt = true;
    }

    public long sizeForHeap(BlobType heap) {
        switch (heap) {
            case All:
                return this.reserved;
            case NonNMethod:
                return this.nonNmethods;
            case MethodNonProfiled:
                return this.nonProfiled;
            case MethodProfiled:
                return this.profiled;
            case MethodJBoltHot:
                return this.jboltHot;
            case MethodJBoltTmp:
                return this.jboltTmp;
            default:
                throw new Error("Unknown heap: " + heap.name());
        }
    }

    public String[] prepareOptions(String... additionalOptions) {
        List<String> options = new ArrayList<>();
        Collections.addAll(options, additionalOptions);
        Collections.addAll(options,
                CommandLineOptionTest.prepareBooleanFlag(
                        SEGMENTED_CODE_CACHE, segmented),
                CommandLineOptionTest.prepareNumericFlag(
                        BlobType.All.sizeOptionName, reserved));

        if (segmented) {
            Collections.addAll(options,
                    CommandLineOptionTest.prepareNumericFlag(
                            BlobType.NonNMethod.sizeOptionName, nonNmethods),
                    CommandLineOptionTest.prepareNumericFlag(
                            BlobType.MethodNonProfiled.sizeOptionName,
                            nonProfiled),
                    CommandLineOptionTest.prepareNumericFlag(
                            BlobType.MethodProfiled.sizeOptionName, profiled));
        }

        if (useJBolt) {
            Collections.addAll(options,
                    CommandLineOptionTest.prepareNumericFlag(
                            BlobType.MethodJBoltHot.sizeOptionName, jboltHot),
                    CommandLineOptionTest.prepareNumericFlag(
                            BlobType.MethodJBoltTmp.sizeOptionName, jboltTmp));
        }

        return options.toArray(new String[options.size()]);
    }

    public CodeCacheOptions mapOptions(EnumSet<BlobType> involvedCodeHeaps) {
        if (involvedCodeHeaps.isEmpty()
                || involvedCodeHeaps.equals(NON_SEGMENTED_HEAPS)
                || involvedCodeHeaps.equals(ALL_SEGMENTED_HEAPS_WITH_JBOLT)) {
            return this;
        } else if (involvedCodeHeaps.equals(ALL_SEGMENTED_HEAPS)) {
            return new CodeCacheOptions(reserved, nonNmethods,
                    nonProfiled + jboltHot + jboltTmp, profiled);
        } else if (involvedCodeHeaps.equals(SEGMENTED_HEAPS_WO_PROFILED)) {
            return new CodeCacheOptions(reserved, nonNmethods,
                    profiled + nonProfiled, 0L);
        } else if (involvedCodeHeaps.equals(ONLY_NON_METHODS_HEAP)) {
            return new CodeCacheOptions(reserved, nonNmethods + profiled
                    + nonProfiled, 0L, 0L);
        } else {
            throw new Error("Test bug: unexpected set of code heaps involved "
                    + "into test.");
        }
    }
}
