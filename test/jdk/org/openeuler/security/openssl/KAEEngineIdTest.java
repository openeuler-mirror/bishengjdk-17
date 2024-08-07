/*
 * Copyright (c) 2023, Huawei Technologies Co., Ltd. All rights reserved.
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

import org.openeuler.security.openssl.KAEProvider;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/*
 * @test
 * @summary Test KAE  property kae.engine.id and kae.libcrypto.useGlobalMode
 * @modules jdk.crypto.kaeprovider/org.openeuler.security.openssl
 * @requires os.arch=="aarch64"
 * @run main/othervm -Dkae.log=true KAEEngineIdTest
 * @run main/othervm -Dkae.log=true -Dkae.engine.id=kae KAEEngineIdTest
 * @run main/othervm -Dkae.log=true -Dkae.engine.id=uadk_engine -Dkae.libcrypto.useGlobalMode=true KAEEngineIdTest
 */
public class KAEEngineIdTest {

    private static final String LOG_PATH = System.getProperty("user.dir") +
            File.separator + "kae.log";

    private static final List<File> files = new ArrayList<>();

    public static void main(String[] args) throws IOException {
        KAETestHelper.Engine engine =  KAETestHelper.getEngine();
        if (!engine.isValid()) {
            System.out.println("Skip test, engine " + engine.getEngineId() + " does not exist.");
            return;
        }

        try {
            new KAEProvider();
            test(engine);
        } finally {
            KAETestHelper.cleanUp(files);
        }
    }

    private static void test(KAETestHelper.Engine engine) throws IOException {
        File file = new File(LOG_PATH);
        if (!file.exists()) {
            throw new RuntimeException(LOG_PATH + " does not exist");
        }
        files.add(file);
        try (BufferedReader bufferedReader = new BufferedReader(new FileReader(file))) {
            String s = bufferedReader.readLine();
            if (!s.contains(engine.getEngineId() + " engine was found")) {
                throw new RuntimeException("test failed");
            }
        }
    }
}
