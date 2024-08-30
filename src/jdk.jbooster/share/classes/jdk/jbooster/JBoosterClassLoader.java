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

package jdk.jbooster;

import java.util.Collection;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

import static java.lang.System.Logger.Level.INFO;
import static jdk.jbooster.JBooster.LOGGER;

public class JBoosterClassLoader extends ClassLoader {
    static {
        ClassLoader.registerAsParallelCapable();
    }

    /**
     * To keep the classes from being GC'ed.
     */
    public static final Map<Integer, Collection<JBoosterClassLoader>> loaders = new ConcurrentHashMap<>();

    /**
     * This method is only invoked in Hotspot C++ side.
     */
    private static JBoosterClassLoader create(int programId,
                                              String loaderClassName, String loaderName, ClassLoader parent) {
        String name = loaderClassName + ":" + loaderName;
        JBoosterClassLoader newLoader = new JBoosterClassLoader(name, parent);
        Collection<JBoosterClassLoader> que = loaders.computeIfAbsent(programId, id -> new ConcurrentLinkedQueue<>());
        que.add(newLoader);
        return newLoader;
    }

    /**
     * This method is only invoked in Hotspot C++ side.
     */
    private static void destroy(int programId) {
        Collection<JBoosterClassLoader> que = loaders.remove(programId);
        LOGGER.log(INFO, "Class loaders and classes that belong to the program will be GCed later: "
                + "program_id={0}, loader_cnt={1}.", programId, (que == null ? -1 : que.size()));
    }

    private JBoosterClassLoader(String name, ClassLoader parent) {
        super(name, parent);
    }
}
