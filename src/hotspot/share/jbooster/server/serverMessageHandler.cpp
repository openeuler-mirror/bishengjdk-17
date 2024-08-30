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

#include "jbooster/lazyAot.hpp"
#include "jbooster/net/serializationWrappers.inline.hpp"
#include "jbooster/net/serverStream.hpp"
#include "jbooster/server/serverControlThread.hpp"
#include "jbooster/server/serverDataManager.hpp"
#include "jbooster/server/serverMessageHandler.hpp"
#include "jbooster/utilities/debugUtils.inline.hpp"
#include "memory/resourceArea.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/method.hpp"
#include "runtime/interfaceSupport.inline.hpp"

ServerMessageHandler::ServerMessageHandler(ServerStream* server_stream): _server_stream(server_stream),
                                                                         _no_more_client_request(false) {}

int ServerMessageHandler::handle_a_task_from_client(MessageType msg_type, TRAPS) {
  _no_more_client_request = false;
  switch (msg_type) {
    case MessageType::ClientDaemonTask:
      ServerDataManager::get().control_thread()->add_client_daemon_connection(&ss(), THREAD);
      _no_more_client_request = true;
      break;
    case MessageType::CacheFilesSyncTask:
      JB_RETURN(handle_cache_file_sync_task(THREAD));
      break;
    case MessageType::LazyAOTCompilationTask:
      JB_RETURN(handle_lazy_aot_compilation_task(THREAD));
      break;
    case MessageType::NoMoreRequests:
      _no_more_client_request = true;
      break;
    default:
      JB_RETURN(JBErr::BAD_MSG_TYPE);
      break;
  }
  return 0;
}

int ServerMessageHandler::handle_tasks_from_client(TRAPS) {
  MessageType msg_type;
  do {
    JB_RETURN(ss().recv_request(msg_type));
    JB_RETURN(handle_a_task_from_client(msg_type, THREAD));
  } while (!_no_more_client_request);
  return 0;
}

int ServerMessageHandler::request_class_loaders(GrowableArray<ClassLoaderLocator> *loaders, TRAPS) {
  if (loaders->is_empty()) return 0;
  ArrayWrapper<ClassLoaderChain> chains_aw(false);
  int size = loaders->length();
  ArrayWrapper<ClassLoaderLocator> aw(loaders);
  JB_RETURN(ss().send_request(MessageType::DataOfClassLoaders, &aw));
  JB_RETURN(ss().recv_response(&chains_aw));
  guarantee(size == chains_aw.size(), "sanity");

  JClientSessionData* sd = ss().session_data();
  ClassLoaderChain* chains = chains_aw.get_array<ClassLoaderChain>();
  for (int i = 0; i < size; ++i) {
    ClassLoaderChain& loader_chain = chains[i];
    GrowableArray<ClassLoaderChain::Node>* chain = loader_chain.chain();
    ClassLoaderChain::Node& last_node = chain->at(chain->length() - 1);
    guarantee(last_node.key.is_boot_loader(), "sanity");
    sd->add_class_loader_if_absent((address) last_node.client_cld_addr,
                                   last_node.key,
                                   last_node.key,
                                   CHECK_(JBErr::THREAD_EXCEPTION));
    for (int j = chain->length() - 2; j >= 0; --j) {
      ClassLoaderChain::Node& node = chain->at(j);
      ClassLoaderChain::Node& parent_node = chain->at(j + 1);
      ClassLoaderData* cld = sd->add_class_loader_if_absent((address) node.client_cld_addr,
                                                            node.key,
                                                            parent_node.key,
                                                            CHECK_(JBErr::THREAD_EXCEPTION));
      if (cld == nullptr) {
        // Failed to register the class loader. So skip the loaders above too.
        break;
      }
    }
  }
  return 0;
}

