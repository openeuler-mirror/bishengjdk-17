/*
 * Copyright (c) 2012, 2023, Oracle and/or its affiliates. All rights reserved.
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

package jdk.internal.vm.compiler.word;

/**
 * The root of the interface hierarchy for machine-word-sized values.
 *
 * @since 19.0
 */
public interface WordBase {

    /**
     * Conversion to a Java primitive value.
     *
     * @since 19.0
     */
    long rawValue();

    /**
     * This is deprecated because of the easy to mistype name collision between {@link #equals} and
     * the other word based equality routines. In general you should never be statically calling
     * this method anyway.
     *
     * @since 19.0
     */
    @Override
    @Deprecated
    boolean equals(Object o);
}
