/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates. All rights reserved.
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
 * @summary check at-file jaotc support
 * @comment based on CompileClassTest with arguments wrote in 'jaotc.cmd' file
 * @requires vm.aot
 * @bug 8215322
 * @library / /test/lib /testlibrary
 * @modules java.base/jdk.internal.misc
 * @build compiler.aot.cli.jaotc.AtFileTest
 * @run driver jdk.test.lib.helpers.ClassFileInstaller compiler.aot.cli.jaotc.data.HelloWorldOne
 * @run driver compiler.aot.cli.jaotc.AtFileTest
 */

package compiler.aot.cli.jaotc;

import compiler.aot.cli.jaotc.data.HelloWorldOne;
import java.io.File;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.Files;
import java.util.List;
import jdk.test.lib.Asserts;
import jdk.test.lib.process.OutputAnalyzer;

public class AtFileTest {
    public static void main(String[] args) throws Exception {
        Path file = Paths.get("jaotc.cmd");
        Files.write(file, List.of("--class-name",
                JaotcTestHelper.getClassAotCompilationName(HelloWorldOne.class)));
        OutputAnalyzer oa = JaotcTestHelper.compileLibrary("@" + file.toString());
        oa.shouldHaveExitValue(0);
        File compiledLibrary = new File(JaotcTestHelper.DEFAULT_LIB_PATH);
        Asserts.assertTrue(compiledLibrary.exists(), "Compiled library file missing");
        Asserts.assertGT(compiledLibrary.length(), 0L, "Unexpected compiled library size");
        JaotcTestHelper.checkLibraryUsage(HelloWorldOne.class.getName());
    }
}
