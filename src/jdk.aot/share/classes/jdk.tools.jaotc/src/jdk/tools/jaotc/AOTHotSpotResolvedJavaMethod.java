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



package jdk.tools.jaotc;

import org.graalvm.compiler.code.CompilationResult;
import org.graalvm.compiler.core.target.Backend;
import org.graalvm.compiler.hotspot.HotSpotCompiledCodeBuilder;
import org.graalvm.compiler.options.OptionValues;

import jdk.vm.ci.hotspot.HotSpotCompiledCode;
import jdk.vm.ci.hotspot.HotSpotResolvedJavaMethod;

final class AOTHotSpotResolvedJavaMethod implements JavaMethodInfo {

    private final HotSpotResolvedJavaMethod method;
    private final Backend backend;
    private final OptionValues options;

    AOTHotSpotResolvedJavaMethod(HotSpotResolvedJavaMethod method, Backend backend, OptionValues options) {
        this.method = method;
        this.backend = backend;
        this.options = options;
    }

    @Override
    public String getSymbolName() {
        return JavaMethodInfo.uniqueMethodName(method);
    }

    @Override
    public String getNameAndSignature() {
        String className = method.getDeclaringClass().getName();
        return className + "." + method.getName() + method.getSignature().toMethodDescriptor();
    }

    @Override
    public HotSpotCompiledCode compiledCode(CompilationResult result) {
        return HotSpotCompiledCodeBuilder.createCompiledCode(backend.getCodeCache(), method, null, result, options);
    }

}
