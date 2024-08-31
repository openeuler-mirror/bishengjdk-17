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
 * @build compiler.aot.TestAotWithRetransform
 *        compiler.aot.AotCompiler
 * @run driver compiler.aot.AotCompiler -libname libTestAotWithRetransform.so
 *      -class compiler.aot.HelloWorldPrinter
 *      -compile compiler.aot.HelloWorldPrinter.print()V
 * @run driver jdk.test.lib.util.JavaAgentBuilder
 *      compiler.aot.TestAotWithRetransform TestAotWithRetransform.jar
 * @run driver compiler.aot.TestAotWithRetransform
 */

package compiler.aot;

import compiler.aot.HelloWorldPrinter;
import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.IllegalClassFormatException;
import java.lang.instrument.Instrumentation;
import java.security.ProtectionDomain;
import java.util.ArrayList;
import java.util.List;
import jdk.test.lib.cli.CommandLineOptionTest;
import jdk.test.lib.process.ExitCode;

public class TestAotWithRetransform {
    private final static String LIB_NAME = "libTestAotWithRetransform.so";
    private final static String JAR_NAME = "TestAotWithRetransform.jar";
    private final static String CLASS_NAME = "compiler/aot/HelloWorldPrinter";
    private final static String UPDATED_MESSAGE = "Hello earth";

    private final static String[] UNEXPECTED_MESSAGES = new String[] {
        HelloWorldPrinter.CLINIT_MESSAGE,
        HelloWorldPrinter.MESSAGE,
        CLASS_NAME
    };

    private final static String[] EXPECTED_MESSAGES = new String[] {
        LIB_NAME,
        UPDATED_MESSAGE
    };

    public static class Bytes {
        public final static byte[] WORLD = Bytes.asBytes("world");
        public final static byte[] EARTH = Bytes.asBytes("earth");

        public static byte[] asBytes(String string) {
            byte[] result = new byte[string.length()];
            for (int i = 0; i < string.length(); i++) {
                result[i] = (byte)string.charAt(i);
            }
            return result;
        }

        public static byte[] replaceAll(byte[] input, byte[] target, byte[] replacement) {
            List<Byte> result = new ArrayList<>();
            for (int i = 0; i < input.length; i++) {
                if (hasTarget(input, i, target)) {
                    for (int j = 0; j < replacement.length; j++) {
                        result.add(replacement[j]);
                    }
                    i += target.length - 1;
                } else {
                    result.add(input[i]);
                }
            }
            byte[] resultArray = new byte[result.size()];
            for (int i = 0; i < resultArray.length; i++) {
                resultArray[i] = result.get(i);
            }
            return resultArray;
        }

        private static boolean hasTarget(byte[] input, int start, byte[] target) {
            for (int i = 0; i < target.length; i++) {
                if (start + i == input.length) {
                    return false;
                }
                if (input[start + i] != target[i]) {
                    return false;
                }
            }
            return true;
        }
    }

    public static class TestClassFileTransformer implements ClassFileTransformer {
        public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined, ProtectionDomain protectionDomain, byte[] classfileBuffer) throws IllegalClassFormatException {
            if (CLASS_NAME.equals(className)) {
                return Bytes.replaceAll(classfileBuffer, Bytes.WORLD, Bytes.EARTH);
            }
            return classfileBuffer;
        }
    }

    // Called when agent is loaded at startup
    public static void premain(String agentArgs, Instrumentation instrumentation) throws Exception {
        // don't init HelloWorldPrinter here, just Load in advance
        HelloWorldPrinter.class.getName();
        instrumentation.addTransformer(new TestClassFileTransformer(), true);
        // retransform HelloWorldPrinter
        instrumentation.retransformClasses(HelloWorldPrinter.class);
    }

    public static void main(String[] args) throws Throwable {
        try {
            boolean addTestVMOptions = true;
            CommandLineOptionTest.verifyJVMStartup(EXPECTED_MESSAGES,
                    UNEXPECTED_MESSAGES, "Unexpected exit code",
                    "Unexpected output", ExitCode.OK, addTestVMOptions,
                    "-XX:+UnlockExperimentalVMOptions", "-XX:+UseAOT", "-XX:+PrintAOT",
                    "-XX:AOTLibrary=./" + LIB_NAME,
                    "-javaagent:./" + JAR_NAME,
                    HelloWorldPrinter.class.getName());
        } catch (Throwable t) {
            throw new Error("Problems executing test " + t, t);
        }
    }
}
