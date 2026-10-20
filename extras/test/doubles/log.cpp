// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <supla-common/log.h>

namespace {
int logLevel = LOG_VERBOSE;
char lastLog[4096] = {};
unsigned int logCalls = 0;
}

void supla_log_set_level(int level) {
  logLevel = level;
}

int supla_log_get_level() {
  return logLevel;
}

char supla_log_is_enabled(int level) {
  return level <= logLevel;
}

void supla_log(int, const char *fmt, ...) {
  ++logCalls;
  if (fmt == nullptr) {
    lastLog[0] = 0;
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(lastLog, sizeof(lastLog), fmt, args);
  va_end(args);
}

extern "C" const char *supla_test_get_last_log() {
  return lastLog;
}

extern "C" void supla_test_clear_last_log() {
  memset(lastLog, 0, sizeof(lastLog));
}

extern "C" void supla_vlog(int priority, const char *message) {
  supla_log(priority, "%s", message);
}

extern "C" unsigned int supla_test_log_calls() {
  return logCalls;
}
