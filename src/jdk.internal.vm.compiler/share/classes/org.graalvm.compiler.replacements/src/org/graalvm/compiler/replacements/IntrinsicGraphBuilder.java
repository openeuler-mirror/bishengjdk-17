/*
 * Copyright (c) 2011, 2023, Oracle and/or its affiliates. All rights reserved.
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


package org.graalvm.compiler.replacements;

import org.graalvm.compiler.bytecode.Bytecode;
import org.graalvm.compiler.bytecode.BytecodeProvider;
import org.graalvm.compiler.core.common.spi.ConstantFieldProvider;
import org.graalvm.compiler.core.common.type.Stamp;
import org.graalvm.compiler.core.common.type.StampFactory;
import org.graalvm.compiler.core.common.type.StampPair;
import org.graalvm.compiler.core.common.type.TypeReference;
import org.graalvm.compiler.debug.DebugCloseable;
import org.graalvm.compiler.debug.DebugContext;
import org.graalvm.compiler.debug.GraalError;
import org.graalvm.compiler.graph.NodeSourcePosition;
import org.graalvm.compiler.nodes.AbstractBeginNode;
import org.graalvm.compiler.nodes.CallTargetNode;
import org.graalvm.compiler.nodes.CallTargetNode.InvokeKind;
import org.graalvm.compiler.nodes.FixedNode;
import org.graalvm.compiler.nodes.FixedWithNextNode;
import org.graalvm.compiler.nodes.FrameState;
import org.graalvm.compiler.nodes.Invoke;
import org.graalvm.compiler.nodes.ParameterNode;
import org.graalvm.compiler.nodes.ReturnNode;
import org.graalvm.compiler.nodes.StateSplit;
import org.graalvm.compiler.nodes.StructuredGraph;
import org.graalvm.compiler.nodes.StructuredGraph.AllowAssumptions;
import org.graalvm.compiler.nodes.UnwindNode;
import org.graalvm.compiler.nodes.ValueNode;
import org.graalvm.compiler.nodes.WithExceptionNode;
import org.graalvm.compiler.nodes.graphbuilderconf.GraphBuilderContext;
import org.graalvm.compiler.nodes.graphbuilderconf.IntrinsicContext;
import org.graalvm.compiler.nodes.graphbuilderconf.InvocationPlugin;
import org.graalvm.compiler.nodes.graphbuilderconf.InvocationPlugin.Receiver;
import org.graalvm.compiler.nodes.java.ExceptionObjectNode;
import org.graalvm.compiler.nodes.spi.CoreProviders;
import org.graalvm.compiler.nodes.spi.Replacements;
import org.graalvm.compiler.nodes.spi.StampProvider;
import org.graalvm.compiler.options.OptionValues;

import jdk.vm.ci.code.BailoutException;
import jdk.vm.ci.code.BytecodeFrame;
import jdk.vm.ci.meta.ConstantReflectionProvider;
import jdk.vm.ci.meta.JavaKind;
import jdk.vm.ci.meta.JavaType;
import jdk.vm.ci.meta.MetaAccessProvider;
import jdk.vm.ci.meta.ResolvedJavaMethod;
import jdk.vm.ci.meta.ResolvedJavaType;
import jdk.vm.ci.meta.Signature;

/**
 * Implementation of {@link GraphBuilderContext} used to produce a graph for a method based on an
 * {@link InvocationPlugin} for the method.
 */
public class IntrinsicGraphBuilder implements GraphBuilderContext, Receiver {

    protected final CoreProviders providers;
    protected final StructuredGraph graph;
    protected final Bytecode code;
    protected final ResolvedJavaMethod method;
    protected final int invokeBci;
    protected FixedWithNextNode lastInstr;
    protected ValueNode[] arguments;
    protected ValueNode returnValue;

    private boolean unwindCreated;

    public IntrinsicGraphBuilder(OptionValues options, DebugContext debug, CoreProviders providers, Bytecode code, int invokeBci) {
        this(options, debug, providers, code, invokeBci, AllowAssumptions.YES);
    }

