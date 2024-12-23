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

#ifndef SHARE_JBOOSTER_UTILITIES_SSLUtils_HPP
#define SHARE_JBOOSTER_UTILITIES_SSLUtils_HPP

#include "memory/allocation.hpp"
#include <openssl/ssl.h>

class SSLUtils : AllStatic {
  typedef const char*   (*openssl_version_func_t)(int num);
  typedef int           (*openssl_init_ssl_func_t)(uint64_t opts, const void* settings);
  typedef SSL_CTX*      (*ssl_ctx_new_func_t)(const SSL_METHOD* meth);
  typedef SSL_METHOD*   (*sslv23_client_method_func_t)(void);
  typedef SSL_METHOD*   (*sslv23_server_method_func_t)(void);
  typedef long          (*ssl_ctx_ctrl_func_t)(SSL_CTX* ctx, int cmd, long larg, void* parg);
  typedef uint64_t      (*ssl_ctx_set_options_func_t)(SSL_CTX* ctx, uint64_t options);
  typedef int           (*ssl_ctx_set_cipher_list_func_t)(SSL_CTX* ctx, const char* str);
  typedef int           (*ssl_ctx_set_ciphersuites_func_t)(SSL_CTX* ctx, const char* str);
  typedef void          (*ssl_ctx_set_verify_func_t)(SSL_CTX* ctx, int mode, SSL_verify_cb callback);
  typedef int           (*ssl_ctx_load_verify_locations_func_t)(SSL_CTX* ctx, const char* CAfile, const char* CApath);
  typedef int           (*ssl_ctx_use_certificate_file_func_t)(SSL_CTX* ctx, const char* file, int type);
  typedef int           (*ssl_ctx_use_privatekey_file_func_t)(SSL_CTX* ctx, const char* file, int type);
  typedef int           (*ssl_ctx_check_private_key_func_t)(const SSL_CTX* ctx);
  typedef void          (*ssl_ctx_set_security_level_func_t)(SSL_CTX *ctx, int level);
  typedef SSL*          (*ssl_new_func_t)(SSL_CTX* ctx);
  typedef int           (*ssl_set_fd_func_t)(SSL* s, int fd);
  typedef int           (*ssl_get_error_func_t)(const SSL* s, int ret_code);
  typedef int           (*ssl_accept_func_t)(const SSL* ssl);
  typedef int           (*ssl_connect_func_t)(const SSL* ssl);
  typedef int           (*ssl_read_func_t)(SSL* ssl, void* buf, int num);
  typedef int           (*ssl_write_func_t)(SSL* ssl, const void* buf, int num);
  typedef int           (*ssl_shutdown_func_t)(SSL* ssl);
  typedef void          (*ssl_free_func_t)(SSL* ssl);
  typedef void          (*ssl_ctx_free_func_t)(SSL_CTX* ctx);
  typedef X509*         (*ssl_get_peer_certificate_func_t)(const SSL* ssl);
  typedef long          (*ssl_get_verify_result_func_t)(const SSL* ssl);
  typedef unsigned long (*err_peak_error_func_t)(void);
  typedef void          (*err_error_string_n_func_t)(unsigned long e, char *buf, size_t len);
  typedef unsigned long (*err_get_error_func_t)(void);
  typedef void          (*err_print_errors_fp_func_t)(FILE *fp);

  static enum SSLVersion {
    V0, V1_0, V1_1, V3
  } _version;

  static void* _lib_handle;
  static const char* OPENSSL_VERSION_1_0;
  static const char* OPENSSL_VERSION_1_1;
  static const char* OPENSSL_VERSION_3_X;

  static openssl_version_func_t               _openssl_version;
  static openssl_init_ssl_func_t              _openssl_init_ssl;
  static ssl_ctx_new_func_t                   _ssl_ctx_new;
  static sslv23_client_method_func_t          _sslv23_client_method;
  static sslv23_server_method_func_t          _sslv23_server_method;
  static ssl_ctx_ctrl_func_t                  _ssl_ctx_ctrl;
  static ssl_ctx_set_options_func_t           _ssl_ctx_set_options;
  static ssl_ctx_set_cipher_list_func_t       _ssl_ctx_set_cipher_list;
  static ssl_ctx_set_ciphersuites_func_t      _ssl_ctx_set_ciphersuites;
  static ssl_ctx_set_verify_func_t            _ssl_ctx_set_verify;
  static ssl_ctx_load_verify_locations_func_t _ssl_ctx_load_verify_locations;
  static ssl_ctx_use_certificate_file_func_t  _ssl_ctx_use_certificate_file;
  static ssl_ctx_use_privatekey_file_func_t   _ssl_ctx_use_privatekey_file;
  static ssl_ctx_check_private_key_func_t     _ssl_ctx_check_private_key;
  static ssl_ctx_set_security_level_func_t    _ssl_ctx_set_security_level;
  static ssl_new_func_t                       _ssl_new;
  static ssl_set_fd_func_t                    _ssl_set_fd;
  static ssl_get_error_func_t                 _ssl_get_error;
  static ssl_accept_func_t                    _ssl_accept;
  static ssl_connect_func_t                   _ssl_connect;
  static ssl_read_func_t                      _ssl_read;
  static ssl_write_func_t                     _ssl_write;
  static ssl_shutdown_func_t                  _ssl_shutdown;
  static ssl_free_func_t                      _ssl_free;
  static ssl_ctx_free_func_t                  _ssl_ctx_free;
  static ssl_get_peer_certificate_func_t      _ssl_get_peer_certificate;
  static ssl_get_verify_result_func_t         _ssl_get_verify_result;
  static err_peak_error_func_t                _err_peak_error;
  static err_error_string_n_func_t            _err_error_string_n;
  static err_get_error_func_t                 _err_get_error;
  static err_print_errors_fp_func_t           _err_print_errors_fp;

public:
  static bool init_ssl_lib();
  static void find_ssl_version();
  static void find_address_in_dynamic_lib();
  static bool check_if_find_all_functions();

