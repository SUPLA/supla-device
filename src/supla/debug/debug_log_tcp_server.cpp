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

#include <supla/debug/debug_log_tcp_server.h>

#if SUPLA_INSECURE_DEBUG_INTERFACE

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <supla/network/network.h>
#include <supla/time.h>

#if defined(SUPLA_LINUX)
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#elif defined(ESP_PLATFORM) && !defined(ARDUINO)
#include <errno.h>
#include <fcntl.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#elif defined(ARDUINO) && \
    (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32))
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#endif

namespace {

constexpr unsigned int kLineBufferSize = 320;

unsigned int appendSanitizedMessage(const char *message,
                                    char *output,
                                    unsigned int size) {
  if (output == nullptr || size == 0) {
    return 0;
  }
  if (message == nullptr) {
    output[0] = '\0';
    return 0;
  }

  unsigned int pos = 0;
  while (*message != '\0' && pos + 1 < size) {
    char c = *message++;
    if (c == '\r' || c == '\n') {
      c = ' ';
    }
    output[pos++] = c;
  }
  output[pos] = '\0';
  return pos;
}

#if defined(SUPLA_LINUX) || (defined(ESP_PLATFORM) && !defined(ARDUINO))
bool setNonBlocking(int socket) {
  int flags = fcntl(socket, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif

}  // namespace

namespace Supla {
namespace Debug {

#if defined(ARDUINO) && \
    (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32))
class DebugLogTcpServer::Impl {
 public:
  explicit Impl(uint16_t port) : server(port) {
  }

  WiFiServer server;
  WiFiClient client;
};
#endif

DebugLogTcpServer::DebugLogTcpServer(uint16_t port) : port(port) {
}

DebugLogTcpServer::~DebugLogTcpServer() {
  end();
}

bool DebugLogTcpServer::begin() {
  if (port == 0) {
    return true;
  }
#if defined(ARDUINO) && \
    (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32))
  if (impl == nullptr) {
    impl = new Impl(port);
    if (impl == nullptr) {
      return false;
    }
    impl->server.begin();
  }
#endif
  setLogSink(this);
  return true;
}

void DebugLogTcpServer::end() {
  clearLogSink(this);
#if defined(SUPLA_LINUX) || (defined(ESP_PLATFORM) && !defined(ARDUINO))
  closeClient();
  closeListenSocket();
#elif defined(ARDUINO) && \
    (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32))
  if (impl != nullptr) {
    impl->client.stop();
    impl->server.stop();
    delete impl;
    impl = nullptr;
  }
#endif
}

void DebugLogTcpServer::iterate() {
  if (port == 0) {
    return;
  }
  if (!Supla::Network::IsReady()) {
#if defined(SUPLA_LINUX) || (defined(ESP_PLATFORM) && !defined(ARDUINO))
    closeClient();
    closeListenSocket();
#elif defined(ARDUINO) && \
    (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32))
    if (impl != nullptr) {
      impl->client.stop();
    }
#endif
    return;
  }

#if defined(SUPLA_LINUX) || (defined(ESP_PLATFORM) && !defined(ARDUINO))
  if (listenSocket < 0 && !openListenSocket()) {
    return;
  }
  acceptClient();
#elif defined(ARDUINO) && \
    (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32))
  if (impl == nullptr) {
    return;
  }
  if (impl->client && !impl->client.connected()) {
    impl->client.stop();
  }
  WiFiClient incoming = impl->server.available();
  if (incoming) {
    if (impl->client && impl->client.connected()) {
      static const char busy[] = "{\"ok\":false,\"error\":\"busy\"}\n";
      incoming.write(reinterpret_cast<const uint8_t *>(busy),
                     sizeof(busy) - 1);
      incoming.stop();
    } else {
      impl->client = incoming;
    }
  }
#endif
}

void DebugLogTcpServer::writeLog(int priority, const char *message) {
  writeLine(priority, message);
}

void DebugLogTcpServer::writeLine(int priority, const char *message) {
  if (port == 0) {
    return;
  }
  char line[kLineBufferSize + 32] = {};
  int prefixSize = snprintf(line,
                            sizeof(line),
                            "%s[%" PRIu32 "] ",
                            logPriorityPrefix(priority),
                            static_cast<uint32_t>(millis()));
  if (prefixSize < 0) {
    return;
  }
  unsigned int pos = static_cast<unsigned int>(prefixSize);
  if (pos >= sizeof(line)) {
    pos = sizeof(line) - 1;
  }
  pos += appendSanitizedMessage(message, line + pos, sizeof(line) - pos);
  if (pos + 1 < sizeof(line)) {
    line[pos++] = '\n';
    line[pos] = '\0';
  } else {
    line[sizeof(line) - 2] = '\n';
    line[sizeof(line) - 1] = '\0';
  }
  writeBytes(line, static_cast<unsigned int>(strlen(line)));
}

