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

#include "classfile/vmSymbols.hpp"
#include "jbooster/dataTransmissionUtils.hpp"
#include "jbooster/jBoosterManager.hpp"
#include "jbooster/jClientArguments.hpp"
#include "jbooster/jClientVMFlags.hpp"
#include "jbooster/net/clientStream.hpp"
#include "jbooster/net/communicationStream.hpp"
#include "jbooster/net/errorCode.hpp"
#include "jbooster/net/message.hpp"
#include "jbooster/net/messageBuffer.hpp"
#include "jbooster/net/rpcCompatibility.hpp"
#include "jbooster/net/serializationWrappers.hpp"
#include "jbooster/net/serverStream.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/method.hpp"
#include "oops/methodData.hpp"

static constexpr uint32_t calc_new_hash(uint32_t old_hash) {
  return old_hash;
}

template <typename... Rest>
static constexpr uint32_t calc_new_hash(uint32_t old_hash, uint32_t ele_hash, Rest... rest_hashes) {
  uint32_t new_hash = old_hash * 31u + ele_hash;
  return calc_new_hash(new_hash, rest_hashes...);
}

template <typename T>
static constexpr uint32_t calc_class_hash() {
  uint32_t hash = (uint32_t) sizeof(T);
  return hash ^ (hash >> 3);
}

template <typename Last>
static constexpr uint32_t calc_classes_hash() {
  return calc_class_hash<Last>();
}

template <typename First, typename Second, typename... Rest>
static constexpr uint32_t calc_classes_hash() {
  return calc_new_hash(calc_classes_hash<Second, Rest...>(), calc_class_hash<First>());
}

static constexpr uint32_t classes_hash() {
  return calc_classes_hash<
      JBoosterManager, JClientVMFlags, JClientArguments,
      JBErr, Message, MessageBuffer,
      CommunicationStream, ClientStream, ServerStream,
      ArrayWrapper<int>, MemoryWrapper, StringWrapper, FileWrapper,
      ClassLoaderKey, ClassLoaderChain, ClassLoaderLocator, KlassLocator, MethodLocator, ProfileDataCollector,
      InstanceKlass, Method, MethodData
  >();
}

static constexpr uint32_t little_or_big_endian() {
  return (uint32_t) LITTLE_ENDIAN_ONLY('L') BIG_ENDIAN_ONLY('B');
}

/**
 * Returns a magic number computed at compile time based on the sizes of some classes.
 * It is just an crude way to check compatibility for now. More policies can be added later.
 */
static constexpr uint32_t calc_magic_num() {
  return calc_new_hash(
    classes_hash(),
    little_or_big_endian(),
    static_cast<uint32_t>(vmSymbolID::SID_LIMIT)
  );
}

static constexpr uint32_t _magic_num = calc_magic_num();

uint32_t RpcCompatibility::magic_num() {
  return _magic_num;
}
