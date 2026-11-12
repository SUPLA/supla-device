// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_LOG_INDENT_H_
#define SRC_SUPLA_LOG_INDENT_H_

#include <stdint.h>

#if !defined(SUPLA_DISABLE_LOGS) && \
    (defined(SUPLA_DEVICE_ESP32) || defined(ARDUINO_ARCH_ESP32))
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace Supla {

// Indents SUPLA_LOG_* messages in this scope, including nested calls.
// Keep scopes synchronous: indentation does not follow queued callbacks.
class LogIndent {
 public:
#ifdef SUPLA_DISABLE_LOGS
  LogIndent() {}
  ~LogIndent() {}
  static int spaces() { return 0; }
#else
  LogIndent() : previous(depth()) {
    if (depth() < 8) {
      ++depth();
    }
  }
  ~LogIndent() { depth() = previous; }
  static int spaces() { return depth() * 2; }
#endif

  LogIndent(const LogIndent &) = delete;
  LogIndent &operator=(const LogIndent &) = delete;
  LogIndent(LogIndent &&) = delete;
  LogIndent &operator=(LogIndent &&) = delete;

 private:
#ifndef SUPLA_DISABLE_LOGS
  static uint8_t &depth() {
#if defined(SUPLA_DEVICE_ESP32) || defined(ARDUINO_ARCH_ESP32)
    // Global constructors run before FreeRTOS installs a task's thread pointer.
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
      static uint8_t startupDepth = 0;
      return startupDepth;
    }
#endif
#if defined(SUPLA_LINUX) || defined(SUPLA_TEST) || \
    defined(SUPLA_DEVICE_ESP32) || defined(ARDUINO_ARCH_ESP32) || \
    (defined(SUPLA_FREERTOS) && defined(__linux__))
    static thread_local uint8_t value = 0;
#else
    static uint8_t value = 0;
#endif
    return value;
  }
  uint8_t previous;
#endif
};

}  // namespace Supla

#endif  // SRC_SUPLA_LOG_INDENT_H_