int ServerMessageHandler::request_klasses(GrowableArray<KlassLocator>* klasses, TRAPS) {
  const int max_req_cnt = 1000;
  const int klass_cnt = klasses->length();
  for (int recv_cnt = 0; recv_cnt < klass_cnt; recv_cnt += max_req_cnt) {
    int req_cnt;
    { ArrayWrapper<KlassLocator> kl_aw(klasses);
      req_cnt = kl_aw.set_sub_arr(recv_cnt, max_req_cnt);
      JB_RETURN(ss().send_request(MessageType::DataOfKlasses, &kl_aw));
      // Cannot use recv_response() here because deserialization of InstanceKlass must be in
      // _threa_in_vm state. So we split it into recv_request() and parse_request().
      MessageType msg_type;
      JB_RETURN(ss().recv_request(msg_type));
      if (msg_type != MessageType::DataOfKlasses) {
        JB_RETURN(JBErr::BAD_MSG_TYPE);
      }
    }
    ThreadInVMfromNative tiv(THREAD);
    ArrayWrapper<InstanceKlass> ik_aw(true);
    JB_RETURN(ss().parse_request(&ik_aw));
    guarantee(ik_aw.size() == req_cnt, "sanity");
  }
  return 0;
}

int ServerMessageHandler::request_missing_class_loaders(TRAPS) {
  JB_RETURN(ss().send_request(MessageType::ClassLoaderLocators));
  ArrayWrapper<ClassLoaderLocator> cll_aw(false);
  JB_RETURN(ss().recv_response(&cll_aw));
  ClassLoaderLocator* cll_arr = cll_aw.get_array<ClassLoaderLocator>();

  ResourceMark rm(THREAD);
  GrowableArray<ClassLoaderLocator> missing_loaders;
  for (int i = 0; i < cll_aw.size(); ++i) {
    ClassLoaderLocator& cll = cll_arr[i];
    if (cll.class_loader_data() != nullptr) continue;
    missing_loaders.append(cll);
  }

  // get class loaders
  JB_RETURN(request_class_loaders(&missing_loaders, THREAD));
  return 0;
}

int ServerMessageHandler::request_missing_klasses(TRAPS) {
  JB_RETURN(ss().send_request(MessageType::KlassLocators));
  ArrayWrapper<KlassLocator> kl_aw(false);
  JB_RETURN(ss().recv_response(&kl_aw));
  KlassLocator* kl_arr = kl_aw.get_array<KlassLocator>();
  log_debug(jbooster, compilation)("Related klasses: %d. session_id=%u.",
                                   kl_aw.size(),
                                   ss().session_id());

  ResourceMark rm(THREAD);
  GrowableArray<KlassLocator> missing_klasses;
  {
    JavaThread* java_thread = THREAD->as_Java_thread();
    for (int i = 0; i < kl_aw.size(); ++i) {
      KlassLocator& kl = kl_arr[i];
      InstanceKlass* ik;
      { ThreadInVMfromNative tivm(java_thread);
        ik = kl.try_to_get_ik(THREAD);
      }
      if (ik == nullptr) missing_klasses.append(kl);
      else {
        assert(ik->has_stored_fingerprint(), "add -XX:+CalculateClassFingerprint please");
        if (ik->get_stored_fingerprint() != kl.fingerprint()) {
          missing_klasses.append(kl);
          ResourceMark rm(THREAD);
          log_warning(jbooster, compilation)("Bad fingerprint of \"%s\": client=%lu, server=%lu. session_id=%u.",
                                             ik->internal_name(),
                                             kl.fingerprint(), ik->get_stored_fingerprint(),
                                             ss().session_id());
        } else {
          JClientSessionData* session_data = ss().session_data();
          session_data->add_klass_address((address)(kl.client_klass()),
                                          (address)ik,
                                          CHECK_(JBErr::THREAD_EXCEPTION));
        }
      }
    }
  }
  log_debug(jbooster, compilation)("Missing klasses: %d. session_id=%u.",
                                   missing_klasses.length(),
                                   ss().session_id());
  if (!missing_klasses.is_empty()) {
    JB_RETURN(request_klasses(&missing_klasses, THREAD));
  }
  return 0;
}

