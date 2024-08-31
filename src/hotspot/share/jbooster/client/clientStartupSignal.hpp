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

#ifndef SHARE_JBOOSTER_CLIENT_CLIENTSTARTUPSIGNAL_HPP
#define SHARE_JBOOSTER_CLIENT_CLIENTSTARTUPSIGNAL_HPP

#include "jbooster/jBoosterManager.hpp"

class InstanceKlass;
class Method;
class Symbol;

/**
 * This class is used to determine whether the current program is
 * at the end of the startup phase.
 */
class ClientStartupSignal: public AllStatic {
  static volatile bool _startup_class_found;

  static Symbol* _klass_name_sym;

  static const char* _klass_name;
  static const char* _method_name;
  static const char* _method_signature;

private:
  static void init_x_flag();
  static void init_d_flag();
  static bool inject_startup_callback(unsigned char** data_ptr, int* data_len, TRAPS);

public:
  static void init_phase1();
  static void init_phase2();
  void free();

  static const char* klass_name() { return _klass_name; }
  static const char* method_name() { return _method_name; }
  static const char* method_signature() { return _method_signature; }

  static bool is_target_klass(Symbol* klass_name) { return _klass_name_sym == klass_name; }
  static bool try_inject_startup_callback(unsigned char** data_ptr, int* data_len, TRAPS);
};

#endif // SHARE_JBOOSTER_CLIENT_CLIENTSTARTUPSIGNAL_HPP
