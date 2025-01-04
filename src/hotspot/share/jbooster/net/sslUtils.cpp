/*
 * Copyright (c) 2020, 2024, Huawei Technologies Co., Ltd. All rights reserved.
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

#include "logging/log.hpp"
#include "jbooster/net/sslUtils.hpp"
#include "runtime/java.hpp"
#include <dlfcn.h>
#include <string.h>

void* SSLUtils::_lib_handle = nullptr;
enum SSLUtils::SSLVersion SSLUtils::_version = V0;
const char* SSLUtils::OPENSSL_VERSION_1_0 = "OpenSSL 1.0.";
const char* SSLUtils::OPENSSL_VERSION_1_1 = "OpenSSL 1.1.";
const char* SSLUtils::OPENSSL_VERSION_3_X = "OpenSSL 3.";

SSLUtils::openssl_version_func_t                SSLUtils::_openssl_version               = nullptr;
SSLUtils::openssl_init_ssl_func_t               SSLUtils::_openssl_init_ssl              = nullptr;
SSLUtils::ssl_ctx_new_func_t                    SSLUtils::_ssl_ctx_new                   = nullptr;
SSLUtils::sslv23_client_method_func_t           SSLUtils::_sslv23_client_method          = nullptr;
SSLUtils::sslv23_server_method_func_t           SSLUtils::_sslv23_server_method          = nullptr;
SSLUtils::ssl_ctx_ctrl_func_t                   SSLUtils::_ssl_ctx_ctrl                  = nullptr;
SSLUtils::ssl_ctx_set_options_func_t            SSLUtils::_ssl_ctx_set_options           = nullptr;
SSLUtils::ssl_ctx_set_cipher_list_func_t        SSLUtils::_ssl_ctx_set_cipher_list       = nullptr;
SSLUtils::ssl_ctx_set_ciphersuites_func_t       SSLUtils::_ssl_ctx_set_ciphersuites      = nullptr;
SSLUtils::ssl_ctx_set_verify_func_t             SSLUtils::_ssl_ctx_set_verify            = nullptr;
SSLUtils::ssl_ctx_load_verify_locations_func_t  SSLUtils::_ssl_ctx_load_verify_locations = nullptr;
SSLUtils::ssl_ctx_use_certificate_file_func_t   SSLUtils::_ssl_ctx_use_certificate_file  = nullptr;
SSLUtils::ssl_ctx_use_privatekey_file_func_t    SSLUtils::_ssl_ctx_use_privatekey_file   = nullptr;
SSLUtils::ssl_ctx_check_private_key_func_t      SSLUtils::_ssl_ctx_check_private_key     = nullptr;
SSLUtils::ssl_ctx_set_security_level_func_t     SSLUtils::_ssl_ctx_set_security_level    = nullptr;
SSLUtils::ssl_new_func_t                        SSLUtils::_ssl_new                       = nullptr;
SSLUtils::ssl_set_fd_func_t                     SSLUtils::_ssl_set_fd                    = nullptr;
SSLUtils::ssl_get_error_func_t                  SSLUtils::_ssl_get_error                 = nullptr;
SSLUtils::ssl_accept_func_t                     SSLUtils::_ssl_accept                    = nullptr;
SSLUtils::ssl_connect_func_t                    SSLUtils::_ssl_connect                   = nullptr;
SSLUtils::ssl_read_func_t                       SSLUtils::_ssl_read                      = nullptr;
SSLUtils::ssl_write_func_t                      SSLUtils::_ssl_write                     = nullptr;
SSLUtils::ssl_shutdown_func_t                   SSLUtils::_ssl_shutdown                  = nullptr;
SSLUtils::ssl_free_func_t                       SSLUtils::_ssl_free                      = nullptr;
SSLUtils::ssl_ctx_free_func_t                   SSLUtils::_ssl_ctx_free                  = nullptr;
SSLUtils::ssl_get_peer_certificate_func_t       SSLUtils::_ssl_get_peer_certificate      = nullptr;
SSLUtils::ssl_get_verify_result_func_t          SSLUtils::_ssl_get_verify_result         = nullptr;
SSLUtils::err_peak_error_func_t                 SSLUtils::_err_peak_error                = nullptr;
SSLUtils::err_error_string_n_func_t             SSLUtils::_err_error_string_n            = nullptr;
SSLUtils::err_get_error_func_t                  SSLUtils::_err_get_error                 = nullptr;
SSLUtils::err_print_errors_fp_func_t            SSLUtils::_err_print_errors_fp           = nullptr;

static void* open_ssl_lib() {
  const char* const lib_names[] = {
    "libssl.so.3",
    "libssl.so.1.1",
    "libssl.so.1.0.0",
    "libssl.so.10",
    "libssl.so"
  };
  const int lib_names_len = sizeof(lib_names) / sizeof(lib_names[0]);

  void* res = nullptr;
  for (int i = 0; i < lib_names_len; ++i) {
    res = ::dlopen(lib_names[i], RTLD_NOW);
    if (res != nullptr) {
      break;
    }
  }
  return res;
}

void SSLUtils::find_ssl_version() {
  const char* openssl_version_res = nullptr;
  _openssl_version = (openssl_version_func_t)::dlsym(_lib_handle, "OpenSSL_version");
  if (_openssl_version) {
      openssl_version_res = openssl_version();
      if (0 == strncmp(openssl_version_res, OPENSSL_VERSION_1_1, strlen(OPENSSL_VERSION_1_1))) {
        _version = V1_1;
      } else if (0 == strncmp(openssl_version_res, OPENSSL_VERSION_3_X, strlen(OPENSSL_VERSION_3_X))) {
        _version = V3;
      }
  } else {
      _openssl_version = (openssl_version_func_t) ::dlsym(_lib_handle, "SSLeay_version");
      if (_openssl_version) {
      openssl_version_res = openssl_version();
      if (0 == strncmp(openssl_version_res, OPENSSL_VERSION_1_0, strlen(OPENSSL_VERSION_1_0))) {
        _version = V1_0;
      }
    }
  }
}

void SSLUtils::find_address_in_dynamic_lib() {
  if (_version == V3) {
    _ssl_get_peer_certificate = (ssl_get_peer_certificate_func_t) ::dlsym(_lib_handle, "SSL_get1_peer_certificate");
  } else {
    _ssl_get_peer_certificate = (ssl_get_peer_certificate_func_t) ::dlsym(_lib_handle, "SSL_get_peer_certificate");
  }

  _openssl_init_ssl              = (openssl_init_ssl_func_t)              ::dlsym(_lib_handle, "OPENSSL_init_ssl");
  _sslv23_client_method          = (sslv23_client_method_func_t)          ::dlsym(_lib_handle, "TLS_client_method");
  _sslv23_server_method          = (sslv23_server_method_func_t)          ::dlsym(_lib_handle, "TLS_server_method");
  _ssl_ctx_new                   = (ssl_ctx_new_func_t)                   ::dlsym(_lib_handle, "SSL_CTX_new");
  _ssl_ctx_ctrl                  = (ssl_ctx_ctrl_func_t)                  ::dlsym(_lib_handle, "SSL_CTX_ctrl");
  _ssl_ctx_set_options           = (ssl_ctx_set_options_func_t)           ::dlsym(_lib_handle, "SSL_CTX_set_options");
  _ssl_ctx_set_cipher_list       = (ssl_ctx_set_cipher_list_func_t)       ::dlsym(_lib_handle, "SSL_CTX_set_cipher_list");
  _ssl_ctx_set_ciphersuites      = (ssl_ctx_set_ciphersuites_func_t)      ::dlsym(_lib_handle, "SSL_CTX_set_ciphersuites");
  _ssl_ctx_set_verify            = (ssl_ctx_set_verify_func_t)            ::dlsym(_lib_handle, "SSL_CTX_set_verify");
  _ssl_ctx_load_verify_locations = (ssl_ctx_load_verify_locations_func_t) ::dlsym(_lib_handle, "SSL_CTX_load_verify_locations");
  _ssl_ctx_use_certificate_file  = (ssl_ctx_use_certificate_file_func_t)  ::dlsym(_lib_handle, "SSL_CTX_use_certificate_file");
  _ssl_ctx_use_privatekey_file   = (ssl_ctx_use_privatekey_file_func_t)   ::dlsym(_lib_handle, "SSL_CTX_use_PrivateKey_file");
  _ssl_ctx_check_private_key     = (ssl_ctx_check_private_key_func_t)     ::dlsym(_lib_handle, "SSL_CTX_check_private_key");
  _ssl_ctx_set_security_level    = (ssl_ctx_set_security_level_func_t)    ::dlsym(_lib_handle, "SSL_CTX_set_security_level");
  _ssl_new                       = (ssl_new_func_t)                       ::dlsym(_lib_handle, "SSL_new");
  _ssl_set_fd                    = (ssl_set_fd_func_t)                    ::dlsym(_lib_handle, "SSL_set_fd");
  _ssl_get_error                 = (ssl_get_error_func_t)                 ::dlsym(_lib_handle, "SSL_get_error");
  _ssl_accept                    = (ssl_accept_func_t)                    ::dlsym(_lib_handle, "SSL_accept");
  _ssl_connect                   = (ssl_connect_func_t)                   ::dlsym(_lib_handle, "SSL_connect");
  _ssl_read                      = (ssl_read_func_t)                      ::dlsym(_lib_handle, "SSL_read");
  _ssl_write                     = (ssl_write_func_t)                     ::dlsym(_lib_handle, "SSL_write");
  _ssl_shutdown                  = (ssl_shutdown_func_t)                  ::dlsym(_lib_handle, "SSL_shutdown");
  _ssl_free                      = (ssl_free_func_t)                      ::dlsym(_lib_handle, "SSL_free");
  _ssl_ctx_free                  = (ssl_ctx_free_func_t)                  ::dlsym(_lib_handle, "SSL_CTX_free");
  _ssl_get_verify_result         = (ssl_get_verify_result_func_t)         ::dlsym(_lib_handle, "SSL_get_verify_result");
  _err_peak_error                = (err_peak_error_func_t)                ::dlsym(_lib_handle, "ERR_peek_error");
  _err_error_string_n            = (err_error_string_n_func_t)            ::dlsym(_lib_handle, "ERR_error_string_n");
  _err_get_error                 = (err_get_error_func_t)                 ::dlsym(_lib_handle, "ERR_get_error");
  _err_print_errors_fp           = (err_print_errors_fp_func_t)           ::dlsym(_lib_handle, "ERR_print_errors_fp");
}

bool SSLUtils::check_if_find_all_functions() {
  if (
    (_openssl_version == nullptr) ||
    (_openssl_init_ssl == nullptr) ||
    (_ssl_ctx_new == nullptr) ||
    (_sslv23_client_method == nullptr) ||
    (_sslv23_server_method == nullptr) ||
    (_ssl_ctx_ctrl == nullptr) ||
    (_ssl_ctx_set_options == nullptr) ||
    (_ssl_ctx_set_cipher_list == nullptr) ||
    (_ssl_ctx_set_ciphersuites == nullptr) ||
    (_ssl_ctx_set_verify == nullptr) ||
    (_ssl_ctx_load_verify_locations == nullptr) ||
    (_ssl_ctx_use_certificate_file == nullptr) ||
    (_ssl_ctx_use_privatekey_file == nullptr) ||
    (_ssl_ctx_check_private_key == nullptr) ||
    (_ssl_ctx_set_security_level == nullptr) ||
    (_ssl_new == nullptr) ||
    (_ssl_set_fd == nullptr) ||
    (_ssl_get_error == nullptr) ||
    (_ssl_accept == nullptr) ||
    (_ssl_connect == nullptr) ||
    (_ssl_read == nullptr) ||
    (_ssl_write == nullptr) ||
    (_ssl_shutdown == nullptr) ||
    (_ssl_free == nullptr) ||
    (_ssl_ctx_free == nullptr) ||
    (_ssl_get_peer_certificate == nullptr) ||
    (_ssl_get_verify_result == nullptr) ||
    (_err_peak_error == nullptr) ||
    (_err_error_string_n == nullptr) ||
    (_err_get_error == nullptr) ||
    (_err_print_errors_fp == nullptr)
  ) { return false; }
  return true;
}

bool SSLUtils::init_ssl_lib() {
  _lib_handle = open_ssl_lib();
  if (_lib_handle == nullptr) {
    return false;
  }

  find_ssl_version();
  if (_version == V0) {
    return false;
  } else if (_version == V1_0) {
    log_error(jbooster, rpc)("JBooster only supports OpenSSL 1.1.0+.");
    return false;
  }

  find_address_in_dynamic_lib();
  return check_if_find_all_functions();
}

void SSLUtils::shutdown_and_free_ssl(SSL*& ssl) {
  if (ssl != nullptr) {
    SSLUtils::ssl_shutdown(ssl);
    SSLUtils::ssl_free(ssl);
    ssl = nullptr;
  }
}

void SSLUtils::handle_ssl_ctx_error(const char* error_description) {
  log_error(jbooster, rpc)("%s", error_description);
  if (log_is_enabled(Error, jbooster, rpc)) {
    SSLUtils::err_print_errors_fp(stderr);
  }
  vm_exit(1);
}

void SSLUtils::handle_ssl_error(SSL* ssl, int ret, const char* error_description) {
  const char* errno_copy = strerror(errno);
  int error = SSLUtils::ssl_get_error(ssl, ret);
  unsigned long earliest_error = SSLUtils::err_peak_error();

  char error_string[256] = {0};
  SSLUtils::err_error_string_n(earliest_error, error_string, sizeof(error_string));

  log_error(jbooster, rpc)("%s! Error Code: %d, Earliest Error: %lu, Error String: %s, Errno: %s",
                            error_description, error, earliest_error, error_string, errno_copy);
  if (log_is_enabled(Error, jbooster, rpc)) {
    SSLUtils::err_print_errors_fp(stderr);
  }
}