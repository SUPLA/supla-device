// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ESP_IDF_ESP_PARTITION_H_
#define EXTRAS_TEST_DOUBLES_ESP_IDF_ESP_PARTITION_H_

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct esp_partition_t {
  int subtype;
  uint32_t address;
} esp_partition_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_partition_read(const esp_partition_t *partition,
                             size_t srcOffset,
                             void *destination,
                             size_t size);

#ifdef __cplusplus
}
#endif

#endif  // EXTRAS_TEST_DOUBLES_ESP_IDF_ESP_PARTITION_H_