    protected IntrinsicGraphBuilder(OptionValues options, DebugContext debug, CoreProviders providers, Bytecode code, int invokeBci, AllowAssumptions allowAssumptions) {
        this.providers = providers;
        this.code = code;
        this.method = code.getMethod();
        this.graph = new StructuredGraph.Builder(options, debug, allowAssumptions).method(method).setIsSubstitution(true).trackNodeSourcePosition(true).build();
        this.invokeBci = invokeBci;
        this.lastInstr = graph.start();

        Signature sig = method.getSignature();
        int max = sig.getParameterCount(false);
        this.arguments = new ValueNode[max + (method.isStatic() ? 0 : 1)];

        int javaIndex = 0;
        int index = 0;
        if (!method.isStatic()) {
            // add the receiver
            Stamp receiverStamp = StampFactory.objectNonNull(TypeReference.createWithoutAssumptions(method.getDeclaringClass()));
            ValueNode receiver = graph.addWithoutUnique(new ParameterNode(javaIndex, StampPair.createSingle(receiverStamp)));
            arguments[index] = receiver;
            javaIndex = 1;
            index = 1;
        }
        ResolvedJavaType accessingClass = method.getDeclaringClass();
        for (int i = 0; i < max; i++) {
            JavaType type = sig.getParameterType(i, accessingClass).resolve(accessingClass);
            JavaKind kind = type.getJavaKind();
            Stamp stamp;
            if (kind == JavaKind.Object && type instanceof ResolvedJavaType) {
                stamp = StampFactory.object(TypeReference.createWithoutAssumptions((ResolvedJavaType) type));
            } else {
                stamp = StampFactory.forKind(kind);
            }
            ValueNode param = graph.addWithoutUnique(new ParameterNode(index, StampPair.createSingle(stamp)));
            arguments[index] = param;
            javaIndex += kind.getSlotCount();
            index++;
        }
    }

    private <T extends ValueNode> void updateLastInstruction(T v) {
        if (v instanceof FixedNode) {
            FixedNode fixedNode = (FixedNode) v;
            if (lastInstr != null) {
                lastInstr.setNext(fixedNode);
            }
            if (fixedNode instanceof FixedWithNextNode) {
                FixedWithNextNode fixedWithNextNode = (FixedWithNextNode) fixedNode;
                assert fixedWithNextNode.next() == null : "cannot append instruction to instruction which isn't end";
                lastInstr = fixedWithNextNode;

            } else if (fixedNode instanceof WithExceptionNode) {
                WithExceptionNode withExceptionNode = (WithExceptionNode) fixedNode;
                AbstractBeginNode normalSuccessor = graph.add(withExceptionNode.createNextBegin());
                ExceptionObjectNode exceptionSuccessor = graph.add(new ExceptionObjectNode(getMetaAccess()));
                setExceptionState(exceptionSuccessor);
                exceptionSuccessor.setNext(graph.add(new UnwindNode(exceptionSuccessor)));

                if (unwindCreated) {
                    throw GraalError.shouldNotReachHere("Intrinsic graph can only have one node with an exception edge");
                }
                unwindCreated = true;

                withExceptionNode.setNext(normalSuccessor);
                withExceptionNode.setExceptionEdge(exceptionSuccessor);
                lastInstr = normalSuccessor;

            } else {
                lastInstr = null;
            }
        }
    }

    /**
     * Currently unimplemented here, but implemented in subclasses that need it.
     *
     * @param exceptionObject The node that needs an exception state.
     */
    protected void setExceptionState(ExceptionObjectNode exceptionObject) {
        throw GraalError.shouldNotReachHere("unsupported by this IntrinsicGraphBuilder");
    }

    @Override
    public <T extends ValueNode> T append(T v) {
        if (v.graph() != null) {
            return v;
        }
        T added = graph.addOrUniqueWithInputs(v);
        if (added == v) {
            updateLastInstruction(v);
        }
        return added;
    }

