/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

const char *SUPLA_TAG = "SUPLA";

#ifdef SUPLA_DEVICE_ESP32
#include <stdarg.h>
#include <stdio.h>

#include <esp_log.h>
#include <supla-common/log.h>
#include <supla/debug/debug_log.h>

void supla_device_logf(int __pri, const char *__fmt, ...) {
  if (__fmt == NULL || !supla_log_is_enabled(__pri)) {
    return;
  }

  char buffer[256] = {};
  va_list args;
  va_start(args, __fmt);
  int written = vsnprintf(buffer, sizeof(buffer), __fmt, args);
  va_end(args);

  if (written < 0) {
    return;
  }
  if (written >= static_cast<int>(sizeof(buffer))) {
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

#include "log_wrapper.h"

extern "C" void serialPrintLn(const char *);

void serialPrintLn(const char *message) {
  Serial.println(message);
}

void supla_logf(int __pri, const __FlashStringHelper *__fmt, ...) {
  va_list ap;
  char *buffer = NULL;
  int size = 0;

  if (__fmt == NULL || !supla_log_is_enabled(__pri)) return;

  String fmt(__fmt);


  while (1) {
    va_start(ap, __fmt);
    if (0 == supla_log_string(&buffer, &size, ap, fmt.c_str())) {
      va_end(ap);
      break;
    } else {
      va_end(ap);
    }
    va_end(ap);
  }

  if (buffer == NULL) return;

  supla_vlog(__pri, buffer);
  supla_debug_log_write(__pri, buffer);
  free(buffer);
}

#endif /*ARDUINO*/