  static const char* openssl_version() { return (*_openssl_version)(0); }

  static int openssl_init_ssl() { return (*_openssl_init_ssl)(0, NULL); }
  static SSL_CTX* ssl_ctx_new(const SSL_METHOD* meth) { return (*_ssl_ctx_new)(meth); }
  static SSL_METHOD* sslv23_client_method() { return (*_sslv23_client_method)(); }
  static SSL_METHOD* sslv23_server_method() { return (*_sslv23_server_method)(); }
  static long ssl_ctx_ctrl(SSL_CTX* ctx, int cmd, long larg, void* parg) { return (*_ssl_ctx_ctrl)(ctx, cmd, larg, parg); }
  static uint64_t ssl_ctx_set_options(SSL_CTX* ctx, uint64_t options) { return (*_ssl_ctx_set_options)(ctx, options); };
  static int ssl_ctx_set_cipher_list(SSL_CTX* ctx, const char* str) { return (*_ssl_ctx_set_cipher_list)(ctx, str); };
  static int ssl_ctx_set_ciphersuites(SSL_CTX* ctx, const char* str) { return (*_ssl_ctx_set_ciphersuites)(ctx, str); };
  static void ssl_ctx_set_verify(SSL_CTX* ctx, int mode, SSL_verify_cb callback) { return (*_ssl_ctx_set_verify)(ctx, mode, callback); }
  static int ssl_ctx_load_verify_locations(SSL_CTX* ctx, const char* CAfile, const char* CApath) { return (*_ssl_ctx_load_verify_locations)(ctx, CAfile, CApath); }
  static int ssl_ctx_use_certificate_file(SSL_CTX* ctx, const char* file, int type) { return (*_ssl_ctx_use_certificate_file)(ctx, file, type); }
  static int ssl_ctx_use_privatekey_file(SSL_CTX* ctx, const char* file, int type) { return (*_ssl_ctx_use_privatekey_file)(ctx, file, type); }
  static void ssl_ctx_set_security_level(SSL_CTX *ctx, int level) { return (*_ssl_ctx_set_security_level)(ctx, level); }
  static int ssl_ctx_check_private_key(const SSL_CTX* ctx) { return (*_ssl_ctx_check_private_key)(ctx); }
  static SSL* ssl_new(SSL_CTX* ctx) { return (*_ssl_new)(ctx); }
  static int ssl_set_fd(SSL* s, int fd) { return (*_ssl_set_fd)(s, fd); }
  static int ssl_get_error(const SSL* s, int ret_code) { return (*_ssl_get_error)(s, ret_code); }
  static int ssl_accept(const SSL* ssl) { return (*_ssl_accept)(ssl); }
  static int ssl_connect(const SSL* ssl) { return (*_ssl_connect)(ssl); }
  static int ssl_read(SSL* ssl, void* buf, int num) { return (*_ssl_read)(ssl, buf, num); }
  static int ssl_write(SSL* ssl, const void* buf, int num) { return (*_ssl_write)(ssl, buf, num); }
  static int ssl_shutdown(SSL* ssl) { return (*_ssl_shutdown)(ssl); }
  static void ssl_free(SSL* ssl) { return (*_ssl_free)(ssl); }
  static void ssl_ctx_free(SSL_CTX* ctx) { return (*_ssl_ctx_free)(ctx); }
  static X509* ssl_get_peer_certificate(const SSL* ssl) { return (*_ssl_get_peer_certificate)(ssl); }
  static long ssl_get_verify_result(const SSL* ssl) { return (*_ssl_get_verify_result)(ssl); }
  static unsigned long err_peak_error() { return (*_err_peak_error)(); }
  static void err_error_string_n(unsigned long e, char *buf, size_t len) { return (*_err_error_string_n)(e, buf, len); }
  static unsigned long err_get_error() { return (*_err_get_error)(); }
  static void err_print_errors_fp(FILE *fp) { return (*_err_print_errors_fp)(fp); };

  static void shutdown_and_free_ssl(SSL*& ssl);
  static void handle_ssl_ctx_error(const char* error_description);
  static void handle_ssl_error(SSL* ssl, int ret, const char* error_description);
};

#endif // SHARE_JBOOSTER_UTILITIES_SSLUtils_HPP
