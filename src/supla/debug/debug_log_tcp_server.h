// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
