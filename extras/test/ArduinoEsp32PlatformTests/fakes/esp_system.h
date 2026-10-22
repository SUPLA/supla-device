// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_ESP_SYSTEM_H_
#define EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_ESP_SYSTEM_H_

#include <stdint.h>
constexpr int ESP_RST_SW = 1;
constexpr int ESP_RST_POWERON = 2;
inline int esp_reset_reason() { return ESP_RST_SW; }
inline void esp_fill_random(uint8_t *, int) {}

#endif  // EXTRAS_TEST_ARDUINOESP32PLATFORMTESTS_FAKES_ESP_SYSTEM_H_
