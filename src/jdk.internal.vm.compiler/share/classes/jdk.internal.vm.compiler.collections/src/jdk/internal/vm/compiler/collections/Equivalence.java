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

package jdk.internal.vm.compiler.collections;

/**
 * Strategy for comparing two objects. Default predefined strategies are {@link #DEFAULT},
 * {@link #IDENTITY}, and {@link #IDENTITY_WITH_SYSTEM_HASHCODE}.
 *
 * @since 19.0
 */
public abstract class Equivalence {

    /**
     * Default equivalence calling {@link #equals(Object)} to check equality and {@link #hashCode()}
     * for obtaining hash values. Do not change the logic of this class as it may be inlined in
     * other places.
     *
     * @since 19.0
     */
    public static final Equivalence DEFAULT = new Equivalence() {

        @Override
        public boolean equals(Object a, Object b) {
            return a.equals(b);
        }

        @Override
        public int hashCode(Object o) {
            return o.hashCode();
        }
    };

    /**
     * Identity equivalence using {@code ==} to check equality and {@link #hashCode()} for obtaining
     * hash values. Do not change the logic of this class as it may be inlined in other places.
     *
     * @since 19.0
     */
    public static final Equivalence IDENTITY = new Equivalence() {

        @Override
        public boolean equals(Object a, Object b) {
            return a == b;
        }

        @Override
        public int hashCode(Object o) {
            return o.hashCode();
        }
    };

    /**
     * Identity equivalence using {@code ==} to check equality and
     * {@link System#identityHashCode(Object)} for obtaining hash values. Do not change the logic of
     * this class as it may be inlined in other places.
     *
     * @since 19.0
     */
    public static final Equivalence IDENTITY_WITH_SYSTEM_HASHCODE = new Equivalence() {

        @Override
        public boolean equals(Object a, Object b) {
            return a == b;
        }

        @Override
        public int hashCode(Object o) {
            return System.identityHashCode(o);
        }
    };

    /**
     * Subclass for creating custom equivalence definitions.
     *
     * @since 19.0
     */
    protected Equivalence() {
    }

    /**
     * Returns {@code true} if the non-{@code null} arguments are equal to each other and
     * {@code false} otherwise.
     *
     * @since 19.0
     */
    public abstract boolean equals(Object a, Object b);

    /**
     * Returns the hash code of a non-{@code null} argument {@code o}.
     *
     * @since 19.0
     */
    public abstract int hashCode(Object o);
}
