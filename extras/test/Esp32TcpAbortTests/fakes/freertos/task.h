// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_FREERTOS_TASK_H_
#define EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_FREERTOS_TASK_H_

#include <freertos/FreeRTOS.h>

constexpr BaseType_t taskSCHEDULER_SUSPENDED = 0;
constexpr BaseType_t taskSCHEDULER_NOT_STARTED = 1;
constexpr BaseType_t taskSCHEDULER_RUNNING = 2;

inline BaseType_t logIndentTestSchedulerState = taskSCHEDULER_NOT_STARTED;

inline BaseType_t xTaskGetSchedulerState() {
  return logIndentTestSchedulerState;
}

#endif  // EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_FREERTOS_TASK_H_
