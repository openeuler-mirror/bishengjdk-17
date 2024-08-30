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

// ---------------------------------------------------------------------------------
// This file implements serialization for the C++ classes in Hotspot.
// This file has nothing to do with the serialization of Java classes.
// ---------------------------------------------------------------------------------

#ifndef SHARE_JBOOSTER_NET_SERIALIZATION_HPP
#define SHARE_JBOOSTER_NET_SERIALIZATION_HPP

#include <string.h>

#include "jbooster/net/messageBuffer.hpp"
#include "jbooster/net/netCommon.hpp"
#include "jbooster/utilities/debugUtils.inline.hpp"
#include "memory/allocation.hpp"
#include "memory/resourceArea.hpp"
#include "utilities/globalDefinitions.hpp"

// ----------------------------- Serializer Implements -----------------------------
//  template <typename Arg>
//  struct CppClassSerializerImpl {
//    // Serialize the argument (invoked only if the arg is not null).
//    static int serialize(MessageBuffer& buf, const Arg& arg);
//
//    // There are two deserialization modes. You don't need to implement both.
//    // * Implement the first one if the memory space of the arg has been
//    //   pre-allocated.
//    // * Implement the second one if the memory space of the arg should be
//    //   allocated inside deserialization.
//    static int deserialize_ref(MessageBuffer& buf, Arg& arg);
//    static int deserialize_ptr(MessageBuffer& buf, Arg*& arg_ptr);
//  };

// ------------------------ Unsupported Deserializer Macro -------------------------

#define CANNOT_DESERIALIZE_REFERENCE(Arg)                         \
  static int deserialize_ref(MessageBuffer& buf, Arg& arg) {      \
    fatal("Reference deserialization is not supported: %s,",      \
          DebugUtils::type_name<Arg>());                          \
    return 0;                                                     \
  }

#define CANNOT_DESERIALIZE_POINTER(Arg)                           \
  static int deserialize_ptr(MessageBuffer& buf, Arg*& arg_ptr) { \
    fatal("Pointer deserialization is not supported: %s,",        \
          DebugUtils::type_name<Arg>());                          \
    return 0;                                                     \
  }

// ------------------------------ Default Serializer -------------------------------
// The types it support for serialization/deserialization:
// - Base types: bool, int, long long, size_t, uint64_t, and so on.
// - POD classes without pointers.
//
// memcpy() is good enough in most cases. Even for base types such as char and size_t,
// memcpy() has the similar performance as assignment (`=`) according to our tests.
// It's also a choice to use assignment here. But if a class overloads the operator `=`
// and allocates something on the heap, it can cause a memory leak.

template <typename Arg>
struct SerializationImpl {
  static int serialize(MessageBuffer& buf, const Arg& arg) {
    return buf.serialize_memcpy(&arg, sizeof(Arg));
  }

  static int deserialize_ref(MessageBuffer& buf, Arg& arg) {
    return buf.deserialize_memcpy(&arg, sizeof(Arg));
  }

  CANNOT_DESERIALIZE_POINTER(Arg);
};

// ----------------------- Intrusive Serializer Declaration ------------------------

#define DECLARE_SERIALIZER_INTRUSIVE(Arg)                         \
template <> struct SerializationImpl<Arg> {                       \
  static int serialize(MessageBuffer& buf, const Arg& arg) {      \
    return arg.serialize(buf);                                    \
  }                                                               \
  static int deserialize_ref(MessageBuffer& buf, Arg& arg) {      \
    return arg.deserialize(buf);                                  \
  }                                                               \
  CANNOT_DESERIALIZE_POINTER(Arg);                                \
};

// --------------------- Non-Intrusive Serializer Declaration ----------------------

#define DECLARE_SERIALIZER_ONLY_REFERENCE(Arg)                    \
class Arg;                                                        \
template <> struct SerializationImpl<Arg> {                       \
  static int serialize(MessageBuffer& buf, const Arg& arg);       \
  static int deserialize_ref(MessageBuffer& buf, Arg& arg);       \
  CANNOT_DESERIALIZE_POINTER(Arg);                                \
};

#define DECLARE_SERIALIZER_ONLY_POINTER(Arg)                      \
class Arg;                                                        \
template <> struct SerializationImpl<Arg> {                       \
  static int serialize(MessageBuffer& buf, const Arg& arg);       \
  static int deserialize_ptr(MessageBuffer& buf, Arg*& arg_ptr);  \
  CANNOT_DESERIALIZE_REFERENCE(Arg);                              \
};

// Declare the classes that do not belong to JBooster below.
DECLARE_SERIALIZER_ONLY_POINTER(Symbol);
DECLARE_SERIALIZER_ONLY_POINTER(InstanceKlass);
DECLARE_SERIALIZER_ONLY_POINTER(ArrayKlass);

// ------------------ Serializer of C-style String (i.e., char*) -------------------
// Threre are 4 specializations of the template:
// - const char [arr_size] : serialize() only
// - char [arr_size]       : both serialize() and deserialize()
// - const char *          : both serialize() and deserialize()
// - char *                : both serialize() and deserialize()
// They and StringWrapper are compatible with each other.
//
// **ATTENTION** Type `address` will be identified as `char*`.
//               So if you want to send an `address`, cast it to an `uintptr_t`.

template <> struct SerializationImpl<const char*> {
  using Arg = const char*;
  static int serialize(MessageBuffer& buf, const Arg& arg);
  static int deserialize_ref(MessageBuffer& buf, Arg& arg);
  CANNOT_DESERIALIZE_POINTER(Arg);
};

template <> struct SerializationImpl<char*> {
  using Arg = char*;
  static int serialize(MessageBuffer& buf, const Arg& arg);
  static int deserialize_ref(MessageBuffer& buf, Arg& arg);
  CANNOT_DESERIALIZE_POINTER(Arg);
};

template <size_t arr_size> struct SerializationImpl<const char[arr_size]> {
  using Arg = const char[arr_size];
  static int serialize(MessageBuffer& buf, const Arg& arg) {
    return buf.serialize_no_meta<const char*>(arg);
  }

  // Should not implement deserialize() here as the arg cannot be const.
};

template <size_t arr_size> struct SerializationImpl<char[arr_size]> {
  using Arg = char[arr_size];
  static int serialize(MessageBuffer& buf, const Arg& arg) {
    return buf.serialize_no_meta<const char*>(arg);
  }

  static int deserialize_ref(MessageBuffer& buf, Arg& arg) {
    uint32_t str_len;
    JB_RETURN(buf.deserialize_ref_no_meta(str_len));
    guarantee(arr_size >= (size_t) (str_len + 1),
              "array index out of bounds: arr_size=" SIZE_FORMAT ", str_len=%u",
              arr_size, str_len + 1);
    guarantee(str_len != MessageConst::NULL_PTR, "cannot set array to null");
    char* p = arg;
    JB_RETURN(buf.deserialize_memcpy(p, str_len));
    p[str_len] = '\0';
    return 0;
  }

  CANNOT_DESERIALIZE_POINTER(Arg);
};

#endif // SHARE_JBOOSTER_NET_SERIALIZATION_HPP
