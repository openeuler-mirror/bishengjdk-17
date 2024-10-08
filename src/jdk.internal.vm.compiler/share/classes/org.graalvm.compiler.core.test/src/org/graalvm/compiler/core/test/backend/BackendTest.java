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


package org.graalvm.compiler.core.test.backend;

import org.graalvm.compiler.core.GraalCompiler;
import org.graalvm.compiler.core.gen.LIRCompilerBackend;
import org.graalvm.compiler.core.test.GraalCompilerTest;
import org.graalvm.compiler.debug.DebugContext;
import org.graalvm.compiler.lir.gen.LIRGenerationResult;
import org.graalvm.compiler.nodes.StructuredGraph;
import org.graalvm.compiler.phases.OptimisticOptimizations;

import jdk.vm.ci.code.Architecture;

public abstract class BackendTest extends GraalCompilerTest {

    public BackendTest() {
        super();
    }

    public BackendTest(Class<? extends Architecture> arch) {
        super(arch);
    }

    @SuppressWarnings("try")
    protected LIRGenerationResult getLIRGenerationResult(final StructuredGraph graph, OptimisticOptimizations optimizations) {
        DebugContext debug = graph.getDebug();
        try (DebugContext.Scope s = debug.scope("FrontEnd")) {
            GraalCompiler.emitFrontEnd(getProviders(), getBackend(), graph, getDefaultGraphBuilderSuite(), optimizations, graph.getProfilingInfo(), createSuites(graph.getOptions()));
        } catch (Throwable e) {
            throw debug.handle(e);
        }

        LIRGenerationResult lirGen = LIRCompilerBackend.emitLIR(getBackend(), graph, null, null, createLIRSuites(graph.getOptions()));
        return lirGen;
    }

    protected LIRGenerationResult getLIRGenerationResult(final StructuredGraph graph) {
        return getLIRGenerationResult(graph, OptimisticOptimizations.NONE);
    }
}
