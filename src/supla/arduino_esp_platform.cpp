// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#include "supla/network/esp_async_client.h"
#else
#include "supla/network/esp8266_async_client.h"
#endif
#include <supla/crypto.h>
#include "tools.h"


void deviceSoftwareReset() {
  ESP.restart();
}

bool isDeviceSoftwareResetSupported() {
  return true;
}

bool isLastResetSoft() {
#ifdef ARDUINO_ARCH_ESP8266
  rst_info *resetInfo = ESP.getResetInfoPtr();
  return resetInfo->reason == REASON_SOFT_RESTART;
#elif defined(ARDUINO_ARCH_ESP32)
  return esp_reset_reason() == ESP_RST_SW;
#else
  return false;
#endif
}

bool Supla::isLastResetPower() {
#ifdef ARDUINO_ARCH_ESP8266
  rst_info *resetInfo = ESP.getResetInfoPtr();
  return resetInfo->reason == REASON_DEFAULT_RST;
#elif defined(ARDUINO_ARCH_ESP32)
  return esp_reset_reason() == ESP_RST_POWERON;
#else
  return false;
#endif
}

Supla::Client *Supla::ClientBuilder() {
#ifdef ARDUINO_ARCH_ESP8266
  auto client = new Esp8266AsyncClient;
#else
  auto client = new EspAsyncClient;
#endif
  // Preserve the Arduino factory's timeout for both connection APIs.
  client->setTimeoutMs(3000);
  return client;
}

int Supla::getPlatformId() {
  // TODO(klew): do we need platfom id for Arduino based ESP SW?
  return 0;
}

void Supla::fillRandom(uint8_t *buffer, int size) {
  if (size >= 0) {
    Supla::Crypto::fillRandom(buffer, static_cast<size_t>(size));
  }
}

#endif
