/*
 * Copyright (c) 2014, 2023, Oracle and/or its affiliates. All rights reserved.
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

/**
 * @test
 * @bug 8035968
 * @summary Verify that SHA-256 multi block intrinsic is actually used.
 * @comment the test verifies compilation of java.base methods, so it can't be run w/ AOT'ed java.base
 * @requires !vm.aot.enabled
 *
 * @library /test/lib /
 * @modules java.base/jdk.internal.misc
 *          java.management
 *
 * @build jdk.test.whitebox.WhiteBox
 * @run driver jdk.test.lib.helpers.ClassFileInstaller jdk.test.whitebox.WhiteBox
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions
 *                   -XX:+WhiteBoxAPI -Xbatch -XX:CompileThreshold=500
 *                   -XX:Tier4InvocationThreshold=500
 *                   -XX:+LogCompilation -XX:LogFile=positive_224.log
 *                   -XX:CompileOnly=sun/security/provider/DigestBase
 *                   -XX:CompileOnly=sun/security/provider/SHA2
 *                   -XX:+UseSHA256Intrinsics -XX:-UseMD5Intrinsics
 *                   -XX:-UseSHA1Intrinsics -XX:-UseSHA512Intrinsics
 *                   -Dalgorithm=SHA-224
 *                   compiler.intrinsics.sha.sanity.TestSHA256MultiBlockIntrinsics
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions
 *                   -XX:+WhiteBoxAPI -Xbatch -XX:CompileThreshold=500
 *                   -XX:Tier4InvocationThreshold=500
 *                   -XX:+LogCompilation -XX:LogFile=positive_224_def.log
 *                   -XX:CompileOnly=sun/security/provider/DigestBase
 *                   -XX:CompileOnly=sun/security/provider/SHA2
 *                   -XX:+UseSHA256Intrinsics -Dalgorithm=SHA-224
 *                   compiler.intrinsics.sha.sanity.TestSHA256MultiBlockIntrinsics
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions
 *                   -XX:+WhiteBoxAPI -Xbatch -XX:CompileThreshold=500
 *                   -XX:Tier4InvocationThreshold=500
 *                   -XX:+LogCompilation -XX:LogFile=negative_224.log
 *                   -XX:CompileOnly=sun/security/provider/DigestBase
 *                   -XX:CompileOnly=sun/security/provider/SHA2 -XX:-UseSHA
 *                   -Dalgorithm=SHA-224
 *                   compiler.intrinsics.sha.sanity.TestSHA256MultiBlockIntrinsics
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions
 *                   -XX:+WhiteBoxAPI -Xbatch -XX:CompileThreshold=500
 *                   -XX:Tier4InvocationThreshold=500
 *                   -XX:+LogCompilation -XX:LogFile=positive_256.log
 *                   -XX:CompileOnly=sun/security/provider/DigestBase
 *                   -XX:CompileOnly=sun/security/provider/SHA2
 *                   -XX:+UseSHA256Intrinsics -XX:-UseMD5Intrinsics
 *                   -XX:-UseSHA1Intrinsics -XX:-UseSHA512Intrinsics
 *                   -Dalgorithm=SHA-256
 *                   compiler.intrinsics.sha.sanity.TestSHA256MultiBlockIntrinsics
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions
 *                   -XX:+WhiteBoxAPI -Xbatch -XX:CompileThreshold=500
 *                   -XX:Tier4InvocationThreshold=500
 *                   -XX:+LogCompilation -XX:LogFile=positive_256_def.log
 *                   -XX:CompileOnly=sun/security/provider/DigestBase
 *                   -XX:CompileOnly=sun/security/provider/SHA
 *                   -XX:+UseSHA256Intrinsics -Dalgorithm=SHA-256
 *                   compiler.intrinsics.sha.sanity.TestSHA256MultiBlockIntrinsics
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions
 *                   -XX:+WhiteBoxAPI -Xbatch -XX:CompileThreshold=500
 *                   -XX:Tier4InvocationThreshold=500
 *                   -XX:+LogCompilation -XX:LogFile=negative_256.log
 *                   -XX:CompileOnly=sun/security/provider/DigestBase
 *                   -XX:CompileOnly=sun/security/provider/SHA -XX:-UseSHA
 *                   -Dalgorithm=SHA-256
 *                   compiler.intrinsics.sha.sanity.TestSHA256MultiBlockIntrinsics
 * @run main/othervm -DverificationStrategy=VERIFY_INTRINSIC_USAGE
 *                   compiler.testlibrary.intrinsics.Verifier positive_224.log positive_256.log
 *                   positive_224_def.log positive_256_def.log negative_224.log
 *                   negative_256.log
 */

package compiler.intrinsics.sha.sanity;
import compiler.testlibrary.sha.predicate.IntrinsicPredicates;

public class TestSHA256MultiBlockIntrinsics {
    public static void main(String args[]) throws Exception {
        new DigestSanityTestBase(IntrinsicPredicates.isSHA256IntrinsicAvailable(),
                DigestSanityTestBase.MB_INTRINSIC_ID).test();
    }
}
