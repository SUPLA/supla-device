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

#ifndef SRC_SUPLA_DEBUG_DEBUG_LOG_TCP_SERVER_H_
#define SRC_SUPLA_DEBUG_DEBUG_LOG_TCP_SERVER_H_

#include <stdint.h>

#include <supla/debug/debug_config.h>
#include <supla/debug/debug_log.h>

namespace Supla {
namespace Debug {

class DebugLogTcpServer : public LogSink {
 public:
  explicit DebugLogTcpServer(uint16_t port = 7778);
  ~DebugLogTcpServer() override;

  bool begin();
  void end();
  void iterate();
  void writeLog(int priority, const char *message) override;

 private:
  void writeLine(int priority, const char *message);
  bool writeBytes(const char *data, unsigned int size);

  uint16_t port = 7778;

#if SUPLA_INSECURE_DEBUG_INTERFACE && defined(SUPLA_LINUX)
  int listenSocket = -1;
  int clientSocket = -1;
  bool openListenSocket();
  void acceptClient();
  void closeListenSocket();
  void closeClient();
#elif SUPLA_INSECURE_DEBUG_INTERFACE && defined(ESP_PLATFORM) && \
    !defined(ARDUINO)
  int listenSocket = -1;
  int clientSocket = -1;
  bool openListenSocket();
  void acceptClient();
  void closeListenSocket();
  void closeClient();
#elif SUPLA_INSECURE_DEBUG_INTERFACE && defined(ARDUINO) && \
    (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32))
  class Impl;
  Impl *impl = nullptr;
#endif
};

}  // namespace Debug
}  // namespace Supla

#endif  // SRC_SUPLA_DEBUG_DEBUG_LOG_TCP_SERVER_H_