#if defined(SUPLA_LINUX)

bool DebugLogTcpServer::openListenSocket() {
  listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (listenSocket < 0) {
    return false;
  }

  int yes = 1;
  setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (!setNonBlocking(listenSocket)) {
    closeListenSocket();
    return false;
  }

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (bind(listenSocket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
      0) {
    closeListenSocket();
    return false;
  }
  if (listen(listenSocket, 1) < 0) {
    closeListenSocket();
    return false;
  }
  return true;
}

void DebugLogTcpServer::acceptClient() {
  int socket = accept(listenSocket, nullptr, nullptr);
  if (socket < 0) {
    return;
  }
  setNonBlocking(socket);
  if (clientSocket >= 0) {
    static const char busy[] = "{\"ok\":false,\"error\":\"busy\"}\n";
    send(socket, busy, sizeof(busy) - 1, MSG_NOSIGNAL);
    close(socket);
    return;
  }
  clientSocket = socket;
}

void DebugLogTcpServer::closeListenSocket() {
  if (listenSocket >= 0) {
    close(listenSocket);
    listenSocket = -1;
  }
}

void DebugLogTcpServer::closeClient() {
  if (clientSocket >= 0) {
    close(clientSocket);
    clientSocket = -1;
  }
}

bool DebugLogTcpServer::writeBytes(const char *data, unsigned int size) {
  if (clientSocket < 0 || data == nullptr || size == 0) {
    return false;
  }
  int result = send(clientSocket, data, size, MSG_NOSIGNAL);
  if (result < 0) {
    closeClient();
    return false;
  }
  if (static_cast<unsigned int>(result) != size) {
    closeClient();
    return false;
  }
  return true;
}

#elif defined(ESP_PLATFORM) && !defined(ARDUINO)

bool DebugLogTcpServer::openListenSocket() {
  listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (listenSocket < 0) {
    return false;
  }

  int yes = 1;
  setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (!setNonBlocking(listenSocket)) {
    closeListenSocket();
    return false;
  }

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(listenSocket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
      0) {
    closeListenSocket();
    return false;
  }
  if (listen(listenSocket, 1) < 0) {
    closeListenSocket();
    return false;
  }
  return true;
}

void DebugLogTcpServer::acceptClient() {
  int socket = accept(listenSocket, nullptr, nullptr);
  if (socket < 0) {
    return;
  }
  setNonBlocking(socket);
  if (clientSocket >= 0) {
    static const char busy[] = "{\"ok\":false,\"error\":\"busy\"}\n";
    send(socket, busy, sizeof(busy) - 1, 0);
    shutdown(socket, SHUT_RDWR);
    close(socket);
    return;
  }
  clientSocket = socket;
}

void DebugLogTcpServer::closeListenSocket() {
  if (listenSocket >= 0) {
    shutdown(listenSocket, SHUT_RDWR);
    close(listenSocket);
    listenSocket = -1;
  }
}

void DebugLogTcpServer::closeClient() {
  if (clientSocket >= 0) {
    shutdown(clientSocket, SHUT_RDWR);
    close(clientSocket);
    clientSocket = -1;
  }
}

bool DebugLogTcpServer::writeBytes(const char *data, unsigned int size) {
  if (clientSocket < 0 || data == nullptr || size == 0) {
    return false;
  }
  int result = send(clientSocket, data, size, 0);
  if (result < 0) {
    closeClient();
    return false;
  }
  if (static_cast<unsigned int>(result) != size) {
    closeClient();
    return false;
  }
  return true;
}

#elif defined(ARDUINO) && \
    (defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32))

bool DebugLogTcpServer::writeBytes(const char *data, unsigned int size) {
  if (impl == nullptr || data == nullptr || size == 0 || !impl->client ||
      !impl->client.connected()) {
    return false;
  }
  size_t written =
      impl->client.write(reinterpret_cast<const uint8_t *>(data), size);
  if (written != size) {
    impl->client.stop();
    return false;
  }
  return true;
}

#else

bool DebugLogTcpServer::writeBytes(const char *, unsigned int) {
  return false;
}

#endif

}  // namespace Debug
}  // namespace Supla

#else

namespace Supla {
namespace Debug {

DebugLogTcpServer::DebugLogTcpServer(uint16_t port) : port(port) {
}

DebugLogTcpServer::~DebugLogTcpServer() {
}

bool DebugLogTcpServer::begin() {
  return true;
}

void DebugLogTcpServer::end() {
}

void DebugLogTcpServer::iterate() {
}

void DebugLogTcpServer::writeLog(int, const char *) {
}

void DebugLogTcpServer::writeLine(int, const char *) {
}

}  // namespace Debug
}  // namespace Supla

#endif  // SUPLA_INSECURE_DEBUG_INTERFACE
