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

#include <jni.h>
#include <jni_util.h>
#include <jvm.h>
#include "jdk_jbooster_JBooster.h"

JNIEXPORT void JNICALL
Java_jdk_jbooster_JBooster_initInVM(JNIEnv * env, jclass unused, jint server_port, jint connection_timeout, jint cleanup_timeout, jstring cache_path)
{
  JVM_JBoosterInitVM(env, server_port, connection_timeout, cleanup_timeout, cache_path);
}

JNIEXPORT void JNICALL
Java_jdk_jbooster_JBooster_handleConnection(JNIEnv * env, jclass unused, jint connection_fd)
{
  JVM_JBoosterHandleConnection(env, connection_fd);
}

JNIEXPORT void JNICALL
Java_jdk_jbooster_JBooster_printStoredClientData(JNIEnv * env, jclass unused, jboolean print_all)
{
  JVM_JBoosterPrintStoredClientData(env, print_all);
}

JNIEXPORT long JNICALL
Java_jdk_jbooster_JBoosterCompilationContextImpl_getMetaspaceMethodData(JNIEnv * env, jclass unused, jint session_id, jlong metaspace_method)
{
  return JVM_JBoosterGetMetaspaceMethodData(env, session_id, metaspace_method);
}
