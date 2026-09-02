// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEBUG_DEBUG_LOG_H_
#define SRC_SUPLA_DEBUG_DEBUG_LOG_H_

#include <stdint.h>

#include <supla/debug/debug_config.h>

#ifdef __cplusplus
namespace Supla {
namespace Debug {

class LogSink {
 public:
  virtual ~LogSink() = default;
  virtual void writeLog(int priority, const char *message) = 0;
};

void setLogSink(LogSink *sink);
void clearLogSink(LogSink *sink);
void writeLog(int priority, const char *message);
const char *logPriorityPrefix(int priority);

}  // namespace Debug
}  // namespace Supla

extern "C" void supla_debug_log_write(int priority, const char *message);
extern "C" void supla_debug_logf(int priority, const char *format, ...);
#else
void supla_debug_log_write(int priority, const char *message);
void supla_debug_logf(int priority, const char *format, ...);
#endif

#endif  // SRC_SUPLA_DEBUG_DEBUG_LOG_H_
