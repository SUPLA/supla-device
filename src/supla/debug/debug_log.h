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
