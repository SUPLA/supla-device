// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32)

#include "esp_idf_mutex.h"

Supla::Mutex *Supla::Mutex::Create() {
  return new Supla::EspIdfMutex;
}

Supla::EspIdfMutex::~EspIdfMutex() {
  unlock();
}

Supla::EspIdfMutex::EspIdfMutex() {
  mutex = xSemaphoreCreateMutex();
  // there is some issue with mutex creation and its default state when two
  // tasks try to use them. As a workaround we take and give mutex back and
  // after such calls it works always as expected.
  xSemaphoreTake(mutex, portMAX_DELAY);
  xSemaphoreGive(mutex);
}

void Supla::EspIdfMutex::lock() {
  xSemaphoreTake(mutex, portMAX_DELAY);
}

void Supla::EspIdfMutex::unlock() {
  xSemaphoreGive(mutex);
}

#endif