int ServerMessageHandler::request_method_data(TRAPS) {
  ResourceMark rm(THREAD);
  GrowableArray<address> client_klass_array;
  GrowableArray<address> server_klass_array;
  ss().session_data()->klass_array(&client_klass_array,
                                   &server_klass_array,
                                   THREAD);

  {
    ArrayWrapper<address> aw(&client_klass_array);
    JB_RETURN(ss().send_request(MessageType::Profilinginfo, &aw));
    InstanceKlass** klass_array_base = NULL;
    if (server_klass_array.length() > 0) {
      klass_array_base = (InstanceKlass**)server_klass_array.adr_at(0);
    }
    ProfileDataCollector data_collector(server_klass_array.length(), klass_array_base);
    JB_RETURN(ss().recv_response(&data_collector));
  }

  {
    JB_RETURN(ss().send_request(MessageType::ArrayKlasses));
    MessageType msg_type;
    JB_RETURN(ss().recv_request(msg_type));
    if (msg_type != MessageType::ArrayKlasses) {
      JB_RETURN(JBErr::BAD_MSG_TYPE);
    }
    ThreadInVMfromNative tivm(THREAD);
    ArrayWrapper<ArrayKlass> klasses(true);
    JB_RETURN(ss().parse_request(&klasses));
  }

  ss().session_data()->klass_pointer_map_to_server(&server_klass_array, THREAD);
  return 0;
}

int ServerMessageHandler::request_methods_to_compile(GrowableArray<InstanceKlass*>* klasses_to_compile,
                                                     GrowableArray<Method*>* methods_to_compile,
                                                     TRAPS) {
  bool to_compile = true;
  JB_RETURN(ss().send_request(MessageType::MethodLocators, &to_compile));
  ArrayWrapper<MethodLocator> ml_aw(false);
  JB_RETURN(ss().recv_response(&ml_aw));
  InstanceKlass* last_ik = nullptr;
  MethodLocator* ml_arr = ml_aw.get_array<MethodLocator>();
  for (int i = 0; i < ml_aw.size(); ++i) {
    Method* m = ml_arr[i].get_method();
    if (m != nullptr) {
      methods_to_compile->append(m);
      InstanceKlass* ik = m->method_holder();
      if (last_ik != ik) {
        last_ik = ik;
        klasses_to_compile->append(ik);
      }
    }
  }
  log_debug(jbooster, compilation)("To compile: klasses=%d, methods=%d, session_id=%u.",
                                   klasses_to_compile->length(),
                                   methods_to_compile->length(),
                                   ss().session_id());
  return 0;
}

int ServerMessageHandler::request_methods_not_compile(GrowableArray<Method*>* methods_not_compile, TRAPS) {
  bool to_compile = false;
  JB_RETURN(ss().send_request(MessageType::MethodLocators, &to_compile));
  ArrayWrapper<MethodLocator> ml_aw(false);
  JB_RETURN(ss().recv_response(&ml_aw));
  MethodLocator* ml_arr = ml_aw.get_array<MethodLocator>();
  for (int i = 0; i < ml_aw.size(); ++i) {
    Method* m = ml_arr[i].get_method();
    if (m != nullptr) {
      methods_not_compile->append(m);
    }
  }
  log_debug(jbooster, compilation)("Methods not compile: %d. session_id=%u.",
                                   methods_not_compile->length(),
                                   ss().session_id());
  return 0;
}

int ServerMessageHandler::request_client_cache(MessageType msg_type, JClientCacheState& cache) {
  if (cache.is_allowed() && !cache.is_cached() && cache.set_being_generated()) {
    JB_TRY {
      JB_THROW(ss().send_request(msg_type));
      FileWrapper file(cache.file_path(), SerializationMode::DESERIALIZE);
      JB_THROW(file.recv_file(&ss()));
      if (file.is_null()) cache.set_not_generated();
      else cache.set_generated();
    } JB_TRY_END
    JB_CATCH_REST() {
      cache.set_not_generated();
      return JB_ERR;
    } JB_CATCH_END;
  }
  return 0;
}

int ServerMessageHandler::handle_cache_file_sync_task(TRAPS) {
  DebugUtils::assert_thread_nonjava_or_in_native();

  JClientProgramData* pd = ss().session_data()->program_data();

  JB_RETURN(request_client_cache(MessageType::CacheClassLoaderResource, pd->clr_cache_state()));
  JB_RETURN(request_client_cache(MessageType::CacheAggressiveCDS,       pd->cds_cache_state()));

  JB_RETURN(ss().send_request(MessageType::EndOfCurrentPhase));
  return 0;
}

