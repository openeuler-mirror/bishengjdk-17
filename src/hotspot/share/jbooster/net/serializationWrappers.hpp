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

#ifndef SHARE_JBOOSTER_NET_SERIALIZATIONWRAPPERS_HPP
#define SHARE_JBOOSTER_NET_SERIALIZATIONWRAPPERS_HPP

#include "jbooster/net/serialization.hpp"

template <typename T> class Array;
class ClassLoaderData;
class CommunicationStream;
template <typename T> class GrowableArray;
class InstanceKlass;
class Method;

// Here are some wrappers defined for serialization and deserialization.

class WrapperBase: public StackObj {
public:
  // signed-int version of unsigned-int MessageConst::NULL_PTR
  enum: int {
    INT_NULL_PTR = static_cast<int>(MessageConst::NULL_PTR)
  };

protected:
  SerializationMode _smode;

  // should free the memory for deserialization at ~WrapperBase()
  bool _free_memory;

  WrapperBase(SerializationMode smode): _smode(smode), _free_memory(false) {}

public:
  // The APIs that must be implemented by each wrapper:
  // int serialize(MessageBuffer& buf) const;
  // int deserialize(MessageBuffer& buf);
  //
  // And don't forget to register the wrapper to SerializationImpl:
  // DECLARE_SERIALIZER_INTRUSIVE(TheWrapper);
};

/**
 * ArrayWrapper supports serialization/deserialization of both pointers or values,
 * that is, it can (de)serialize Arg* and Arg**.
 *
 * Layout of the serialized array:
 * |  array meta   |                  ...... elements ......                   |
 * | element count | arg0 meta | arg0 payload | arg1 meta | arg1 payload | ... |
 */
template <typename Arg>
class ArrayWrapper final: public WrapperBase {
public:
  using ConstArgPtr = const Arg*;

  template <typename Type>
  struct ArrayAllocater {
    virtual void* alloc_val_container(int size) { ShouldNotReachHere(); return nullptr; }
    virtual void* alloc_ptr_container(int size) { ShouldNotReachHere(); return nullptr; }
  };

private:
  const void* _base;  // start of the list
  int _size;          // count of elements
  bool _is_ptr;       // whether the elements are pointers or values

  // Used to allocate third-party arrays such as GrowableArray and Array.
  // This API is not implemented yet.
  ArrayAllocater<Arg>* const _allocater;

  template <typename T>
  void check_arr_type();

  Arg* cast_to_non_const_val_base();  // for deserialization only
  Arg** cast_to_non_const_ptr_base(); // for deserialization only

  void* alloc_val_container();
  void* alloc_ptr_container();

private:
  template <typename ArrLike>
  static const void* arr_base(ArrLike* array);

  template <typename ArrLike>
  static int arr_size(ArrLike* array);

public:
  ArrayWrapper(bool is_ptr, ArrayAllocater<Arg>* allocater = nullptr); // for deserialization only

  ArrayWrapper(const Arg* val_base, int size);          // for serialization only
  ArrayWrapper(const ConstArgPtr* ptr_base, int size);  // for serialization only

  ArrayWrapper(const Array<Arg>* array);                // for serialization only
  ArrayWrapper(const Array<Arg*>* array);               // for serialization only
  ArrayWrapper(const GrowableArray<Arg>* array);        // for serialization only
  ArrayWrapper(const GrowableArray<Arg*>* array);       // for serialization only

  ~ArrayWrapper();

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  bool is_null() const { return _base == nullptr; }
  int size() const { return _size; }

  // Attention: it always return "Arg*" whether it is an array of "Arg*" or "Arg".
  // Use get_array() if you care about the performance.
  const Arg* get(int index) const;

  // This function is for serialization, And it's irreversible.
  int set_sub_arr(int start, int size);

  // after deserialization

  // The array will still be destroyed (if in deserialization mode) in ~ArrayWrapper.
  template <typename T>
  T* get_array();
  // Now the array will not be destroyed in ~ArrayWrapper.
  template <typename T>
  T* export_array();
};

