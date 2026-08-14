// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ESP_IDF_ESP_OTA_OPS_H_
#define EXTRAS_TEST_DOUBLES_ESP_IDF_ESP_OTA_OPS_H_

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_partition.h"

typedef uint32_t esp_ota_handle_t;

#define OTA_WITH_SEQUENTIAL_WRITES 0

#ifdef __cplusplus
extern "C" {
#endif

const esp_partition_t *esp_ota_get_next_update_partition(
    const esp_partition_t *startFrom);
esp_err_t esp_ota_begin(const esp_partition_t *partition,
                        size_t imageSize,
                        esp_ota_handle_t *outHandle);
esp_err_t esp_ota_write(esp_ota_handle_t handle, const void *data, size_t size);
esp_err_t esp_ota_end(esp_ota_handle_t handle);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition);

#ifdef __cplusplus
}
#endif

#endif  // EXTRAS_TEST_DOUBLES_ESP_IDF_ESP_OTA_OPS_H_
