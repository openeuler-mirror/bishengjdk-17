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

#ifndef SHARE_JBOOSTER_DATATRANSMISSIONUTILS_HPP
#define SHARE_JBOOSTER_DATATRANSMISSIONUTILS_HPP

#include "jbooster/jBoosterManager.hpp"
#include "jbooster/net/serializationWrappers.hpp"
#include "utilities/growableArray.hpp"

class Symbol;
class InstanceKlass;
class MethodData;

/**
 * To uniquely identify a class loader.
 */
class ClassLoaderKey final: public StackObj {
  // Fields that uniquely identify a class loader.
  Symbol* _loader_class_name;
  Symbol* _loader_name;
  Symbol* _first_loaded_class_name;

public:
  ClassLoaderKey(Symbol* loader_class_name, Symbol* loader_name, Symbol* first_loaded_class_name):
                 _loader_class_name(loader_class_name),
                 _loader_name(loader_name),
                 _first_loaded_class_name(first_loaded_class_name) { /* no need to increment_refcount() */ }
  ClassLoaderKey(const ClassLoaderKey& key);

  ~ClassLoaderKey();

  Symbol* loader_class_name() const { return _loader_class_name; }
  Symbol* loader_name() const { return _loader_name; }
  Symbol* first_loaded_class_name() const { return _first_loaded_class_name; }

  uintx hash() const {
    int v = 0;
    v = v * 31 + primitive_hash(_loader_class_name);
    v = v * 31 + primitive_hash(_loader_name);
    v = v * 31 + primitive_hash(_first_loaded_class_name);
    return v;
  }
  bool equals(const ClassLoaderKey& other) const {
    return _loader_class_name == other._loader_class_name
        && _loader_name == other._loader_name
        && _first_loaded_class_name == other._first_loaded_class_name;
  }
  ClassLoaderKey& operator = (const ClassLoaderKey& other);

  static ClassLoaderKey boot_loader_key();
  bool is_boot_loader() const;
  static ClassLoaderKey platform_loader_key();
  bool is_platform_loader() const;

  static ClassLoaderKey key_of_class_loader_data(ClassLoaderData* cld);
};

/**
 * The parent chain of the target class loader.
 * With the chain we can rebuild the dependencies for all class loaders on the
 * server. Notice that we assume that all class loaders follow the parents
 * delegation model (for now).
 */
class ClassLoaderChain: public StackObj {
public:
  struct Node: public StackObj {
    ClassLoaderKey key;
    uintptr_t client_cld_addr;

    Node(): key(nullptr, nullptr, nullptr), client_cld_addr(0) {}
    Node(const ClassLoaderKey& key, uintptr_t adr): key(key), client_cld_addr(adr) {}
    Node(const Node& node): key(node.key), client_cld_addr(node.client_cld_addr) {}

    Node& operator = (const Node& node) {
      key = node.key;
      client_cld_addr = node.client_cld_addr;
      return *this;
    }
  };

private:
  ClassLoaderData* _cld;
  mutable GrowableArray<Node> _chain;

private:
  void parse_cld() const;

public:
  ClassLoaderChain(): _cld(nullptr), _chain() {}
  ClassLoaderChain(ClassLoaderData* cld): _cld(cld), _chain() {
    assert(_cld != nullptr, "sanity");
  }
  ClassLoaderChain(const ClassLoaderChain& clc): _cld(clc._cld), _chain() {
    assert(_cld != nullptr, "sanity");
  }

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  ClassLoaderChain& operator = (const ClassLoaderChain& clc) {
    _cld = clc._cld;
    assert(_cld != nullptr, "sanity");
    assert(_chain.is_empty(), "sanity");
    return *this;
  }

  GrowableArray<Node>* chain() { return &_chain; }
};

DECLARE_SERIALIZER_INTRUSIVE(ClassLoaderChain);

/**
 * To locate the class loader oop or its ClassLoaderData.
 *
 * Class loaders are frequently transmitted between the client and server.
 * Therefore, only the ID (for now we use the pointer) of the loader are
 * transferred to reduce the data to be transmitted.
 *
 * * To transmit complete class loader data to the server, please use the
 * ClassLoaderData class itself.
 */
class ClassLoaderLocator: public StackObj {
  ClassLoaderData* _class_loader_data;
  mutable uintptr_t _client_cld_address;  // only used by the server

public:
  ClassLoaderLocator(Handle class_loader_h);
  ClassLoaderLocator(ClassLoaderData* data): _class_loader_data(data),
                                             _client_cld_address(0) {}
  ClassLoaderLocator(): _class_loader_data(nullptr),
                        _client_cld_address(0) {}

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  void set_class_loader_data(Handle class_loader_h);
  void set_class_loader_data(ClassLoaderData* cld) { _class_loader_data = cld; }
  ClassLoaderData* class_loader_data() const { return _class_loader_data; }

  // only used by the server
  ClassLoaderData* recheck_class_loader_data(MessageBuffer& buf);
  uintptr_t client_cld_address() { return _client_cld_address; }
};

DECLARE_SERIALIZER_INTRUSIVE(ClassLoaderLocator);

/**
 * To locate a klass.
 */
class KlassLocator: public StackObj {
  Symbol* _class_name;
  uintptr_t _client_klass;
  uint64_t _fingerprint;
  ClassLoaderLocator _class_loader_locator;

public:
  KlassLocator(InstanceKlass* klass);
  KlassLocator(): _class_name(nullptr),
                  _client_klass(0u),
                  _fingerprint(0u),
                  _class_loader_locator((ClassLoaderData*)nullptr) {}

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  void set_class_name(Symbol* class_name) { _class_name = class_name; }
  Symbol* class_name() const { return _class_name; }

  uintptr_t client_klass() { return _client_klass; }

  void set_fingerprint(uint64_t fingerprint) { _fingerprint = fingerprint; }
  uint64_t fingerprint() { return _fingerprint; }

  void set_class_loader(Handle class_loader_h) {
    _class_loader_locator.set_class_loader_data(class_loader_h);
  }
  void set_class_loader(ClassLoaderData* class_loader_data) {
    _class_loader_locator.set_class_loader_data(class_loader_data);
  }

  ClassLoaderLocator& class_loader_locator() { return _class_loader_locator; }

  InstanceKlass* try_to_get_ik(TRAPS);
};

DECLARE_SERIALIZER_INTRUSIVE(KlassLocator);

/**
 * To locate a method.
 */
class MethodLocator: public StackObj {
  Method* _method;

public:
  MethodLocator(Method* method): _method(method) {}
  MethodLocator(): _method(nullptr) {}

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  Method* get_method() { return _method; }
};

DECLARE_SERIALIZER_INTRUSIVE(MethodLocator);

/**
 * To collect method data.
 */
class ProfileDataCollector: public StackObj {
private:
  uint32_t _klass_size;
  InstanceKlass** _klass_array_base;

public:
  ProfileDataCollector(uint32_t size, InstanceKlass** klass_array_base);

  int serialize(MessageBuffer& buf) const;
  int deserialize(MessageBuffer& buf);

  uint32_t klass_size() const { return _klass_size; }
  InstanceKlass* klass_at(uint32_t index) const;
};

DECLARE_SERIALIZER_INTRUSIVE(ProfileDataCollector);

#endif // SHARE_JBOOSTER_DATATRANSMISSIONUTILS_HPP