template <typename Arg>
struct SerializationImpl<ArrayWrapper<Arg>> {
  static int serialize(MessageBuffer& buf, const ArrayWrapper<Arg>& arg);
  static int deserialize_ref(MessageBuffer& buf, ArrayWrapper<Arg>& arg);
  CANNOT_DESERIALIZE_POINTER(ArrayWrapper<Arg>);
};

/**
 * Serializing contiguous memory (using memcpy())
 */
class MemoryWrapper final: public WrapperBase {
public:
  struct MemoryAllocater {
    virtual void* alloc(int size) { ShouldNotReachHere(); return nullptr; }
  };

private:
  const void* _base;  // start of the memory
  uint32_t _size;     // bytes of the memory

  MemoryAllocater* const _allocater;

public:
  MemoryWrapper(const void* base, uint32_t size);       // for serialization only
  MemoryWrapper(MemoryAllocater* allocater = nullptr);  // for deserialization only

  ~MemoryWrapper();

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  // after deserialization
  bool is_null() const { return _base == nullptr; }
  uint32_t size() const { return _size; }

  // The memory will still be destroyed (if in deserialization mode) in ~MemoryWrapper.
  void* get_memory() const { return const_cast<void*>(_base); }
  // Now the memory will not be destroyed in ~MemoryWrappe.
  void* export_memory() {
    _free_memory = false;
    return get_memory();
  }
};

DECLARE_SERIALIZER_INTRUSIVE(MemoryWrapper);

/**
 * Compatible with the serializers of `char*` and `char[]`, but supports RAII.
 */
class StringWrapper final: public WrapperBase {
private:
  const char* _base;
  uint32_t _size;     // not including '\0'

public:
  StringWrapper(const char* str);                 // for serialization only
  StringWrapper(const char* str, uint32_t size);  // for serialization only
  StringWrapper();                                // for deserialization only
  ~StringWrapper();

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  uint32_t size() { return _size; }

  // The string will still be destroyed (if in deserialization mode) in ~StringWrapper.
  const char* get_string() const { return _base; }
  // Now the string will not be destroyed in ~StringWrapper.
  const char* export_string() {
    _free_memory = false;
    return _base;
  }
};

DECLARE_SERIALIZER_INTRUSIVE(StringWrapper);

class FileWrapper final: public WrapperBase {
public:
  static constexpr uint32_t MAX_SIZE_PER_TRANS = 40 * 1024 * 1024; // 40 MB

private:
  const char*       _file_path;
  uint32_t          _file_size;
  const char*       _tmp_file_path;

  mutable int       _fd;
  mutable int       _errno;
  mutable uint32_t  _handled_file_size;
  mutable bool      _handled_once;  // to cover _file_size = 0

private:
  void init_for_serialize();
  void init_for_deserialize();

  void on_deser_tmp_file_opened();
  void on_deser_tmp_file_exist();
  void on_deser_dir_not_exist();

  void on_deser_null();
  void on_deser_end();

  uint32_t get_size_to_send_this_time() const;

public:
  FileWrapper(const char* file_path, SerializationMode smode);
  ~FileWrapper();

  static uint32_t get_arg_meta_size_if_not_null();

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  bool is_null() const { return _file_size == MessageConst::NULL_PTR; }
  bool is_file_all_handled() const {
    guarantee(_file_size >= _handled_file_size, "sanity");
    return _handled_once && _file_size == _handled_file_size;
  }

  const char* file_path() const { return _file_path; }
  uint32_t file_size() const { return _file_size; }
  int fd() const { return _fd; }
  int has_file_err() const { return _errno != 0; }
  int file_err() const { return _errno; }
  bool is_tmp_file_already_exists() { _smode.assert_can_deserialize(); return _errno == EEXIST; }

  // Wait for other threads or processes to generate files.
  bool wait_for_file_deserialization(int timeout_millisecond = 2000);

  int send_file(CommunicationStream* stream);
  int recv_file(CommunicationStream* stream);
};

DECLARE_SERIALIZER_INTRUSIVE(FileWrapper);

#endif // SHARE_JBOOSTER_NET_SERIALIZATIONWRAPPERS_HPP
