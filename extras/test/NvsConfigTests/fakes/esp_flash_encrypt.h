// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_FLASH_ENCRYPT_H_
#define EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_FLASH_ENCRYPT_H_
using esp_flash_enc_mode_t = int;
const int ESP_FLASH_ENC_MODE_RELEASE = 1;
inline esp_flash_enc_mode_t esp_get_flash_encryption_mode() { return 0; }
#endif  // EXTRAS_TEST_NVSCONFIGTESTS_FAKES_ESP_FLASH_ENCRYPT_H_
