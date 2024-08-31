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

#ifndef SHARE_JBOOSTER_CLIENT_CLIENTMESSAGEHANDLER_HPP
#define SHARE_JBOOSTER_CLIENT_CLIENTMESSAGEHANDLER_HPP

#include "jbooster/jBoosterManager.hpp"
#include "jbooster/net/messageType.hpp"

#define MESSAGES_FOR_CLIENT_TO_HANDLE(F)  \
  F(EndOfCurrentPhase       )             \
  F(CacheAggressiveCDS      )             \
  F(CacheClassLoaderResource)             \
  F(ClassLoaderLocators     )             \
  F(DataOfClassLoaders      )             \
  F(KlassLocators           )             \
  F(DataOfKlasses           )             \
  F(MethodLocators          )             \
  F(Profilinginfo           )             \
  F(ArrayKlasses            )             \

class ArrayKlass;
class ClassLoaderData;
class ClientStream;
template <typename T> class GrowableArray;
class InstanceKlass;
class JavaThread;
class Method;

/**
 * Try not to use TRAPS here because these functions may be called on any thread.
 */
class ClientMessageHandler: public StackObj {
public:
  enum TriggerTaskPhase {
    ON_STARTUP,
    ON_SHUTDOWN
  };

  /**
   * All the members should be trivial.
   */
  struct ClientMessageContext {
    GrowableArray<ClassLoaderData*>* all_class_loaders;
    GrowableArray<InstanceKlass*>* klasses_to_compile;
    GrowableArray<Method*>* methods_to_compile;
    GrowableArray<Method*>* methods_not_compile;
    GrowableArray<InstanceKlass*>* all_sorted_klasses;
    GrowableArray<ArrayKlass*>* array_klasses;
  };

private:
  ClientStream* _client_stream;
  ClientMessageContext _clent_message_ctx;
  bool _no_more_server_message;

private:
  NONCOPYABLE(ClientMessageHandler);

#define DECLARE_HANDLER(MT) int handle_##MT();
  MESSAGES_FOR_CLIENT_TO_HANDLE(DECLARE_HANDLER)
#undef DECLARE_HANDLER

public:
  ClientMessageHandler(ClientStream* client_stream);

  ClientStream& cs() { return *_client_stream; }
  ClientMessageContext& ctx() { return _clent_message_ctx; }

  static void trigger_cache_generation_tasks(TriggerTaskPhase phase, TRAPS);

  int handle_a_message_from_server(MessageType msg_type);
  int handle_messages_from_server();

  int send_cache_file_sync_task();
  int send_lazy_aot_compilation_task();
};

#endif // SHARE_JBOOSTER_CLIENT_CLIENTMESSAGEHANDLER_HPP
