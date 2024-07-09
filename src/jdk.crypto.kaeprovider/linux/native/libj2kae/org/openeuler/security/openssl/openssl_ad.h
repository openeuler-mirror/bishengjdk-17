#include <openssl/opensslv.h>
#include <openssl/err.h>

#if defined(OPENSSL_VERSION_MAJOR) && OPENSSL_VERSION_MAJOR >= 3

// ERR_GET_FUNC() was removed since Openssl3
#ifndef ERR_GET_FUNC
#define ERR_GET_FUNC(e) (int)(((e) >> 12L) & 0xFFFL)
#endif
#endif
