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

#include "cds/dynamicArchive.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/systemDictionary.hpp"
#include "jbooster/client/clientDataManager.hpp"
#include "jbooster/client/clientMessageHandler.hpp"
#include "jbooster/dataTransmissionUtils.hpp"
#include "jbooster/lazyAot.hpp"
#include "jbooster/net/clientStream.hpp"
#include "jbooster/net/serializationWrappers.inline.hpp"
#include "jbooster/utilities/debugUtils.inline.hpp"
#include "memory/resourceArea.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/method.hpp"
#include "runtime/arguments.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/timerTrace.hpp"
#include "utilities/growableArray.hpp"

ClientMessageHandler::ClientMessageHandler(ClientStream* client_stream):
                                          _client_stream(client_stream),
                                          _no_more_server_message(false) {
  // Make sure all the members are trivial.
  memset(&_clent_message_ctx, 0, sizeof(_clent_message_ctx));
}

int ClientMessageHandler::handle_a_message_from_server(MessageType msg_type) {
  DebugUtils::assert_thread_in_native();

  _no_more_server_message = false;
  switch (msg_type) {
#define MESSAGE_TYPE_CASE(MT)     \
    case MessageType::MT:         \
      JB_RETURN(handle_##MT());   \
      break;                      \

    MESSAGES_FOR_CLIENT_TO_HANDLE(MESSAGE_TYPE_CASE)
#undef MESSAGE_TYPE_CASE
    default:
      JB_RETURN(JBErr::BAD_MSG_TYPE);
      break;
  }
  return 0;
}

int ClientMessageHandler::handle_messages_from_server() {
  DebugUtils::assert_thread_in_native();

  MessageType msg_type;
  do {
    JB_RETURN(cs().recv_request(msg_type));
    JB_RETURN(handle_a_message_from_server(msg_type));
  } while (!_no_more_server_message);
  return 0;
}

void ClientMessageHandler::trigger_cache_generation_tasks(TriggerTaskPhase phase, TRAPS) {
  DebugUtils::assert_thread_in_native();

  ClientStream cs(JBoosterAddress, JBoosterPort, JBoosterTimeout, THREAD);
  ClientStream* client_stream = &cs;

  JB_TRY {
    JB_THROW(client_stream->connect_and_init_stream());
    ClientMessageHandler msg_handler(client_stream);

    bool cache_file_task = false;
    if (ClientDataManager::get().boost_level().is_clr_allowed() || ClientDataManager::get().boost_level().is_cds_allowed()) {
      cache_file_task = (phase == ON_SHUTDOWN);
    }
    if (cache_file_task) {
      JB_THROW(msg_handler.send_cache_file_sync_task());
    }

    bool lazy_aot_task = false;
    if (ClientDataManager::get().boost_level().is_aot_allowed()) {
      lazy_aot_task = ((phase == ON_STARTUP) || ((phase == ON_SHUTDOWN) && !ClientDataManager::get().is_startup_end()));
    }
    if (lazy_aot_task) {
      JB_THROW(msg_handler.send_lazy_aot_compilation_task());
    }
  } JB_TRY_END
  JB_CATCH_REST() {
    client_stream->LOG_OR_CRASH();
  } JB_CATCH_END;

  if (HAS_PENDING_EXCEPTION) {
    LogTarget(Warning, jbooster) lt;
    if (lt.is_enabled()) {
      lt.print("Unhandled exception at ClientMessageHandler::trigger_cache_generation_tasks()!");
    }
    DebugUtils::clear_java_exception_and_print_stack_trace(lt, THREAD);
  }
}

// ------------------------------- Message Handlers --------------------------------

int ClientMessageHandler::handle_EndOfCurrentPhase() {
  _no_more_server_message = true;
  return 0;
}

static void dump_cds() {
  JavaThread* THREAD = JavaThread::current();
  TraceTime tt("Duration CDS", TRACETIME_LOG(Info, jbooster));
  ThreadInVMfromNative tivm(THREAD);
  ExceptionMark em(THREAD);

  const char* cds_path = ClientDataManager::get().cache_cds_path();
  // SharedDynamicArchivePath is "<cds-path>.tmp" here.
  const char* tmp_cds_path = Arguments::GetSharedDynamicArchivePath();

  // Try to create the tmp file (get the file lock) before DynamicArchive::dump().
  // Do not try to create the tmp file in FileMapInfo::open_for_write() because if
  // the file fails to be created, the whole process will exit (see
  // FileMapInfo::fail_stop()).
  int fd = JBoosterManager::create_and_open_tmp_cache_file(tmp_cds_path);
  if (fd < 0) {
    log_error(jbooster, cds)("Failed to open the tmp cds dump file \"%s\". Skip dump. "
                              "Why is some other process dumping?",
                              tmp_cds_path);
    return;
  }
  os::close(fd);
  fd = -1;

  bool rename_successful = false;
  // Skip dump if some other process already dumped.
  if (!FileUtils::is_file(cds_path)) {
    DynamicArchive::dump();
    chmod(tmp_cds_path, S_IREAD);
    rename_successful = FileUtils::rename(tmp_cds_path, cds_path);
  } else {
    log_warning(jbooster, cds)("The cds jsa file \"%s\" already exists. Skip dump.", cds_path);
  }

  if (!rename_successful) {
    FileUtils::remove(tmp_cds_path);
  }

  if (HAS_PENDING_EXCEPTION) {
    LogTarget(Error, jbooster, cds) lt;
    if (lt.is_enabled()) {
      lt.print("ArchiveClassesAtExit has failed:");
    }
    DebugUtils::clear_java_exception_and_print_stack_trace(lt, THREAD);
  }
}

int ClientMessageHandler::handle_CacheAggressiveCDS() {
  if (DynamicDumpSharedSpaces && ClientDataManager::get().boost_level().is_cds_allowed()) {
    dump_cds();
  }
  FileWrapper file(ClientDataManager::get().cache_cds_path(),
                   SerializationMode::SERIALIZE);
  JB_RETURN(file.send_file(&cs()));
  return 0;
}

int ClientMessageHandler::handle_CacheClassLoaderResource() {
  FileWrapper file(ClientDataManager::get().cache_clr_path(),
                   SerializationMode::SERIALIZE);
  JB_RETURN(file.send_file(&cs()));
  return 0;
}

int ClientMessageHandler::handle_ClassLoaderLocators() {
  GrowableArray<ClassLoaderLocator> clls;
  GrowableArray<ClassLoaderData*>* all_loaders = ctx().all_class_loaders;
  for (GrowableArrayIterator<ClassLoaderData*> iter = all_loaders->begin();
                                               iter != all_loaders->end();
                                               ++iter) {
    clls.append(ClassLoaderLocator(*iter));
  }
  ArrayWrapper<ClassLoaderLocator> cll_aw(&clls);
  JB_RETURN(cs().send_response(&cll_aw));
  return 0;
}

int ClientMessageHandler::handle_DataOfClassLoaders() {
  ArrayWrapper<ClassLoaderLocator> cll_aw(false);
  JB_RETURN(cs().parse_request(&cll_aw));
  ClassLoaderLocator* loader_locators = cll_aw.get_array<ClassLoaderLocator>();
  GrowableArray<ClassLoaderChain> chains;
  for (int i = 0; i < cll_aw.size(); ++i) {
    ClassLoaderData* data = loader_locators[i].class_loader_data();
    chains.append(ClassLoaderChain(data));
  }
  ArrayWrapper<ClassLoaderChain> aw(&chains);
  JB_RETURN(cs().send_response(&aw));
  return 0;
}

int ClientMessageHandler::handle_KlassLocators() {
  GrowableArray<KlassLocator> kls;
  GrowableArray<InstanceKlass*>* klasses = ctx().all_sorted_klasses;
  for (GrowableArrayIterator<InstanceKlass*> iter = klasses->begin();
                                             iter != klasses->end();
                                             ++iter) {
    kls.append(KlassLocator(*iter));
  }
  ArrayWrapper<KlassLocator> kl_aw(&kls);
  JB_RETURN(cs().send_response(&kl_aw));
  return 0;
}

int ClientMessageHandler::handle_DataOfKlasses() {
  Thread* thread = Thread::current();
  ArrayWrapper<KlassLocator> klasses_aw(false);
  JB_RETURN(cs().parse_request(&klasses_aw));
  KlassLocator* klass_locators = klasses_aw.get_array<KlassLocator>();
  ResourceMark rm;
  InstanceKlass** klasses = NEW_RESOURCE_ARRAY(InstanceKlass*, klasses_aw.size());
  for (int i = 0; i < klasses_aw.size(); ++i) {
    KlassLocator& w = klass_locators[i];
    Handle loader(thread, w.class_loader_locator().class_loader_data()->class_loader());
    Klass* k = SystemDictionary::find_instance_klass(w.class_name(), loader, Handle());
    if (k == nullptr) {
      klasses[i] = nullptr;
      ResourceMark rm;
      log_warning(jbooster, compilation)("Unresolved class \"%s\".",
                                         w.class_name()->as_C_string());
    } else {
      klasses[i] = InstanceKlass::cast(k);
    }
  }
  ArrayWrapper<InstanceKlass> klass_array(klasses, klasses_aw.size());
  JB_RETURN(cs().send_response(&klass_array));
  return 0;
}

int ClientMessageHandler::handle_MethodLocators() {
  GrowableArray<MethodLocator> mls;
  GrowableArray<Method*>* methods;
  bool to_compile;
  JB_RETURN(cs().parse_request(&to_compile));
  if (to_compile) {
    methods = ctx().methods_to_compile;
  } else {
    methods = ctx().methods_not_compile;
  }
  for (GrowableArrayIterator<Method*> iter = methods->begin();
                                      iter != methods->end();
                                      ++iter) {
    mls.append(MethodLocator(*iter));
  }
  ArrayWrapper<MethodLocator> ml_aw(&mls);
  JB_RETURN(cs().send_response(&ml_aw));
  return 0;
}

int ClientMessageHandler::handle_Profilinginfo() {
  ResourceMark rm;
  ArrayWrapper<uintptr_t> aw(false);
  JB_RETURN(cs().parse_request(&aw));
  uintptr_t* klass_array = aw.get_array<uintptr_t>();
  ProfileDataCollector data_collector(aw.size(), (InstanceKlass**) klass_array);
  JB_RETURN(cs().send_response(&data_collector));
  return 0;
}

int ClientMessageHandler::handle_ArrayKlasses() {
  ResourceMark rm;
  GrowableArray<ArrayKlass*>* array_klasses = ctx().array_klasses;
  ArrayWrapper<ArrayKlass> klass_array(array_klasses);
  JB_RETURN(cs().send_response(&klass_array));
  return 0;
}

int ClientMessageHandler::handle_ResolveExtraKlasses() {
  ResourceMark rm;
  bool should_resolve_extra_Klasses = JBoosterResolveExtraKlasses;
  JB_RETURN(cs().send_response(&should_resolve_extra_Klasses));
  return 0;
}
// ---------------------------------- Some Tasks -----------------------------------

int ClientMessageHandler::send_cache_file_sync_task() {
  DebugUtils::assert_thread_nonjava_or_in_native();
  JB_RETURN(cs().send_request(MessageType::CacheFilesSyncTask));
  JB_RETURN(handle_messages_from_server());
  return 0;
}

int ClientMessageHandler::send_lazy_aot_compilation_task() {
  DebugUtils::assert_thread_nonjava_or_in_native();
  JavaThread* THREAD = JavaThread::current();
  HandleMark hm(THREAD);
  ResourceMark rm(THREAD);

  JB_RETURN(cs().send_request(MessageType::LazyAOTCompilationTask));
  bool should_send_klasses;
  JB_RETURN(cs().recv_response(&should_send_klasses));
  if (!should_send_klasses) {
    log_info(jbooster, compilation)("The server doesn't need klass data now.");
    return 0;
  }

  // Keep the class loaders alive until the end of this function.
  ClassLoaderKeepAliveMark clka;

  GrowableArray<ClassLoaderData*> all_loaders;
  GrowableArray<InstanceKlass*>   klasses_to_compile;
  GrowableArray<Method*>          methods_to_compile;
  GrowableArray<Method*>          methods_not_compile;
  GrowableArray<InstanceKlass*>   all_sorted_klasses;
  GrowableArray<ArrayKlass*>      array_klasses;

  {
    TraceTime tt("Duration AOT", TRACETIME_LOG(Info, jbooster));
    LazyAOT::collect_all_klasses_to_compile(clka,
                                            &all_loaders,
                                            &klasses_to_compile,
                                            &methods_to_compile,
                                            &methods_not_compile,
                                            &all_sorted_klasses,
                                            &array_klasses,
                                            CHECK_(JBErr::THREAD_EXCEPTION));
  }

  ctx().all_class_loaders   = &all_loaders;
  ctx().klasses_to_compile  = &klasses_to_compile;
  ctx().methods_to_compile  = &methods_to_compile;
  ctx().methods_not_compile = &methods_not_compile;
  ctx().all_sorted_klasses  = &all_sorted_klasses;
  ctx().array_klasses       = &array_klasses;
  JB_RETURN(handle_messages_from_server());

  log_info(jbooster, compilation)("All %d klasses have been sent to the server.",
                                  all_sorted_klasses.length());
  return 0;
}
