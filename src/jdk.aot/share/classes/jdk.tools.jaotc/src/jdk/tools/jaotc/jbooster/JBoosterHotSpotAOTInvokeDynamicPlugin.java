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

package jdk.tools.jaotc.jbooster;

import jdk.vm.ci.hotspot.HotSpotResolvedJavaMethod;
import org.graalvm.compiler.bytecode.Bytecodes;
import org.graalvm.compiler.hotspot.meta.HotSpotInvokeDynamicPlugin;
import org.graalvm.compiler.nodes.graphbuilderconf.GraphBuilderContext;

/**
 * JBoosterHotSpotAOTInvokeDynamicPlugin
 */
public class JBoosterHotSpotAOTInvokeDynamicPlugin extends HotSpotInvokeDynamicPlugin {
    public JBoosterHotSpotAOTInvokeDynamicPlugin(DynamicTypeStore dynoStore) {
        super(dynoStore);
    }

    @Override
    public boolean supportsDynamicInvoke(GraphBuilderContext builder, int index, int opcode) {
        // Support ConstantCallSite of invokedynamic only.
        if (opcode != Bytecodes.INVOKEDYNAMIC) return false;
        HotSpotResolvedJavaMethod target = (HotSpotResolvedJavaMethod) builder.getCode().getConstantPool().lookupMethod(index, opcode);
        return "linkToTargetMethod".equals(target.getName());
    }
}
