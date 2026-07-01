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

#include <supla/debug/debug_log.h>

#if SUPLA_INSECURE_DEBUG_INTERFACE

#include <cstdarg>
#include <cstdio>
#include <supla-common/log.h>

namespace {

Supla::Debug::LogSink *currentLogSink = nullptr;

}  // namespace

namespace Supla {
namespace Debug {

void setLogSink(LogSink *sink) {
  currentLogSink = sink;
}

void clearLogSink(LogSink *sink) {
  if (currentLogSink == sink) {
    currentLogSink = nullptr;
  }
}

void writeLog(int priority, const char *message) {
  auto *sink = currentLogSink;
  if (sink != nullptr && message != nullptr) {
    sink->writeLog(priority, message);
  }
}

const char *logPriorityPrefix(int priority) {
  switch (priority) {
    case LOG_EMERG:
      return "EME";
    case LOG_ALERT:
      return "ALR";
    case LOG_CRIT:
      return "CRI";
    case LOG_ERR:
      return "ERR";
    case LOG_WARNING:
      return "WAR";
    case LOG_NOTICE:
      return "NOT";
    case LOG_INFO:
      return "INF";
    case LOG_DEBUG:
      return "DEB";
    case LOG_VERBOSE:
      return "VER";
  }
  return "LOG";
}

}  // namespace Debug
}  // namespace Supla

extern "C" void supla_debug_log_write(int priority, const char *message) {
  Supla::Debug::writeLog(priority, message);
}

extern "C" void supla_debug_logf(int priority, const char *format, ...) {
  if (format == nullptr) {
    return;
  }

  char buffer[256] = {};
  va_list args;
  va_start(args, format);
  int written = vsnprintf(buffer, sizeof(buffer), format, args);  // NOLINT
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

  supla_debug_log_write(priority, buffer);
}

#else

namespace Supla {
namespace Debug {

void setLogSink(LogSink *) {
}

void clearLogSink(LogSink *) {
}

void writeLog(int, const char *) {
}

const char *logPriorityPrefix(int) {
  return "LOG";
}

}  // namespace Debug
}  // namespace Supla

extern "C" void supla_debug_log_write(int, const char *) {
}

extern "C" void supla_debug_logf(int, const char *, ...) {
}

#endif  // SUPLA_INSECURE_DEBUG_INTERFACE
