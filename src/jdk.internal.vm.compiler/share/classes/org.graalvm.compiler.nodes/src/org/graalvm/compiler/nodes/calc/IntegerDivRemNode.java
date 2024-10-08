/*
 * Copyright (c) 2016, 2023, Oracle and/or its affiliates. All rights reserved.
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


package org.graalvm.compiler.nodes.calc;

import static org.graalvm.compiler.nodeinfo.NodeCycles.CYCLES_32;
import static org.graalvm.compiler.nodeinfo.NodeSize.SIZE_1;

import org.graalvm.compiler.core.common.type.IntegerStamp;
import org.graalvm.compiler.core.common.type.Stamp;
import org.graalvm.compiler.graph.IterableNodeType;
import org.graalvm.compiler.graph.NodeClass;
import org.graalvm.compiler.nodeinfo.InputType;
import org.graalvm.compiler.nodeinfo.NodeInfo;
import org.graalvm.compiler.nodes.NodeView;
import org.graalvm.compiler.nodes.ValueNode;
import org.graalvm.compiler.nodes.extended.GuardingNode;
import org.graalvm.compiler.nodes.spi.Lowerable;

@NodeInfo(cycles = CYCLES_32, size = SIZE_1)
public abstract class IntegerDivRemNode extends FixedBinaryNode implements Lowerable, IterableNodeType {

    public static final NodeClass<IntegerDivRemNode> TYPE = NodeClass.create(IntegerDivRemNode.class);

    public enum Op {
        DIV,
        REM
    }

    public enum Type {
        SIGNED,
        UNSIGNED
    }

    @OptionalInput(InputType.Guard) private GuardingNode zeroCheck;

    private final Op op;
    private final Type type;
    private final boolean canDeopt;

    protected IntegerDivRemNode(NodeClass<? extends IntegerDivRemNode> c, Stamp stamp, Op op, Type type, ValueNode x, ValueNode y, GuardingNode zeroCheck) {
        super(c, stamp, x, y);
        this.zeroCheck = zeroCheck;
        this.op = op;
        this.type = type;

        // Assigning canDeopt during constructor, because it must never change during lifetime of
        // the node.
        IntegerStamp yStamp = (IntegerStamp) getY().stamp(NodeView.DEFAULT);
        this.canDeopt = (yStamp.contains(0) && zeroCheck == null) || yStamp.contains(-1);
    }

    public final GuardingNode getZeroCheck() {
        return zeroCheck;
    }

    public final Op getOp() {
        return op;
    }

    public final Type getType() {
        return type;
    }

    @Override
    public boolean canDeoptimize() {
        return canDeopt;
    }
}
