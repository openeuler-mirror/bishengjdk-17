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


package org.graalvm.compiler.core.test;

import org.graalvm.compiler.core.common.type.Stamp;
import org.graalvm.compiler.core.common.type.StampFactory;
import org.junit.Test;

import jdk.vm.ci.meta.JavaConstant;
import jdk.vm.ci.meta.JavaKind;
import jdk.vm.ci.meta.MemoryAccessProvider;

/**
 *
 */
public class StampMemoryAccessTest extends GraalCompilerTest {

    @Test
    public void testReadPrimitive() {
        MemoryAccessProvider memory = getConstantReflection().getMemoryAccessProvider();
        Stamp stamp = StampFactory.forKind(JavaKind.Long);
        JavaConstant objectBase = getSnippetReflection().forObject("");
        assertTrue(stamp.readConstant(memory, objectBase, 128) == null);
        JavaConstant arrayBase = getSnippetReflection().forObject(new int[]{});
        assertTrue(stamp.readConstant(memory, arrayBase, 128) == null);
    }

    @Test
    public void testReadObject() {
        MemoryAccessProvider memory = getConstantReflection().getMemoryAccessProvider();
        Stamp stamp = StampFactory.forKind(JavaKind.Object);
        JavaConstant objectBase = getSnippetReflection().forObject("");
        assertTrue(stamp.readConstant(memory, objectBase, 128) == null);
        JavaConstant arrayBase = getSnippetReflection().forObject(new int[]{});
        assertTrue(stamp.readConstant(memory, arrayBase, 128) == null);
    }
}
