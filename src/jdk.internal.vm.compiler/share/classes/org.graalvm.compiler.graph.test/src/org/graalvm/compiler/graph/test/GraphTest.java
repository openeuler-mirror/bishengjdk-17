/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates. All rights reserved.
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


package org.graalvm.compiler.graph.test;

import org.graalvm.compiler.api.test.Graal;
import org.graalvm.compiler.debug.DebugContext;
import org.graalvm.compiler.debug.DebugContext.Builder;
import org.graalvm.compiler.options.OptionValues;
import org.junit.After;

public abstract class GraphTest {

    static OptionValues getOptions() {
        return Graal.getRequiredCapability(OptionValues.class);
    }

    private final ThreadLocal<DebugContext> cachedDebug = new ThreadLocal<>();

    protected DebugContext getDebug(OptionValues options) {
        DebugContext cached = cachedDebug.get();
        if (cached != null) {
            if (cached.getOptions() == options) {
                return cached;
            }
            throw new AssertionError("At most one " + DebugContext.class.getName() + " object should be created per test");
        }
        DebugContext debug = new Builder(options).build();
        cachedDebug.set(debug);
        return debug;
    }

    protected DebugContext getDebug() {
        return getDebug(getOptions());
    }

    @After
    public void afterTest() {
        DebugContext cached = cachedDebug.get();
        if (cached != null) {
            cached.closeDumpHandlers(true);
        }
    }
}
