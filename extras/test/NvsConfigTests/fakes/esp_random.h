// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_RANDOM_H_
#define EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_RANDOM_H_
#include <supla/tools.h>
inline void esp_fill_random(void *data, size_t size) {
  Supla::fillRandom(static_cast<uint8_t *>(data), static_cast<int>(size));
}
#endif  // EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_RANDOM_H_