    @Override
    public void push(JavaKind kind, ValueNode value) {
        assert kind != JavaKind.Void;
        GraalError.guarantee(returnValue == null, "can only push one value");
        returnValue = value;
    }

    @Override
    public ValueNode pop(JavaKind slotKind) {
        GraalError.guarantee(returnValue != null, "no value pushed");
        ValueNode result = returnValue;
        returnValue = null;
        return result;
    }

    @Override
    public Invoke handleReplacedInvoke(InvokeKind invokeKind, ResolvedJavaMethod targetMethod, ValueNode[] args, boolean forceInlineEverything) {
        throw GraalError.shouldNotReachHere();
    }

    @Override
    public void handleReplacedInvoke(CallTargetNode callTarget, JavaKind resultType) {
        throw GraalError.shouldNotReachHere();
    }

    @Override
    public StampProvider getStampProvider() {
        return providers.getStampProvider();
    }

    @Override
    public MetaAccessProvider getMetaAccess() {
        return providers.getMetaAccess();
    }

    @Override
    public ConstantReflectionProvider getConstantReflection() {
        return providers.getConstantReflection();
    }

    @Override
    public ConstantFieldProvider getConstantFieldProvider() {
        return providers.getConstantFieldProvider();
    }

    @Override
    public Replacements getReplacements() {
        return providers.getReplacements();
    }

    @Override
    public StructuredGraph getGraph() {
        return graph;
    }

    @Override
    public void setStateAfter(StateSplit sideEffect) {
        assert sideEffect.hasSideEffect();
        FrameState stateAfter = getGraph().add(new FrameState(BytecodeFrame.BEFORE_BCI));
        sideEffect.setStateAfter(stateAfter);
    }

    @Override
    public GraphBuilderContext getParent() {
        return null;
    }

    @Override
    public Bytecode getCode() {
        return code;
    }

    @Override
    public ResolvedJavaMethod getMethod() {
        return method;
    }

    @Override
    public int bci() {
        return invokeBci;
    }

    @Override
    public InvokeKind getInvokeKind() {
        return method.isStatic() ? InvokeKind.Static : InvokeKind.Virtual;
    }

    @Override
    public JavaType getInvokeReturnType() {
        return method.getSignature().getReturnType(method.getDeclaringClass());
    }

    @Override
    public int getDepth() {
        return 0;
    }

    @Override
    public boolean parsingIntrinsic() {
        return true;
    }

    @Override
    public IntrinsicContext getIntrinsic() {
        throw GraalError.shouldNotReachHere();
    }

    @Override
    public BailoutException bailout(String string) {
        throw GraalError.shouldNotReachHere();
    }

    @Override
    public ValueNode get(boolean performNullCheck) {
        return arguments[0];
    }

    @SuppressWarnings("try")
    public StructuredGraph buildGraph(InvocationPlugin plugin) {
        NodeSourcePosition position = graph.trackNodeSourcePosition() ? NodeSourcePosition.placeholder(method) : null;
        try (DebugCloseable context = graph.withNodeSourcePosition(position)) {
            Receiver receiver = method.isStatic() ? null : this;
            if (plugin.execute(this, method, receiver, arguments)) {
                assert (returnValue != null) == (method.getSignature().getReturnKind() != JavaKind.Void) : method;
                assert lastInstr != null : "ReturnNode must be linked into control flow";
                append(new ReturnNode(returnValue));
                return graph;
            }
            return null;
        }
    }

    @Override
    public boolean intrinsify(BytecodeProvider bytecodeProvider, ResolvedJavaMethod targetMethod, ResolvedJavaMethod substitute, InvocationPlugin.Receiver receiver, ValueNode[] args) {
        throw GraalError.shouldNotReachHere();
    }

    @Override
    public boolean intrinsify(ResolvedJavaMethod targetMethod, StructuredGraph substituteGraph, Receiver receiver, ValueNode[] argsIncludingReceiver) {
        throw GraalError.shouldNotReachHere();
    }

    @Override
    public String toString() {
        return String.format("%s:intrinsic", method.format("%H.%n(%p)"));
    }
}
