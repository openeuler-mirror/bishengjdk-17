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
// Checkstyle: stop


package org.graalvm.compiler.jtt.hotpath;

import org.junit.Test;

import org.graalvm.compiler.jtt.JTTTest;

/*
 */
public class HP_inline02 extends JTTTest {

    public static int test(int count) {
        int sum = 0;
        for (int i = 0; i < count; i++) {
            sum += foo(i, sum);
        }
        return sum;
    }

    public static int foo(int x, int y) {
        if (x < 18) {
            return bar(x, x - y);
        }
        return bar(x, x + y);
    }

    public static int bar(int x, int y) {
        if (x < 15) {
            return car(x, x + y);
        }
        return x - 1;
    }

    @SuppressWarnings("unused")
    public static int car(int x, int y) {
        if (x < 13) {
            return x + 1;
        }
        return x - 1;
    }

    @Test
    public void run0() throws Throwable {
        runTest("test", 20);
    }

}
