// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_ASYNCDNSTESTS_FAKES_LWIP_DNS_H_
#define EXTRAS_TEST_ASYNCDNSTESTS_FAKES_LWIP_DNS_H_
#include <stdint.h>
typedef int err_t;
struct ip_addr_t { uint32_t address; };
#define ERR_OK 0
#define ERR_MEM -1
#define ERR_INPROGRESS -5
#define IP_IS_V4(address) true
#define ip_2_ip4(address) (address)
#define ip4_addr_get_u32(value) ((value)->address)
using dns_found_callback = void (*)(const char *, const ip_addr_t *, void *);
err_t dns_gethostbyname(const char *, ip_addr_t *, dns_found_callback, void *);
#endif  // EXTRAS_TEST_ASYNCDNSTESTS_FAKES_LWIP_DNS_H_
