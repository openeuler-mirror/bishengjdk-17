/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates. All rights reserved.
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


package org.graalvm.compiler.replacements.amd64;

import static org.graalvm.compiler.replacements.NodeIntrinsificationProvider.INJECTED_TARGET;

import org.graalvm.compiler.api.replacements.ClassSubstitution;
import org.graalvm.compiler.api.replacements.Fold;
import org.graalvm.compiler.api.replacements.MethodSubstitution;
import org.graalvm.compiler.core.common.SuppressFBWarnings;
import org.graalvm.compiler.replacements.nodes.BitScanForwardNode;
import org.graalvm.compiler.replacements.nodes.BitScanReverseNode;

import jdk.vm.ci.amd64.AMD64;
import jdk.vm.ci.code.TargetDescription;

@ClassSubstitution(Integer.class)
public class AMD64IntegerSubstitutions {

    @Fold
    static boolean lzcnt(@Fold.InjectedParameter TargetDescription target) {
        AMD64 arch = (AMD64) target.arch;
        return arch.getFeatures().contains(AMD64.CPUFeature.LZCNT) && arch.getFlags().contains(AMD64.Flag.UseCountLeadingZerosInstruction);
    }

    @Fold
    static boolean tzcnt(@Fold.InjectedParameter TargetDescription target) {
        AMD64 arch = (AMD64) target.arch;
        return arch.getFeatures().contains(AMD64.CPUFeature.BMI1) && arch.getFlags().contains(AMD64.Flag.UseCountTrailingZerosInstruction);
    }

    @MethodSubstitution
    @SuppressFBWarnings(value = "NP_NULL_PARAM_DEREF_NONVIRTUAL", justification = "foldable method parameters are injected")
    public static int numberOfLeadingZeros(int i) {
        if (lzcnt(INJECTED_TARGET)) {
            return AMD64CountLeadingZerosNode.countLeadingZeros(i);
        }
        if (i == 0) {
            return 32;
        }
        return 31 - BitScanReverseNode.unsafeScan(i);
    }

    @MethodSubstitution
    @SuppressFBWarnings(value = "NP_NULL_PARAM_DEREF_NONVIRTUAL", justification = "foldable method parameters are injected")
    public static int numberOfTrailingZeros(int i) {
        if (tzcnt(INJECTED_TARGET)) {
            return AMD64CountTrailingZerosNode.countTrailingZeros(i);
        }
        if (i == 0) {
            return 32;
        }
        return BitScanForwardNode.unsafeScan(i);
    }
}
