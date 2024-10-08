/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates. All rights reserved.
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


package org.graalvm.compiler.jtt.micro;

import org.junit.Test;

import org.graalvm.compiler.jtt.JTTTest;

public class ConstantLoadTest extends JTTTest {

    private static final class MyClass {
        public long a;
        public long b;

        MyClass(long a, long b) {
            this.a = a;
            this.b = b;
        }
    }

    private static final MyClass myClass = new MyClass(Long.MIN_VALUE, Long.MAX_VALUE);
    private static final long myLong = Long.MAX_VALUE;

    public static long test(int arg) {
        if (arg == 0) {
            return myClass.a / arg + myLong;
        }
        if (arg == 1) {
            return myClass.b - arg + myLong;
        }
        long r = 1;
        for (int i = 0; i < arg; i++) {
            r *= i;
        }
        return r;
    }

    @Test
    public void run0() throws Throwable {
        runTest("test", 0);
    }

    @Test
    public void run1() throws Throwable {
        runTest("test", -1);
    }

    @Test
    public void run2() throws Throwable {
        runTest("test", 1);
    }

    @Test
    public void run3() throws Throwable {
        runTest("test", 2);
    }

}
