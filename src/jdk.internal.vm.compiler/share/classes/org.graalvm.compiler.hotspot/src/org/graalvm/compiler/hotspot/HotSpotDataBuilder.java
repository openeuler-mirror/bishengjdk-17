/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates. All rights reserved.
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


package org.graalvm.compiler.hotspot;

import static jdk.vm.ci.hotspot.HotSpotCompressedNullConstant.COMPRESSED_NULL;

import java.nio.ByteBuffer;

import org.graalvm.compiler.code.DataSection.Data;
import org.graalvm.compiler.code.DataSection.Patches;
import org.graalvm.compiler.code.DataSection.SerializableData;
import org.graalvm.compiler.code.DataSection.ZeroData;
import org.graalvm.compiler.debug.GraalError;
import org.graalvm.compiler.lir.asm.DataBuilder;

import jdk.vm.ci.code.TargetDescription;
import jdk.vm.ci.hotspot.HotSpotConstant;
import jdk.vm.ci.meta.Constant;
import jdk.vm.ci.meta.JavaConstant;
import jdk.vm.ci.meta.SerializableConstant;
import jdk.vm.ci.meta.VMConstant;

public class HotSpotDataBuilder extends DataBuilder {

    private final TargetDescription target;

    public HotSpotDataBuilder(TargetDescription target) {
        this.target = target;
    }

    @Override
    public Data createDataItem(Constant constant) {
        if (JavaConstant.isNull(constant)) {
            boolean compressed = COMPRESSED_NULL.equals(constant);
            int size = compressed ? 4 : target.wordSize;
            return ZeroData.create(size, size);
        } else if (constant instanceof VMConstant) {
            VMConstant vmConstant = (VMConstant) constant;
            if (!(constant instanceof HotSpotConstant)) {
                throw new GraalError(String.valueOf(constant));
            }

            HotSpotConstant c = (HotSpotConstant) vmConstant;
            int size = c.isCompressed() ? 4 : target.wordSize;
            return new Data(size, size) {
                @Override
                protected void emit(ByteBuffer buffer, Patches patches) {
                    int position = buffer.position();
                    if (getSize() == Integer.BYTES) {
                        buffer.putInt(0xDEADDEAD);
                    } else {
                        buffer.putLong(0xDEADDEADDEADDEADL);
                    }
                    patches.registerPatch(position, vmConstant);
                }
            };
        } else if (constant instanceof SerializableConstant) {
            SerializableConstant s = (SerializableConstant) constant;
            return new SerializableData(s);
        } else {
            throw new GraalError(String.valueOf(constant));
        }
    }
}
