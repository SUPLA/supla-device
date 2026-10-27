// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_MBEDTLS_SSL_H_
#define EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_MBEDTLS_SSL_H_
#include <stddef.h>
#include <stdint.h>
#include "x509_crt.h"
#define MBEDTLS_SSL_IS_CLIENT 0
#define MBEDTLS_SSL_TRANSPORT_STREAM 0
#define MBEDTLS_SSL_PRESET_DEFAULT 0
#define MBEDTLS_SSL_VERIFY_REQUIRED 1
#define MBEDTLS_SSL_VERIFY_NONE 0
#define MBEDTLS_ERR_SSL_WANT_READ -0x6900
#define MBEDTLS_ERR_SSL_WANT_WRITE -0x6880
#define MBEDTLS_ERR_SSL_INTERNAL_ERROR -0x6c00
struct mbedtls_ssl_context {};
struct mbedtls_ssl_config {};
extern int fakeHandshakeResult;
extern bool fakeReadFailure, fakeWriteFailure;
void recordTlsFree();
inline void mbedtls_ssl_init(mbedtls_ssl_context *) {}
inline void mbedtls_ssl_config_init(mbedtls_ssl_config *) {}
inline void mbedtls_ssl_config_free(mbedtls_ssl_config *) {}
inline void mbedtls_ssl_free(mbedtls_ssl_context *) { recordTlsFree(); }
inline int mbedtls_ssl_config_defaults(mbedtls_ssl_config *, int, int, int) {
  return 0;
}
inline void mbedtls_ssl_conf_rng(mbedtls_ssl_config *,
    int (*)(void *, unsigned char *, size_t), void *) {}
inline void mbedtls_ssl_conf_ca_chain(mbedtls_ssl_config *,
    mbedtls_x509_crt *, void *) {}
inline void mbedtls_ssl_conf_authmode(mbedtls_ssl_config *, int) {}
inline int mbedtls_ssl_setup(mbedtls_ssl_context *, mbedtls_ssl_config *) {
  return 0;
}
inline int mbedtls_ssl_set_hostname(mbedtls_ssl_context *, const char *) {
  return 0;
}
inline void mbedtls_ssl_set_bio(mbedtls_ssl_context *, void *,
    int (*)(void *, const unsigned char *, size_t),
    int (*)(void *, unsigned char *, size_t), void *) {}
inline int mbedtls_ssl_handshake(mbedtls_ssl_context *) {
  return fakeHandshakeResult;
}
inline uint32_t mbedtls_ssl_get_verify_result(const mbedtls_ssl_context *) {
  return 0;
}
inline size_t mbedtls_ssl_get_bytes_avail(const mbedtls_ssl_context *) {
  return 0;
}
inline int mbedtls_ssl_read(mbedtls_ssl_context *, unsigned char *, size_t) {
  return fakeReadFailure ? 0 : MBEDTLS_ERR_SSL_WANT_READ;
}
inline int mbedtls_ssl_write(mbedtls_ssl_context *,
                            const unsigned char *, size_t size) {
  return fakeWriteFailure ? 0 : static_cast<int>(size);
}
#endif  // EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_MBEDTLS_SSL_H_
