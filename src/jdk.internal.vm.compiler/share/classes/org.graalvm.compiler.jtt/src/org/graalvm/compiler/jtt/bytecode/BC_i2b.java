/*
 * Copyright (c) 2007, 2023, Oracle and/or its affiliates. All rights reserved.
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


package org.graalvm.compiler.jtt.bytecode;

import org.junit.Test;

import org.graalvm.compiler.jtt.JTTTest;

/*
 */
public class BC_i2b extends JTTTest {

    public static byte test(int a) {
        return (byte) a;
    }

    @Test
    public void run0() throws Throwable {
        runTest("test", -1);
    }

    @Test
    public void run1() throws Throwable {
        runTest("test", 2);
    }

    @Test
    public void run2() throws Throwable {
        runTest("test", 255);
    }

    @Test
    public void run3() throws Throwable {
        runTest("test", 128);
    }

    public static int testInt(int a) {
        return (byte) a;
    }

    @Test
    public void runI0() throws Throwable {
        runTest("testInt", -1);
    }

    @Test
    public void runI1() throws Throwable {
        runTest("testInt", 2);
    }

    @Test
    public void runI2() throws Throwable {
        runTest("testInt", 255);
    }

    @Test
    public void runI3() throws Throwable {
        runTest("testInt", 128);
    }

    public static long testLong(int a) {
        return (byte) a;
    }

    @Test
    public void runL0() throws Throwable {
        runTest("testLong", -1);
    }

    @Test
    public void runL1() throws Throwable {
        runTest("testLong", 2);
    }

    @Test
    public void runL2() throws Throwable {
        runTest("testLong", 255);
    }

    @Test
    public void runL3() throws Throwable {
        runTest("testLong", 128);
    }
}
