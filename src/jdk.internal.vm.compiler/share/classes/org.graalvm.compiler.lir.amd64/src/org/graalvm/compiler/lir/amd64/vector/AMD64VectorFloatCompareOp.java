/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates. All rights reserved.
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


package org.graalvm.compiler.lir.amd64.vector;

import static jdk.vm.ci.code.ValueUtil.asRegister;
import static jdk.vm.ci.code.ValueUtil.isRegister;
import static org.graalvm.compiler.lir.LIRInstruction.OperandFlag.REG;
import static org.graalvm.compiler.lir.LIRInstruction.OperandFlag.STACK;

import org.graalvm.compiler.asm.amd64.AMD64Address;
import org.graalvm.compiler.asm.amd64.AMD64MacroAssembler;
import org.graalvm.compiler.asm.amd64.AMD64Assembler.VexFloatCompareOp;
import org.graalvm.compiler.asm.amd64.AVXKind.AVXSize;
import org.graalvm.compiler.lir.LIRInstructionClass;
import org.graalvm.compiler.lir.Opcode;
import org.graalvm.compiler.lir.amd64.AMD64LIRInstruction;
import org.graalvm.compiler.lir.asm.CompilationResultBuilder;

import jdk.vm.ci.meta.AllocatableValue;

public class AMD64VectorFloatCompareOp extends AMD64LIRInstruction {
    public static final LIRInstructionClass<AMD64VectorFloatCompareOp> TYPE = LIRInstructionClass.create(AMD64VectorFloatCompareOp.class);

    @Opcode private final VexFloatCompareOp opcode;
    private final AVXSize size;
    @Def({REG}) protected AllocatableValue result;
    @Use({REG}) protected AllocatableValue x;
    @Use({REG, STACK}) protected AllocatableValue y;
    private final VexFloatCompareOp.Predicate predicate;

    public AMD64VectorFloatCompareOp(VexFloatCompareOp opcode, AVXSize size, AllocatableValue result, AllocatableValue x, AllocatableValue y, VexFloatCompareOp.Predicate predicate) {
        super(TYPE);
        this.opcode = opcode;
        this.size = size;
        this.result = result;
        this.x = x;
        this.y = y;
        this.predicate = predicate;
    }

    @Override
    public void emitCode(CompilationResultBuilder crb, AMD64MacroAssembler masm) {
        if (isRegister(y)) {
            opcode.emit(masm, size, asRegister(result), asRegister(x), asRegister(y), predicate);
        } else {
            opcode.emit(masm, size, asRegister(result), asRegister(x), (AMD64Address) crb.asAddress(y), predicate);
        }
    }

}
