// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_PARTITION_H_
#define EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_PARTITION_H_
#include <nvs.h>
using esp_partition_type_t = int;
using esp_partition_subtype_t = int;
const int ESP_PARTITION_TYPE_DATA = 1;
const int ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS = 4;
struct esp_partition_t {
  size_t size;
};
inline const esp_partition_t *esp_partition_find_first(...) { return nullptr; }
inline esp_err_t esp_partition_read(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t esp_partition_write(...) { return ESP_ERR_NOT_FOUND; }
inline esp_err_t esp_partition_erase_range(...) { return ESP_ERR_NOT_FOUND; }
#endif  // EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_PARTITION_H_
