// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/debug/debug_log.h>
#include <supla/log_wrapper.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *SUPLA_TAG = "SUPLA";

#ifndef SUPLA_DEVICE_ESP32
#ifndef ARDUINO
void supla_device_logf(int priority, const char *format, ...) {
  if (format == nullptr || !supla_log_is_enabled(priority)) {
    return;
  }
  const int indent = Supla::LogIndent::spaces();
  int size = strlen(format) + 10 + indent;
  char *buffer = static_cast<char *>(malloc(size));
  while (buffer != nullptr) {
    va_list args;
    va_start(args, format);
    int written = vsnprintf(buffer + indent, size - indent, format, args);
    va_end(args);
    if (written < 0) {
      free(buffer);
      buffer = nullptr;
      break;
    }
    if (written < size - indent) {
      memset(buffer, ' ', indent);
      break;
    }
    size = indent + written + 1;
    char *resized = static_cast<char *>(realloc(buffer, size));
    if (resized == nullptr) {
      free(buffer);
    }
    buffer = resized;
  }
  if (buffer != nullptr) {
    supla_vlog(priority, buffer);
    free(buffer);
  }
}
#endif
#endif

#ifdef SUPLA_DEVICE_ESP32
#include <esp_log.h>
#include <supla-common/log.h>

void supla_device_logf(int __pri, const char *__fmt, ...) {
  if (__fmt == NULL || !supla_log_is_enabled(__pri)) {
    return;
  }

  char buffer[256] = {};
  va_list args;
  va_start(args, __fmt);
  const int indent = Supla::LogIndent::spaces();
  memset(buffer, ' ', indent);
  int written = vsnprintf(buffer + indent, sizeof(buffer) - indent,
                          __fmt, args);
  va_end(args);

  if (written < 0) {
    return;
  }
  if (written >= static_cast<int>(sizeof(buffer)) - indent) {
    buffer[sizeof(buffer) - 5] = '.';
    buffer[sizeof(buffer) - 4] = '.';
    buffer[sizeof(buffer) - 3] = '.';
    buffer[sizeof(buffer) - 2] = '\0';
  }

  switch (__pri) {
    case LOG_VERBOSE:
      ESP_LOGV(SUPLA_TAG, "%s", buffer);
      break;
    case LOG_DEBUG:
      ESP_LOGD(SUPLA_TAG, "%s", buffer);
      break;
    case LOG_INFO:
    case LOG_NOTICE:
      ESP_LOGI(SUPLA_TAG, "%s", buffer);
      break;
    case LOG_WARNING:
      ESP_LOGW(SUPLA_TAG, "%s", buffer);
      break;
    case LOG_ERR:
    case LOG_CRIT:
    case LOG_EMERG:
    case LOG_ALERT:
    default:
      ESP_LOGE(SUPLA_TAG, "%s", buffer);
      break;
  }

  supla_debug_log_write(__pri, buffer);
}
#endif  // SUPLA_DEVICE_ESP32

#ifdef ARDUINO
#include <Arduino.h>

extern "C" void serialPrintLn(const char *);

void serialPrintLn(const char *message) {
  Serial.println(message);
}

void supla_logf(int __pri, const __FlashStringHelper *__fmt, ...) {
  if (__fmt == NULL || !supla_log_is_enabled(__pri)) return;

  String fmt(__fmt);
  const int indent = Supla::LogIndent::spaces();
  int size = strlen(fmt.c_str()) + 10 + indent;
  char *buffer = static_cast<char *>(malloc(size));
  while (buffer != nullptr) {
    va_list args;
    va_start(args, __fmt);
    int written = vsnprintf(buffer + indent, size - indent, fmt.c_str(), args);
    va_end(args);
    if (written < 0) {
      free(buffer);
      buffer = nullptr;
      break;
    }
    if (written < size - indent) {
      memset(buffer, ' ', indent);
      break;
    }
    size = indent + written + 1;
    char *resized = static_cast<char *>(realloc(buffer, size));
    if (resized == nullptr) {
      free(buffer);
    }
    buffer = resized;
  }

  if (buffer == NULL) return;

  supla_vlog(__pri, buffer);
  supla_debug_log_write(__pri, buffer);
  free(buffer);
}

#endif /*ARDUINO*/
