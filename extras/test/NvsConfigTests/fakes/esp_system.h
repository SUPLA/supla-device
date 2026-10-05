// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_SYSTEM_H_
#define EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_SYSTEM_H_
#include <nvs.h>
inline const char *esp_err_to_name(esp_err_t) { return "fake ESP error"; }
#endif  // EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_SYSTEM_H_
