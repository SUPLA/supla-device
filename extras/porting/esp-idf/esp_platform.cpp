// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <esp_system.h>
#include <esp_random.h>
#include <esp_chip_info.h>
#include <supla/tools.h>

void deviceSoftwareReset() {
  esp_restart();
}

bool isDeviceSoftwareResetSupported() {
  return true;
}

bool isLastResetSoft() {
  return esp_reset_reason() == ESP_RST_SW;
}

bool Supla::isLastResetPower() {
  return esp_reset_reason() == ESP_RST_POWERON;
}

int Supla::getPlatformId() {
  esp_chip_info_t chipInfo = {};
  esp_chip_info(&chipInfo);
  switch (chipInfo.model) {
    case CHIP_ESP32:
      return 2;
    case CHIP_ESP32S2:
      return 3;
    case CHIP_ESP32S3:
      return 4;
    case CHIP_ESP32C3:
      return 5;
    case CHIP_ESP32H2:
      return 6;
    case CHIP_ESP32C2:
      return 7;
    case CHIP_ESP32C6:
      return 8;
    case CHIP_ESP32P4:
      return 9;
    case CHIP_ESP32C61:
      return 10;
    case CHIP_ESP32C5:
      return 11;
    default:
      return 0;
  }
}

void Supla::fillRandom(uint8_t *buffer, int size) {
  esp_fill_random(buffer, size);
}
