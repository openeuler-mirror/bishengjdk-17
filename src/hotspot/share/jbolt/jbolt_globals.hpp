/*
 * Copyright (c) 2020, 2024, Huawei Technologies Co., Ltd. All rights reserved.
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

#ifndef SHARE_JBOLT_JBOLT_GLOBALS_HPP
#define SHARE_JBOLT_JBOLT_GLOBALS_HPP

#include "runtime/globals_shared.hpp"

#define JBOLT_FLAGS(develop,                                                \
                       develop_pd,                                          \
                       product,                                             \
                       product_pd,                                          \
                       notproduct,                                          \
                       range,                                               \
                       constraint)                                          \
                                                                            \
  product(bool, UseJBolt, false, EXPERIMENTAL,                              \
          "Enable JBolt feature.")                                          \
                                                                            \
  product(bool, JBoltDumpMode, false, EXPERIMENTAL,                         \
          "Trial run of JBolt. Collect profiling and dump it.")             \
                                                                            \
  product(bool, JBoltLoadMode, false, EXPERIMENTAL,                         \
          "Second run of JBolt. Load the profiling and reorder nmethods.")  \
                                                                            \
  product(ccstr, JBoltOrderFile, NULL, EXPERIMENTAL,                        \
          "The JBolt method order file to dump or load.")                   \
                                                                            \
  product(intx, JBoltSampleInterval, 600, EXPERIMENTAL,                     \
          "Sample interval(second) of JBolt dump mode"                      \
          "only useful in auto mode.")                                      \
          range(0, max_jint)                                                \
                                                                            \
  product(uintx, JBoltCodeHeapSize, 8*M , EXPERIMENTAL,                     \
          "Code heap size of MethodJBoltHot and MethodJBoltTmp heaps.")     \
                                                                            \

// end of JBOLT_FLAGS

DECLARE_FLAGS(JBOLT_FLAGS)

#endif // SHARE_JBOLT_JBOLT_GLOBALS_HPP
