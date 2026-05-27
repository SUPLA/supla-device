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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <supla-common/log.h>

namespace {
int logLevel = LOG_VERBOSE;
char lastLog[4096] = {};
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
