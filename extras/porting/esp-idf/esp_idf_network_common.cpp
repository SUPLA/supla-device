// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_idf_network_common.h"

#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_event.h>

void Supla::initEspNetif() {
  static bool initDone = false;
  if (!initDone) {
//    nvs_flash_init();
    esp_netif_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    initDone = true;
  }
}
