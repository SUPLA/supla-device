// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_MBEDTLS_X509_CRT_H_
#define EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_MBEDTLS_X509_CRT_H_
#include <stddef.h>
#define MBEDTLS_X509_BADCERT_EXPIRED 1
#define MBEDTLS_X509_BADCERT_FUTURE 2
#define MBEDTLS_X509_BADCERT_CN_MISMATCH 4
#define MBEDTLS_X509_BADCERT_NOT_TRUSTED 8
#define MBEDTLS_ERR_X509_CERT_VERIFY_FAILED -0x2700
struct mbedtls_x509_crt {};
extern bool fakeParseFailure;
inline void mbedtls_x509_crt_init(mbedtls_x509_crt *) {}
inline void mbedtls_x509_crt_free(mbedtls_x509_crt *) {}
inline int mbedtls_x509_crt_parse(mbedtls_x509_crt *,
                                 const unsigned char *, size_t) {
  return fakeParseFailure ? -1 : 0;
}
#endif  // EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_MBEDTLS_X509_CRT_H_
