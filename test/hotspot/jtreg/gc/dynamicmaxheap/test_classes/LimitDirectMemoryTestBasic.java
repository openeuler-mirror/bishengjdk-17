/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
 */

import jdk.test.lib.process.ProcessTools;
import jdk.test.lib.JDKToolFinder;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;

public class LimitDirectMemoryTestBasic {
    public static void main(String[] args) throws Exception {
        long pid = ProcessTools.getProcessId();
        String new_size = args[0];
        int alloc_size = Integer.parseInt(args[1]);

        resize(pid, new_size);

        try {
            // alloc direct memory
            int single_alloc_size = 1 * 1024 * 1024;
            ByteBuffer[] buffers = new ByteBuffer[alloc_size];
            for (int i = 0; i < alloc_size; i++) {
                buffers[i] = ByteBuffer.allocateDirect(single_alloc_size);
            }
        } catch (OutOfMemoryError e) {
            System.out.println(e);
            throw e;
        }
        System.out.println("allocation finish!");
    }

    static void resize(long pid, String new_size) {
        try {
            Process process = Runtime.getRuntime().exec(JDKToolFinder.getJDKTool("jcmd") + " " + pid + " GC.elastic_max_direct_memory " + new_size);
            BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String line;
            while ((line = reader.readLine()) != null) {
                System.out.println(line);
            }
            reader.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