int ServerMessageHandler::handle_lazy_aot_compilation_task(TRAPS) {
  DebugUtils::assert_thread_in_native();

  JClientProgramData* pd = ss().session_data()->program_data();
  JClientCacheState& aot_cache_state = pd->aot_cache_state();
  ResourceMark rm(THREAD);
  GrowableArray<InstanceKlass*> klasses_to_compile;
  GrowableArray<Method*>        methods_to_compile;
  GrowableArray<Method*>        methods_not_compile;
  bool compile_in_current_thread = false;

  JB_TRY {
    compile_in_current_thread = !aot_cache_state.is_cached() && aot_cache_state.set_being_generated();
    JB_THROW(ss().send_response(&compile_in_current_thread));
    if (compile_in_current_thread) {
      JB_THROW(request_missing_class_loaders(THREAD));
      JB_THROW(request_missing_klasses(THREAD));
      JB_THROW(request_methods_to_compile(&klasses_to_compile, &methods_to_compile, THREAD));
      if (pd->using_pgo()) {
        JB_THROW(request_methods_not_compile(&methods_not_compile, THREAD));
        JB_THROW(request_method_data(THREAD));
      }
      JB_THROW(ss().send_request(MessageType::EndOfCurrentPhase));
    }
  } JB_TRY_END JB_CATCH_REST() {
    if (compile_in_current_thread) {
      aot_cache_state.set_not_generated();
    }
    return JB_ERR;
  } JB_CATCH_END;

  if (compile_in_current_thread) {
    JB_RETURN(try_to_compile_lazy_aot(&klasses_to_compile,
                                      &methods_to_compile,
                                      &methods_not_compile,
                                      pd->using_pgo(),
                                      THREAD));
  } else {  // not compile in current thread
    if (aot_cache_state.is_being_generated()) {
      log_info(jbooster, compilation)("Skippd as this program is being compiled. session_id=%u.",
                                      ss().session_id());
    } else if (aot_cache_state.is_generated()) {
      log_info(jbooster, compilation)("Skippd as this program has been compiled. session_id=%u.",
                                      ss().session_id());
    } else {
      log_error(jbooster, compilation)("Unknown compile state. session_id=%u.",
                                       ss().session_id());
    }
  }
  guarantee(!(compile_in_current_thread && aot_cache_state.is_being_generated()), "some logic missing?");
  return 0;
}

int ServerMessageHandler::try_to_compile_lazy_aot(GrowableArray<InstanceKlass*>* klasses_to_compile,
                                                  GrowableArray<Method*>* methods_to_compile,
                                                  GrowableArray<Method*>* methods_not_compile,
                                                  bool use_pgo,
                                                  TRAPS) {
  JClientProgramData* pd = ss().session_data()->program_data();
  JClientCacheState& aot_cache_state = pd->aot_cache_state();
  if (klasses_to_compile->is_empty()) {
    aot_cache_state.set_not_generated();
    log_error(jbooster, compilation)("Failed to compile as the compilation list is empty. session_id=%u.",
                                      ss().session_id());
    return 0;
  }

  bool successful;
  int session_id = ss().session_id();
  const char* file_path = aot_cache_state.file_path();

  ThreadInVMfromNative tiv(THREAD);
  if (methods_to_compile->is_empty()) {
    successful = LazyAOT::compile_classes_by_graal(session_id, file_path, klasses_to_compile, use_pgo, THREAD);
  } else {
    successful = LazyAOT::compile_methods_by_graal(session_id, file_path, klasses_to_compile,
                                                   methods_to_compile, methods_not_compile, use_pgo, THREAD);
  }

  if (successful) {
    guarantee(!HAS_PENDING_EXCEPTION, "sanity");
    chmod(file_path, S_IREAD);
    aot_cache_state.set_generated();
    log_info(jbooster, compilation)("Successfully comiled %d classes. session_id=%u.",
                                    klasses_to_compile->length(),
                                    ss().session_id());
  } else if (HAS_PENDING_EXCEPTION) {
    aot_cache_state.set_not_generated();
    LogTarget(Error, jbooster, compilation) lt;
    if (lt.is_enabled()) {
      lt.print("Failed to compile %d classes because:", klasses_to_compile->length());
    }
    DebugUtils::clear_java_exception_and_print_stack_trace(lt, THREAD);
    if (lt.is_enabled()) {
      lt.print("session_id=%u.", ss().session_id());
    }
  } else {
    aot_cache_state.set_not_generated();
    log_error(jbooster, compilation)("Failed to compile %d classes. session_id=%u.",
                                      klasses_to_compile->length(),
                                      ss().session_id());
  }

  return 0;
}
