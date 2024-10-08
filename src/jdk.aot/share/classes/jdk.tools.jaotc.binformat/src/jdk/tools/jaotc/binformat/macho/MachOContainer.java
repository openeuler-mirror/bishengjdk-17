/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates. All rights reserved.
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



package jdk.tools.jaotc.binformat.macho;

import java.io.File;
import java.io.FileOutputStream;

final class MachOContainer {

    private final File outputFile;
    private FileOutputStream outputStream;
    private long fileOffset;

    MachOContainer(String fileName) {

        outputFile = new File(fileName);
        if (outputFile.exists()) {
            outputFile.delete();
        }

        try {
            outputStream = new FileOutputStream(outputFile);
        } catch (Exception e) {
            System.out.println("MachOContainer: Can't create file " + fileName);
        }
        fileOffset = 0;
    }

    void close() {
        try {
            outputStream.close();
        } catch (Exception e) {
            System.out.println("MachOContainer: close failed");
        }
    }

    void writeBytes(byte[] bytes) {
        try {
            outputStream.write(bytes);
        } catch (Exception e) {
            System.out.println("MachOContainer: writeBytes failed");
        }
        fileOffset += bytes.length;
    }

    // Write bytes to output file with up front alignment padding
    void writeBytes(byte[] bytes, int alignment) {
        try {
            // Pad to alignment
            while ((fileOffset & (alignment - 1)) != 0) {
                outputStream.write(0);
                fileOffset++;
            }
            outputStream.write(bytes);
        } catch (Exception e) {
            System.out.println("MachOContainer: writeBytes failed");
        }
        fileOffset += bytes.length;
    }
}
