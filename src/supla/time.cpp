// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "time.h"

#ifndef ARDUINO

#ifdef SUPLA_FREERTOS
// Plain FreeRTOS compilation
#include <FreeRTOS.h>
#include <task.h>

uint32_t millis(void) {
  if (portTICK_PERIOD_MS != 1) {
    // TODO(klew): implement
    // error
  }
  return xTaskGetTickCount();
}

void delay(uint64_t delayMs) {
// TODO(klew):  usleep(delayMs * 1000);
}

void delayMicroseconds(uint64_t delayMicro) {
// TODO(klew): usleep(delayMicro);
}

#elif defined(ESP_PLATFORM)
// ESP-IDF compilation
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unistd.h>
#include <esp_timer.h>

uint32_t millis(void) {
  return static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL;
}

void delay(uint64_t delayMs) {
  usleep(delayMs * 1000);
}

void delayMicroseconds(uint64_t delayMicro) {
  usleep(delayMicro);
}

#elif SUPLA_LINUX
#include <chrono>  // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)

std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

uint32_t millis() {
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - begin)
    .count();
}

void delay(uint64_t v) {
  std::this_thread::sleep_for(std::chrono::milliseconds(v));
}

void delayMicroseconds(uint64_t v) {
  std::this_thread::sleep_for(std::chrono::microseconds(v));
}

#elif SUPLA_TEST
// skip - we use mocks instead
#else
#error "Please implement time functions for current target"
#endif

#endif
