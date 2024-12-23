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

#ifndef SHARE_JBOOSTER_SERVER_SERVERMESSAGEHANDLER_HPP
#define SHARE_JBOOSTER_SERVER_SERVERMESSAGEHANDLER_HPP

#include "jbooster/net/messageType.hpp"
#include "memory/allocation.hpp"

class ClassLoaderLocator;
template <class T> class GrowableArray;
class InstanceKlass;
class KlassLocator;
class Method;
class ServerStream;

/**
 * Unlike in ClientMessageHandler, the use of TRAPS is encouraged here because these
 * logics are all executed in JavaThread and ServerDataManager uses THREAD a lot.
 */
class ServerMessageHandler: public StackObj {
  ServerStream* _server_stream;
  bool _no_more_client_request;

private:
  NONCOPYABLE(ServerMessageHandler);

  int request_class_loaders(GrowableArray<ClassLoaderLocator> *loaders, TRAPS);
  int request_klasses(GrowableArray<KlassLocator>* klasses, TRAPS);
  int request_missing_class_loaders(TRAPS);
  int request_missing_klasses(TRAPS);
  int request_method_data(TRAPS);
  int request_methods_to_compile(GrowableArray<InstanceKlass*>* klasses_to_compile,
                                 GrowableArray<Method*>* methods_to_compile,
                                 TRAPS);
  int request_methods_not_compile(GrowableArray<Method*>* methods_not_compile, TRAPS);
  int request_client_cache(MessageType msg_type, JClientCacheState& cache);

  int try_to_compile_lazy_aot(GrowableArray<InstanceKlass*>* klasses_to_compile,
                              GrowableArray<Method*>* methods_to_compile,
                              GrowableArray<Method*>* methods_not_compile,
                              bool enabling_aot_pgo,
                              bool resolve_extra_klasses,
                              TRAPS);
public:
  ServerMessageHandler(ServerStream* server_stream);

  ServerStream& ss() { return *_server_stream; }

  int handle_a_task_from_client(MessageType msg_type, TRAPS);
  int handle_tasks_from_client(TRAPS);

  int handle_cache_file_sync_task(TRAPS);
  int handle_lazy_aot_compilation_task(TRAPS);
};

#endif // SHARE_JBOOSTER_SERVER_SERVERMESSAGEHANDLER_HPP
