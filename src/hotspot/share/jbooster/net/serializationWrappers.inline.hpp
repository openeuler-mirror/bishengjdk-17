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

#ifndef SHARE_JBOOSTER_NET_SERIALIZATIONWRAPPERS_INLINE_HPP
#define SHARE_JBOOSTER_NET_SERIALIZATIONWRAPPERS_INLINE_HPP

#include <type_traits>

#include "jbooster/net/serializationWrappers.hpp"
#include "memory/metadataFactory.hpp"
#include "oops/array.hpp"
#include "runtime/thread.hpp"
#include "utilities/growableArray.hpp"

// -------------------------- ArrayWrapper for Serializer --------------------------

template <typename Arg>
template <typename ArrLike>
inline const void* ArrayWrapper<Arg>::arr_base(ArrLike* array) {
  if (array == nullptr) return nullptr;
  if (array->is_empty()) {
    // adr_at() requires 0 <= idx < len. So just use a non-null placeholder here.
    // We use 0x1 because if the program accesses it, it crashes.
    return (const void*) ((uintptr_t) 0x1);
  }
  return static_cast<const void*>(array->adr_at(0));
}

template <typename Arg>
template <typename ArrLike>
inline int ArrayWrapper<Arg>::arr_size(ArrLike* array) {
  if (array == nullptr) return INT_NULL_PTR;
  return array->length();
}

template <typename Arg>
inline ArrayWrapper<Arg>::ArrayWrapper(bool is_ptr, ArrayAllocater<Arg>* allocater):
        WrapperBase(SerializationMode::DESERIALIZE),
        _base(nullptr),
        _size(0),
        _is_ptr(is_ptr),
        _allocater(allocater) {}

template <typename Arg>
inline ArrayWrapper<Arg>::ArrayWrapper(const Arg* val_base, int size):
        WrapperBase(SerializationMode::SERIALIZE),
        _base(static_cast<const void*>(val_base)),
        _size(val_base == nullptr ? INT_NULL_PTR : size),
        _is_ptr(false),
        _allocater(nullptr) {}

template <typename Arg>
inline ArrayWrapper<Arg>::ArrayWrapper(const ConstArgPtr* ptr_base, int size):
        WrapperBase(SerializationMode::SERIALIZE),
        _base(static_cast<const void*>(ptr_base)),
        _size(ptr_base == nullptr ? INT_NULL_PTR : size),
        _is_ptr(true),
        _allocater(nullptr) {}

template <typename Arg>
inline ArrayWrapper<Arg>::ArrayWrapper(const Array<Arg>* array):
        WrapperBase(SerializationMode::SERIALIZE),
        _base(arr_base(array)),
        _size(arr_size(array)),
        _is_ptr(false),
        _allocater(nullptr) {}

template <typename Arg>
inline ArrayWrapper<Arg>::ArrayWrapper(const Array<Arg*>* array):
        WrapperBase(SerializationMode::SERIALIZE),
        _base(arr_base(array)),
        _size(arr_size(array)),
        _is_ptr(true),
        _allocater(nullptr) {}

template <typename Arg>
inline ArrayWrapper<Arg>::ArrayWrapper(const GrowableArray<Arg>* array):
        WrapperBase(SerializationMode::SERIALIZE),
        _base(arr_base(array)),
        _size(arr_size(array)),
        _is_ptr(false),
        _allocater(nullptr) {}

template <typename Arg>
inline ArrayWrapper<Arg>::ArrayWrapper(const GrowableArray<Arg*>* array):
        WrapperBase(SerializationMode::SERIALIZE),
        _base(arr_base(array)),
        _size(arr_size(array)),
        _is_ptr(true),
        _allocater(nullptr) {}

template <typename Arg>
inline ArrayWrapper<Arg>::~ArrayWrapper() {
  if (!_free_memory || _base == nullptr) return;
  if (!_is_ptr) {
    Arg* base = cast_to_non_const_val_base();
    for (int i = 0; i < _size; ++i) base[i].~Arg();
    FREE_C_HEAP_ARRAY(Arg, _base);
  }
  else {
    // We do not delete the value of pointers here.
    FREE_C_HEAP_ARRAY(ConstArgPtr, _base);
  }
}

template <typename Arg>
inline int ArrayWrapper<Arg>::set_sub_arr(int start, int max_size) {
  _smode.assert_can_serialize();
  _size = MIN2(max_size, MAX2(0, _size - start));
  // _base may be a illegal address when _size is small than max_size.
  // But it's ok because _size is 0 in such condition.
  if (!_is_ptr) {
    const Arg* base = static_cast<const Arg*>(_base);
    _base = base + start;
  } else {
    const ConstArgPtr* base = static_cast<const ConstArgPtr*>(_base);
    _base = base + start;
  }
  return _size;
}

template <typename Arg>
template <typename T>
inline void ArrayWrapper<Arg>::check_arr_type() {
  if (!_is_ptr) {
    assert((std::is_same<T, Arg>::value), "wrong type");
  } else {
    assert((std::is_same<T, Arg*>::value), "wrong type");
  }
}

