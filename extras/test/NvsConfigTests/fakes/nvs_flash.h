// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_NVSCONFIGTESTS_FAKES_NVS_FLASH_H_
#define EXTRAS_TEST_NVSCONFIGTESTS_FAKES_NVS_FLASH_H_
#include <nvs.h>
struct nvs_sec_cfg_t {};
struct nvs_sec_scheme_t {};
inline nvs_sec_scheme_t *nvs_flash_get_default_security_scheme() {
  return nullptr;
}
inline esp_err_t nvs_flash_read_security_cfg_v2(...) {
  return ESP_ERR_NOT_FOUND;
}
inline esp_err_t nvs_flash_init_partition(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_flash_deinit_partition(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_flash_erase_partition(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_flash_init() { return ESP_ERR_NOT_FOUND; }
inline esp_err_t nvs_flash_secure_init_partition(...) {
  return ESP_ERR_NOT_FOUND;
}
#endif  // EXTRAS_TEST_NVSCONFIGTESTS_FAKES_NVS_FLASH_H_
