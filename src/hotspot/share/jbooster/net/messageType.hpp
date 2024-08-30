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

#ifndef SHARE_JBOOSTER_NET_MESSAGETYPE_HPP
#define SHARE_JBOOSTER_NET_MESSAGETYPE_HPP

#include "utilities/globalDefinitions.hpp"  // for uint16_t

#define MESSAGE_TYPES(f)                                  \
  /* session/stream meta related */                       \
  f(ClientSessionMeta,                  "from client"   ) \
  f(ClientStreamMeta,                   "from client"   ) \
  f(ClientSessionMetaAgain,             "from client"   ) \
                                                          \
  /* task related */                                      \
  f(EndOfCurrentPhase,                  "from both"     ) \
  f(NoMoreRequests,                     "from client"   ) \
  f(ClientDaemonTask,                   "from client"   ) \
  f(CacheFilesSyncTask,                 "from client"   ) \
  f(LazyAOTCompilationTask,             "from client"   ) \
                                                          \
  /* cache file related */                                \
  f(GetLazyAOTCache,                    "from client"   ) \
  f(GetAggressiveCDSCache,              "from client"   ) \
  f(GetClassLoaderResourceCache,        "from client"   ) \
  f(CacheAggressiveCDS,                 "from server"   ) \
  f(CacheClassLoaderResource,           "from server"   ) \
                                                          \
  /* Lazy AOT related */                                  \
  f(ClassLoaderLocators,                "from server"   ) \
  f(DataOfClassLoaders,                 "from server"   ) \
  f(KlassLocators,                      "from server"   ) \
  f(Profilinginfo,                      "from server"   ) \
  f(ArrayKlasses,                       "from server"   ) \
  f(DataOfKlasses,                      "from server"   ) \
  f(MethodLocators,                     "from server"   ) \
                                                          \
  /* others */                                            \
  f(FileSegment,                        "from both"     ) \
  f(Heartbeat,                          "from both"     ) \
  f(AOTRelatedClassNames,               "from client"   ) \
  f(AOTCompilationResult,               "from server"   ) \
  f(AbortCompilation,                   "from both"     ) \
  f(CompilationFailure,                 "from both"     ) \
  f(UnexpectedMessageType,              "from both"     ) \
  f(UnsupportedClient,                  "from server"   ) \
  f(Unknown,                            "from both"     ) \


#define REGISTER_MESSAGE_TYPE_ENUM(type_name, human_readable) type_name,

enum class MessageType: uint16_t {
  MESSAGE_TYPES(REGISTER_MESSAGE_TYPE_ENUM)
  END_OF_MESSAGE_TYPE
};

#undef REGISTER_MESSAGE_TYPE_ENUM

const char* msg_type_name(MessageType meg_type);

#endif // SHARE_JBOOSTER_NET_MESSAGETYPE_HPP
