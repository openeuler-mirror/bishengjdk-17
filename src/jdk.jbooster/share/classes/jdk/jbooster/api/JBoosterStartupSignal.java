/*
 * Copyright (c) 2020, 2023, Huawei Technologies Co., Ltd. All rights reserved.
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

package jdk.jbooster.api;

import jdk.internal.org.objectweb.asm.ClassReader;
import jdk.internal.org.objectweb.asm.ClassVisitor;
import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.MethodVisitor;

import java.lang.System.Logger;
import java.util.Objects;
import java.util.Optional;
import java.util.concurrent.atomic.AtomicBoolean;

import static java.lang.System.Logger.Level.ERROR;
import static jdk.internal.org.objectweb.asm.Opcodes.*;

/**
 * This class is client-side only. All the other classes in jdk.jbooster are
 * server-side only. So please keep this class independent of other classes
 * in jdk.jbooster.
 */
public class JBoosterStartupSignal {
    private static final Logger LOGGER = System.getLogger(JBoosterStartupSignal.class.getName());

    private static final AtomicBoolean invoked = new AtomicBoolean(false);

    private JBoosterStartupSignal() {}

    /**
     * Invoke this method after the program startup phase is complete.
     * @see CallbackMethodVisitor#visitInsn(int)
     */
    public static void startupCallback() {
        // can only be invoked once
        if (!invoked.compareAndSet(false, true)) {
            return;
        }

        // recheck the caller
        Optional<StackWalker.StackFrame> frameOptional = StackWalker.getInstance()
                .walk(stream -> stream.skip(1).findFirst());
        if (frameOptional.isEmpty()) {
            return;
        }
        StackWalker.StackFrame frame = frameOptional.get();

        String expectedClassName = System.getProperty("jdk.jbooster.startup_klass_name");
        String expectedMethodName = System.getProperty("jdk.jbooster.startup_method_name");
        String expectedMethodSignature = System.getProperty("jdk.jbooster.startup_method_signature");
        if (expectedClassName != null) {
            expectedClassName = expectedClassName.replace('/', '.');
        }

        if (!Objects.equals(expectedClassName, frame.getClassName())
                || !Objects.equals(expectedMethodName, frame.getMethodName())) {
            return;
        }
        if (expectedMethodSignature != null && !expectedMethodSignature.equals(frame.getDescriptor())) {
            return;
        }

        // callback logic in the new thread
        Thread thread = new Thread(() -> {
            System.loadLibrary("jbooster");
            startupNativeCallback();
        });
        thread.setName("JBooster Startup");
        thread.setDaemon(true);
        thread.setUncaughtExceptionHandler(((t, e) -> {
            LOGGER.log(ERROR, "Failed to call the startup callback.");
        }));
        thread.start();
    }

    /**
     * Append the static invocation of startupCallBack() at the end of the target method.
     * This method is invoked only in C++.
     * @param originBytecode the origin bytecode bytes of the target class
     * @param methodName the name of the target method
     * @param methodSignature the signature of the method (something like "(IIILjava/lang/Object;)Z").
     *                        PS: In Hotspot it is called "signature" but in ASM it is called "descriptor".
     * @return the modified bytecode bytes
     */
    private static byte[] injectStartupCallback(byte[] originBytecode, String methodName, String methodSignature) {
        ClassReader cr = new ClassReader(originBytecode);
        ClassWriter cw = new ClassWriter(cr, ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
        CallbackClassVisitor cv = new CallbackClassVisitor(cw, methodName, methodSignature);
        cr.accept(cv, 0);
        return cv.isInjected() ? cw.toByteArray() : null;
    }

    private static native void startupNativeCallback();

    private static final class CallbackClassVisitor extends ClassVisitor {
        private final String methodName;
        private final String methodSignature;

        private boolean injected;

        public CallbackClassVisitor(ClassWriter classVisitor, String methodName, String methodSignature) {
            super(ASM7, classVisitor);
            this.methodName = methodName;
            this.methodSignature = methodSignature;
            this.injected = false;
        }

        public boolean isInjected() {
            return injected;
        }

        @Override
        public MethodVisitor visitMethod(int access, String name, String descriptor,
                                         String signature, String[] exceptions) {
            if (!methodName.equals(name) || (methodSignature != null && !methodSignature.equals(descriptor))) {
                return super.visitMethod(access, name, descriptor, signature, exceptions);
            }
            injected = true;
            MethodVisitor mv = cv.visitMethod(access, name, descriptor, signature, exceptions);
            return new CallbackMethodVisitor(mv);
        }
    }

    private static final class CallbackMethodVisitor extends MethodVisitor {
        public CallbackMethodVisitor(MethodVisitor methodVisitor) {
            super(ASM7, methodVisitor);
        }

        /**
         * @see JBoosterStartupSignal#startupCallback()
         */
        @Override
        public void visitInsn(int opcode) {
            if ((opcode >= IRETURN && opcode <= RETURN) || opcode == ATHROW) {
                mv.visitMethodInsn(INVOKESTATIC,
                        JBoosterStartupSignal.class.getName().replace('.', '/'),
                        "startupCallback", "()V", false);
            }
            mv.visitInsn(opcode);
        }
    }
}
