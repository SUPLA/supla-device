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

#ifndef SRC_SUPLA_DEBUG_COMMAND_PROCESSOR_H_
#define SRC_SUPLA_DEBUG_COMMAND_PROCESSOR_H_

#include <stddef.h>
#include <stdint.h>

#include <supla/debug/debug_config.h>

class SuplaDeviceClass;

namespace Supla {
namespace Debug {

class ResponseWriter {
 public:
  virtual ~ResponseWriter() = default;
  virtual void write(const char *text) = 0;
};

class CommandProcessor {
 public:
  explicit CommandProcessor(SuplaDeviceClass *device);

  bool processLine(const char *line, ResponseWriter *writer);

 private:
  struct Command;

  bool parseCommand(const char *json, Command *command);
  void processCommand(const Command &command, ResponseWriter *writer);
  void processDirectCommandJson(const char *line, ResponseWriter *writer);

  void sendError(ResponseWriter *writer, const char *error);
  void sendDone(ResponseWriter *writer, bool ok);
  void sendText(ResponseWriter *writer, const char *text);
  void sendJsonString(ResponseWriter *writer,
                      const char *prefix,
                      const char *value,
                      const char *suffix);

  uint32_t nextSessionId();

  SuplaDeviceClass *device = nullptr;
  uint32_t sessionCounter = 1;
};

}  // namespace Debug
}  // namespace Supla

#endif  // SRC_SUPLA_DEBUG_COMMAND_PROCESSOR_H_

