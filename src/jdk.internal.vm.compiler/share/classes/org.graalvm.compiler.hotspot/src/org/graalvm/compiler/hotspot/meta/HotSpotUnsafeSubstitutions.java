/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates. All rights reserved.
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


package org.graalvm.compiler.hotspot.meta;

import org.graalvm.compiler.api.replacements.ClassSubstitution;
import org.graalvm.compiler.api.replacements.MethodSubstitution;
import org.graalvm.compiler.hotspot.replacements.UnsafeCopyMemoryNode;
import org.graalvm.compiler.serviceprovider.JavaVersionUtil;

@ClassSubstitution(className = {"jdk.internal.misc.Unsafe", "sun.misc.Unsafe"})
public class HotSpotUnsafeSubstitutions {

    public static final String copyMemoryName = JavaVersionUtil.JAVA_SPEC <= 8 ? "copyMemory" : "copyMemory0";

    @SuppressWarnings("unused")
    @MethodSubstitution(isStatic = false)
    static void copyMemory(Object receiver, Object srcBase, long srcOffset, Object destBase, long destOffset, long bytes) {
        UnsafeCopyMemoryNode.copyMemory(false, receiver, srcBase, srcOffset, destBase, destOffset, bytes);
    }

    @SuppressWarnings("unused")
    @MethodSubstitution(value = "copyMemory", isStatic = false)
    static void copyMemoryGuarded(Object receiver, Object srcBase, long srcOffset, Object destBase, long destOffset, long bytes) {
        UnsafeCopyMemoryNode.copyMemory(true, receiver, srcBase, srcOffset, destBase, destOffset, bytes);
    }
}
