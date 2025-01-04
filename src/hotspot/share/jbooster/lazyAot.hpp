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

#ifndef SHARE_JBOOSTER_LAZYAOT_HPP
#define SHARE_JBOOSTER_LAZYAOT_HPP

#include "jbooster/utilities/scalarHashMap.hpp"
#include "memory/allocation.hpp"
#include "runtime/handles.hpp"
#include "runtime/thread.hpp"

class AOTCodeHeap;
class ArrayKlass;
class ClassLoaderData;
template <class T> class GrowableArray;
class InstanceKlass;
class Method;
class MethodData;
class OopHandle;

class ClassLoaderKeepAliveMark: public StackObj {
  GrowableArray<OopHandle> _handles;

public:
  ~ClassLoaderKeepAliveMark();

  void add(ClassLoaderData* cld);
  void add_all(GrowableArray<ClassLoaderData*>* clds);
};

class LazyAOT: public AllStatic {
private:
  static const int _compilation_related_klasses_layers = 2;

private:
  static InstanceKlass* get_klass_from_class_tag(const constantPoolHandle& cp,
                                                 int index,
                                                 Handle loader,
                                                 int which_klasses,
                                                 TRAPS);
  static void get_klasses_from_name_and_type_tag(const constantPoolHandle& cp,
                                                 int index,
                                                 Handle loader,
                                                 GrowableArray<InstanceKlass*> &res,
                                                 int which_klasses,
                                                 TRAPS);

  static void collect_klasses_by_inheritance(GrowableArray<InstanceKlass*>* dst,
                                             ScalarHashSet<InstanceKlass*>* vis,
                                             InstanceKlass* ik,
                                             TRAPS);
  static void collect_klasses_in_constant_pool(GrowableArray<InstanceKlass*>* dst,
                                               ScalarHashSet<InstanceKlass*>* vis,
                                               InstanceKlass* ik,
                                               int which_klasses,
                                               TRAPS);
  static void collect_klasses_in_constant_pool(GrowableArray<InstanceKlass*>* dst,
                                               ScalarHashSet<InstanceKlass*>* vis,
                                               TRAPS);
  static void collect_klasses_in_method_data(GrowableArray<InstanceKlass*>* dst_ik,
                                             GrowableArray<ArrayKlass*>* dst_ak,
                                             ScalarHashSet<InstanceKlass*>* ik_vis,
                                             TRAPS);
  static void collect_klasses_in_method_data(GrowableArray<InstanceKlass*>* dst_ik,
                                             GrowableArray<ArrayKlass*>* dst_ak,
                                             ScalarHashSet<InstanceKlass*>* ik_vis,
                                             ScalarHashSet<ArrayKlass*>* ak_vis,
                                             MethodData* method_data,
                                             int which_klasses,
                                             TRAPS);
  static void sort_klasses_by_inheritance(GrowableArray<InstanceKlass*>* dst_ik,
                                          GrowableArray<ArrayKlass*>* dst_ak,
                                          GrowableArray<InstanceKlass*>* src,
                                          bool reverse_scan_src,
                                          TRAPS);

public:
  static bool can_be_compiled(const methodHandle& mh);
  static bool can_be_compiled(InstanceKlass* ik, bool check_cld = true);
  static bool can_be_compiled(ClassLoaderData* cld);

  static void collect_all_klasses_to_compile(ClassLoaderKeepAliveMark& clka,
                                             GrowableArray<ClassLoaderData*>* all_loaders,
                                             GrowableArray<InstanceKlass*>* klasses_to_compile,
                                             GrowableArray<Method*>* methods_to_compile,
                                             GrowableArray<Method*>* methods_not_compile,
                                             GrowableArray<InstanceKlass*>* all_sorted_klasses,
                                             GrowableArray<ArrayKlass*>* array_klasses,
                                             TRAPS);

  static bool compile_classes_by_graal(int session_id,
                                       const char* file_path,
                                       GrowableArray<InstanceKlass*>* klasses,
                                       bool use_pgo,
                                       bool resolve_extra_klasses,
                                       TRAPS);
  static bool compile_methods_by_graal(int session_id,
                                       const char* file_path,
                                       GrowableArray<InstanceKlass*>* klasses,
                                       GrowableArray<Method*>* methods_to_compile,
                                       GrowableArray<Method*>* methods_not_compile,
                                       bool use_pgo,
                                       bool resolve_extra_klasses,
                                       TRAPS);
};

#endif // SHARE_JBOOSTER_LAZYAOT_HPP
