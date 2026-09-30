// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef EXTRAS_TEST_SUPLANESPUDPTESTS_FAKES_ESP_NETIF_H_
#define EXTRAS_TEST_SUPLANESPUDPTESTS_FAKES_ESP_NETIF_H_

#include <stdint.h>
struct esp_netif_t { int index; };
struct esp_ip4_addr_t { uint32_t addr; };
struct esp_netif_ip_info_t { esp_ip4_addr_t ip; };
static const int ESP_OK = 0;
esp_netif_t *esp_netif_next_unsafe(esp_netif_t *netif);
bool esp_netif_is_netif_up(esp_netif_t *netif);
int esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *info);

#endif  // EXTRAS_TEST_SUPLANESPUDPTESTS_FAKES_ESP_NETIF_H_
