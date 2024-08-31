/*
 * Copyright (c) 2024, Huawei Technologies Co., Ltd. All rights reserved.
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
 * @requires vm.aot
 * @library /test/lib /testlibrary /
 * @build compiler.aot.TestAotPrint
 *        jdk.test.whitebox.WhiteBox
 * @run driver jdk.test.lib.helpers.ClassFileInstaller jdk.test.whitebox.WhiteBox
 * @run driver compiler.aot.AotCompiler -libname libAotString.so
 *      -class java.lang.String
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseAOT
 *      -XX:AOTLibrary=./libAotString.so -XX:+PrintCompilation
 *      -XX:+UnlockDiagnosticVMOptions -XX:+LogCompilation
 *      -XX:+WhiteBoxAPI -Xbootclasspath/a:.
 *      -XX:+PrintAOT compiler.aot.TestAotPrint
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseAOT
 *      -XX:AOTLibrary=./libAotString.so -XX:+PrintCompilation
 *      -XX:+UnlockDiagnosticVMOptions -XX:+LogCompilation
 *      -XX:+WhiteBoxAPI -Xbootclasspath/a:.
 *      -XX:+PrintAOT compiler.aot.TestAotPrint true
 */

package compiler.aot;

import compiler.aot.HelloWorldPrinter;
import jdk.test.lib.cli.CommandLineOptionTest;
import jdk.test.lib.process.ExitCode;
import jdk.test.whitebox.WhiteBox;

public class TestAotPrint {
    private static final WhiteBox WB = WhiteBox.getWhiteBox();

    public static void main(String[] args) throws Throwable {
        boolean preserve_static_stubs = false;
        if (args.length > 0 && "true".equals(args[0])) {
            preserve_static_stubs = true;
        }
        HelloWorldPrinter.print();
        WB.clearInlineCaches(preserve_static_stubs);
    }
}
