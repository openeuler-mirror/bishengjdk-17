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

#ifndef SHARE_JBOLT_JBOLTUTILS_INLINE_HPP
#define SHARE_JBOLT_JBOLTUTILS_INLINE_HPP

#include "jbolt/jBoltUtils.hpp"

// Register a metadata as 'in-use' by the thread. It's fine to register a
// metadata multiple times (though perhaps inefficient).
inline void JBoltUtils::MetaDataKeepAliveMark::add(Metadata* md) {
  assert(md->is_valid(), "obj is valid");
  assert(_thread == Thread::current(), "thread must be current");
  _kept.push(md);
  _thread->metadata_handles()->push(md);
}

#endif // SHARE_JBOLT_JBOLTUTILS_INLINE_HPP
