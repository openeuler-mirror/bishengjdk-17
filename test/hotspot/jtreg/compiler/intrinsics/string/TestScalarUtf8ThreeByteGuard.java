/*
 * Copyright (c) 2026, Huawei Technologies Co., Ltd. All rights reserved.
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
 * @summary Check the malformed UTF-8 guard before the AArch64 scalar stub's three-byte batch store
 * @requires os.arch == "aarch64" & vm.compiler2.enabled
 * @library /test/lib
 * @run driver compiler.intrinsics.string.TestScalarUtf8ThreeByteGuard
 */

package compiler.intrinsics.string;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import jdk.test.lib.process.ProcessTools;

public class TestScalarUtf8ThreeByteGuard {
    public static void main(String[] args) throws Exception {
        String output = ProcessTools.executeLimitedTestJava("-XX:+UnlockDiagnosticVMOptions",
                "-XX:+UseUTFConversionIntrinsics", "-XX:PrintAssemblyOptions=show-bytes",
                "-XX:+PrintStubCode", "-version").shouldHaveExitValue(0).getStdout();
        List<Integer> code = scalarStub(output);

        int constants = find(code, "three-byte lead mask/tag, minimum code point and surrogate prefix",
                0x0f078604, 0x0f078406, 0x0f00a507, 0x0f008769); // MOVI x 4
        int lead = find(code, "check shuffled lead bytes against 1110xxxx",
                0x4e030003, 0x0e241c6a, 0x2e261d4a); // TBL, AND, EOR
        int range = find(code, "reject overlong encodings and surrogate code points",
                0x2e6534e8, 0x2f1504a3, 0x2e698c63, // CMHI, USHR #11, CMEQ
                0x0ea81d4a, 0x0ea31d4a, 0x4e083d48); // ORR, ORR, UMOV of all four lanes
        int branch = range + 6;
        int store = find(code, "three-byte batch store", 0x0c9f7025);
        int fallback = find(code, "NO_FAST remaining length check", 0x7100217f); // CMP w11, #8
        if (!(constants < lead && lead < range && branch + 1 == store && store < fallback)) {
            throw new AssertionError("Malformed UTF-8 guard must precede the batch store");
        }

        // CBNZ x8, NO_FAST. Its signed displacement is in instruction words.
        int cbnz = code.get(branch);
        if ((cbnz & 0xff00001f) != 0xb5000008 || branch + ((cbnz << 8) >> 13) != fallback) {
            throw new AssertionError("A nonzero error mask must branch to NO_FAST");
        }
    }

    private static int find(List<Integer> code, String description, Integer... instructions) {
        int index = Collections.indexOfSubList(code, List.of(instructions));
        if (index < 0) {
            throw new AssertionError("Missing " + description);
        }
        return index;
    }

    private static List<Integer> scalarStub(String output) {
        Matcher stub = Pattern.compile("StubRoutines::scalar_convert_utf8_to_utf16 \\[[^\\r\\n]+?\\]"
                + " \\((\\d+) bytes\\)\\R(.*?)- - - \\[END\\]", Pattern.DOTALL).matcher(output);
        if (!stub.find()) {
            throw new AssertionError("Missing AArch64 scalar UTF-8 stub dump");
        }
        // With hsdis there is one instruction per line; the abstract disassembler packs several.
        Matcher words = Pattern.compile("(?m)(?:^\\h*0x\\p{XDigit}+:\\h*|\\G\\h*\\|\\h*)"
                + "(\\p{XDigit}{4}) (\\p{XDigit}{4})\\b").matcher(stub.group(2));
        List<Integer> code = new ArrayList<>();
        while (words.find()) {
            int bytes = Integer.parseUnsignedInt(words.group(1) + words.group(2), 16);
            code.add(Integer.reverseBytes(bytes));
        }
        if (code.size() * Integer.BYTES != Integer.parseInt(stub.group(1))) {
            throw new AssertionError("Incomplete instruction dump for scalar UTF-8 stub");
        }
        return code;
    }
}
