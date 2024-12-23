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

#ifndef SHARE_JBOOSTER_JCLIENTVMFLAGS_HPP
#define SHARE_JBOOSTER_JCLIENTVMFLAGS_HPP

#include "jbooster/jBoosterManager.hpp"
#include "jbooster/net/serialization.hpp"
#include "runtime/globals_extension.hpp"

#define JCLIENT_CDS_VM_FLAGS(f)               \
  f(size_t,   MaxMetaspaceSize              ) \
  f(size_t,   SharedBaseAddress             ) \
  f(bool,     UseCompressedOops             ) \
  f(bool,     UseCompressedClassPointers    ) \
  f(intx,     ObjectAlignmentInBytes        ) \

#define JCLIENT_AOT_VM_FLAGS(f)               \
  f(intx,     CodeEntryAlignment            ) \
  f(bool,     EnableContended               ) \
  f(bool,     RestrictContended             ) \
  f(intx,     ContendedPaddingWidth         ) \
  f(bool,     VerifyOops                    ) \
  f(bool,     DontCompileHugeMethods        ) \
  f(intx,     HugeMethodLimit               ) \
  f(bool,     Inline                        ) \
  f(bool,     ForceUnreachable              ) \
  f(bool,     FoldStableValues              ) \
  f(intx,     MaxVectorSize                 ) \
  f(bool,     UseTLAB                       ) \
  f(bool,     UseG1GC                       ) \
  f(bool,     BytecodeVerificationLocal     ) \
  f(bool,     BytecodeVerificationRemote    ) \

#define JCLIENT_VM_FLAGS(f) \
  JCLIENT_CDS_VM_FLAGS(f)   \
  JCLIENT_AOT_VM_FLAGS(f)   \

class JClientVMFlags final: public CHeapObj<mtJBooster> {
  enum InitState {
    NOT_INITIALIZED_FOR_SEREVR,
    INITIALIZED_FOR_SEREVR,
    INITIALIZED_FOR_CLIENT
  };

  InitState _state;

#define DEF_VARIABLES(type, flag)  type v_##flag;
  JCLIENT_VM_FLAGS(DEF_VARIABLES)
#undef DEF_VARIABLES

public:
  JClientVMFlags(bool is_client);
  ~JClientVMFlags();

#define DEF_GETTERS(type, flag)  type get_##flag() const { return v_##flag; }
  JCLIENT_VM_FLAGS(DEF_GETTERS)
#undef DEF_GETTERS

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  bool equals(JClientVMFlags* that);
  uint32_t hash();

  void print_flags(outputStream* st);
};

DECLARE_SERIALIZER_INTRUSIVE(JClientVMFlags);

#endif // SHARE_JBOOSTER_JCLIENTVMFLAGS_HPP