template <typename Arg>
inline Arg* ArrayWrapper<Arg>::cast_to_non_const_val_base() {
  _smode.assert_can_deserialize();
  assert(!_is_ptr, "elements should be values");
  void* base_non_const = const_cast<void*>(_base);
  return static_cast<Arg*>(base_non_const);
}
template <typename Arg>
inline Arg** ArrayWrapper<Arg>::cast_to_non_const_ptr_base() {
  _smode.assert_can_deserialize();
  assert(_is_ptr, "elements should be pointers");
  void* base_non_const = const_cast<void*>(_base);
  return static_cast<Arg**>(base_non_const);
}

template <typename Arg>
inline void* ArrayWrapper<Arg>::alloc_val_container() {
  assert(_base == nullptr, "sanity");
  if (_allocater != nullptr) {
    return _allocater->alloc_val_container(_size);
  }
  _free_memory = true;
  void* res = (void*)NEW_C_HEAP_ARRAY(Arg, _size, mtJBooster);
  // We do not initialize the values here.
  return res;
}

template <typename Arg>
inline void* ArrayWrapper<Arg>::alloc_ptr_container() {
  assert(_base == nullptr, "sanity");
  if (_allocater != nullptr) {
    return _allocater->alloc_ptr_container(_size);
  }
  _free_memory = true;
  void* res = (void*)NEW_C_HEAP_ARRAY(Arg*, _size, mtJBooster);
  // We do not initialize the pointers here.
  return res;
}

template <typename Arg>
inline int ArrayWrapper<Arg>::serialize(MessageBuffer& buf) const {
  _smode.assert_can_serialize();
  if (_base == nullptr) {
    return buf.serialize_no_meta(INT_NULL_PTR);
  }
  JB_RETURN(buf.serialize_no_meta(_size));
  if (!_is_ptr) {
    const Arg* base = static_cast<const Arg*>(_base);
    for (int i = 0; i < _size; ++i) {
      JB_RETURN(buf.serialize_with_meta(base + i));
    }
  } else {
    const ConstArgPtr* base = static_cast<const ConstArgPtr*>(_base);
    for (int i = 0; i < _size; ++i) {
      JB_RETURN(buf.serialize_with_meta(base[i]));
    }
  }
  return 0;
}

template <typename C, typename Enable = void>
class ArrayWrapperUtils: public AllStatic {
public:
  static C* placement_new_if_possible(void* base) {
    return ::new (base) C();
  }
};

template <typename C>
class ArrayWrapperUtils<C, typename std::enable_if<std::is_abstract<C>::value || !std::is_default_constructible<C>::value>::type>: public AllStatic {
public:
  static C* placement_new_if_possible(void* base) {
    if (std::is_abstract<C>::value) {
      fatal("Unable to preallocate memory for abstract classes!");
    }
    if (!std::is_default_constructible<C>::value) {
      fatal("Unable to preallocate memory for classes with no default constructors!");
    }
    return nullptr;
  }
};

template <typename Arg>
inline int ArrayWrapper<Arg>::deserialize(MessageBuffer& buf) {
  _smode.assert_can_deserialize();
  assert(_base == nullptr, "can only deserialize once");
  JB_RETURN(buf.deserialize_ref_no_meta(_size));
  if (_size == INT_NULL_PTR) {
    _size = 0;
    return 0;
  }

  if (!_is_ptr) {
    _base = alloc_val_container();
    Arg* base = cast_to_non_const_val_base();
    for (int i = 0; i < _size; ++i) {
      // initialize each element
      ArrayWrapperUtils<Arg>::placement_new_if_possible((void*) (base + i));
      JB_RETURN(buf.deserialize_with_meta(base + i));
    }
  } else {
    _base = alloc_ptr_container();
    Arg** base = cast_to_non_const_ptr_base();
    // set all Arg* to nullptr
    memset(base, 0, _size * sizeof(Arg*));
    for (int i = 0; i < _size; ++i) {
      JB_RETURN(buf.deserialize_with_meta(base[i]));
    }
  }
  return 0;
}

template <typename Arg>
inline const Arg* ArrayWrapper<Arg>::get(int index) const {
  assert(_base != nullptr && index >= 0 && index < size(), "out-of-bounds");
  if (!_is_ptr) {
    const Arg* base = static_cast<const Arg*>(_base);
    return base + index;
  } else {
    const ConstArgPtr* base = static_cast<const ConstArgPtr*>(_base);
    return base[index];
  }
}

template <typename Arg>
template <typename T>
inline T* ArrayWrapper<Arg>::get_array() {
  check_arr_type<T>();
  return (T*)const_cast<void*>(_base);
}

template <typename Arg>
template <typename T>
inline T* ArrayWrapper<Arg>::export_array() {
  _free_memory = false;
  return get_array<T>();
}

// --------------------- Serializer register for ArrayWrapper ----------------------

template <typename Arg>
inline int SerializationImpl<ArrayWrapper<Arg>>::serialize(MessageBuffer& buf, const ArrayWrapper<Arg>& arg) {
  return arg.serialize(buf);
}

template <typename Arg>
inline int SerializationImpl<ArrayWrapper<Arg>>::deserialize_ref(MessageBuffer& buf, ArrayWrapper<Arg>& arg) {
  return arg.deserialize(buf);
}

#endif // SHARE_JBOOSTER_NET_SERIALIZATIONWRAPPERS_INLINE_HPP
