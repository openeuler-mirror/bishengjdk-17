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


package org.graalvm.compiler.nodes.java;

import static org.graalvm.compiler.nodeinfo.InputType.Association;
import static org.graalvm.compiler.nodeinfo.NodeCycles.CYCLES_0;
import static org.graalvm.compiler.nodeinfo.NodeSize.SIZE_0;

import org.graalvm.compiler.core.common.type.StampFactory;
import org.graalvm.compiler.graph.IterableNodeType;
import org.graalvm.compiler.graph.NodeClass;
import org.graalvm.compiler.nodeinfo.NodeInfo;
import org.graalvm.compiler.nodes.FrameState;
import org.graalvm.compiler.nodes.ValueNode;
import org.graalvm.compiler.nodes.spi.LIRLowerable;
import org.graalvm.compiler.nodes.spi.NodeLIRBuilderTool;

/**
 * This node describes one locking scope; it ties the monitor enter, monitor exit and the frame
 * states together. It is thus referenced from the {@link MonitorEnterNode}, from the
 * {@link MonitorExitNode} and from the {@link FrameState}.
 */
@NodeInfo(allowedUsageTypes = Association, cycles = CYCLES_0, size = SIZE_0)
public class MonitorIdNode extends ValueNode implements IterableNodeType, LIRLowerable {

    public static final NodeClass<MonitorIdNode> TYPE = NodeClass.create(MonitorIdNode.class);
    protected int lockDepth;
    protected boolean eliminated;

    public MonitorIdNode(int lockDepth) {
        this(TYPE, lockDepth);
    }

    protected MonitorIdNode(NodeClass<? extends MonitorIdNode> c, int lockDepth) {
        super(c, StampFactory.forVoid());
        this.lockDepth = lockDepth;
    }

    public int getLockDepth() {
        return lockDepth;
    }

    public void setLockDepth(int lockDepth) {
        this.lockDepth = lockDepth;
    }

    public boolean isEliminated() {
        return eliminated;
    }

    public void setEliminated() {
        eliminated = true;
    }

    @Override
    public void generate(NodeLIRBuilderTool generator) {
        // nothing to do
    }
}
