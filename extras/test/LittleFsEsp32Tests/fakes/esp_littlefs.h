// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_LITTLEFSESP32TESTS_FAKES_ESP_LITTLEFS_H_
#define EXTRAS_TEST_LITTLEFSESP32TESTS_FAKES_ESP_LITTLEFS_H_
#include <LittleFS.h>
#include <cstring>

inline const char *lastLittleFsLabel = nullptr;
inline bool esp_littlefs_mounted(const char *label) {
  lastLittleFsLabel = label;
  // Arduino uses the named SPIFFS partition. NULL selects another lookup.
  return label && !std::strcmp(label, "spiffs") && LittleFS.mounted;
}
#endif  // EXTRAS_TEST_LITTLEFSESP32TESTS_FAKES_ESP_LITTLEFS_H_
