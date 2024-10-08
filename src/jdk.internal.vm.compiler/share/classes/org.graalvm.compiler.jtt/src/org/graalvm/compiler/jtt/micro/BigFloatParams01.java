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


package org.graalvm.compiler.jtt.micro;

import org.junit.Test;

import org.graalvm.compiler.jtt.JTTTest;

/*
 */
public class BigFloatParams01 extends JTTTest {

    public static double test(int num) {
        double sum = 0;
        if (num == 0) {
            sum += testA(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testA(1, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testA(2, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testA(3, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testA(4, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testA(5, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testA(6, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testA(7, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testA(8, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        } else if (num == 1) {
            sum += testB(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testB(1, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testB(2, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testB(3, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testB(4, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testB(5, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testB(6, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testB(7, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            sum += testB(8, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        } else if (num == 2) {
            for (int i = 0; i < 9; i++) {
                sum += testA(i, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            }
        } else if (num == 3) {
            for (int i = 0; i < 9; i++) {
                sum += testB(i, 1, 2, 3, 4, 5, 6, 7, 8, 9);
            }
        }
        return sum;
    }

    private static float testA(int choice, float p0, float p1, float p2, float p3, float p4, float p5, float p6, float p7, float p8) {
        switch (choice) {
            case 0:
                return p0;
            case 1:
                return p1;
            case 2:
                return p2;
            case 3:
                return p3;
            case 4:
                return p4;
            case 5:
                return p5;
            case 6:
                return p6;
            case 7:
                return p7;
            case 8:
                return p8;
        }
        return 42;
    }

    private static double testB(int choice, double p0, double p1, double p2, double p3, double p4, double p5, double p6, double p7, double p8) {
        switch (choice) {
            case 0:
                return p0;
            case 1:
                return p1;
            case 2:
                return p2;
            case 3:
                return p3;
            case 4:
                return p4;
            case 5:
                return p5;
            case 6:
                return p6;
            case 7:
                return p7;
            case 8:
                return p8;
        }
        return 42;
    }

    @Test
    public void run0() throws Throwable {
        runTest("test", 0);
    }

    @Test
    public void run1() throws Throwable {
        runTest("test", 1);
    }

    @Test
    public void run2() throws Throwable {
        runTest("test", 2);
    }

    @Test
    public void run3() throws Throwable {
        runTest("test", 3);
    }

    @Test
    public void run4() throws Throwable {
        runTest("test", 4);
    }

}
