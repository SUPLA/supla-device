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

#include "debug_socket.h"

#include <SuplaDevice.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <supla-common/proto.h>
#include <supla/debug/command_processor.h>
#include <supla/log_wrapper.h>
#include <supla/sha256.h>
#include <supla/storage/config.h>
#include <supla/suplet/capability_registry.h>
#include <supla/suplet/definition_cache.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/registry.h>
#include <supla/suplet/server_config.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <memory>
#include <string>

namespace {

class LinuxSupletSha256Provider : public Supla::Suplet::Sha256Provider {
 public:
  bool calculate(const uint8_t *data,
                 size_t dataSize,
                 uint8_t *output,
                 size_t outputSize) override {
    if (output == nullptr || outputSize < 32) {
      return false;
    }
    Supla::Sha256 sha256;
    if (data != nullptr && dataSize > 0) {
      sha256.update(data, dataSize);
    }
    sha256.digest(output, outputSize);
    return true;
  }
};

class SocketResponseWriter : public Supla::Debug::ResponseWriter {
 public:
  explicit SocketResponseWriter(int fd) : fd(fd) {
  }

  void write(const char *text) override {
    if (fd < 0 || text == nullptr) {
      return;
    }
    const char *ptr = text;
    size_t left = strlen(text);
    while (left > 0) {
      ssize_t written = send(fd, ptr, left, MSG_NOSIGNAL);
      if (written < 0) {
        if (errno == EINTR) {
          continue;
        }
        return;
      }
      if (written == 0) {
        return;
      }
      ptr += written;
      left -= static_cast<size_t>(written);
    }
  }

 private:
  int fd = -1;
};

class DebugUnixSocket {
 public:
  explicit DebugUnixSocket(const std::string &path)
      : path(path), processor(&SuplaDevice) {
  }

  ~DebugUnixSocket() {
    closeClient();
    if (serverFd >= 0) {
      close(serverFd);
    }
    if (!path.empty()) {
      unlink(path.c_str());
    }
  }

  bool init() {
    if (path.empty()) {
      return true;
    }
    if (path.size() >= sizeof(sockaddr_un::sun_path)) {
      SUPLA_LOG_ERROR("Debug socket path is too long: %s", path.c_str());
      return false;
    }

    struct stat st = {};
    if (stat(path.c_str(), &st) == 0) {
      if (!S_ISSOCK(st.st_mode)) {
        SUPLA_LOG_ERROR("Debug socket path exists and is not a socket: %s",
                        path.c_str());
        return false;
      }
      unlink(path.c_str());
    } else if (errno != ENOENT) {
      SUPLA_LOG_ERROR(
          "Debug socket stat failed for %s: %s", path.c_str(), strerror(errno));
      return false;
    }

    serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0) {
      SUPLA_LOG_ERROR("Debug socket create failed: %s", strerror(errno));
      return false;
    }
    setNonBlocking(serverFd);

    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
    if (bind(serverFd, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) != 0) {
      SUPLA_LOG_ERROR(
          "Debug socket bind failed for %s: %s", path.c_str(), strerror(errno));
      close(serverFd);
      serverFd = -1;
      return false;
    }
    chmod(path.c_str(), 0600);

    if (listen(serverFd, 1) != 0) {
      SUPLA_LOG_ERROR("Debug socket listen failed: %s", strerror(errno));
      close(serverFd);
      serverFd = -1;
      unlink(path.c_str());
      return false;
    }

    SUPLA_LOG_WARNING("Insecure debug socket enabled: %s", path.c_str());
    return true;
  }

  void iterate() {
    if (serverFd < 0) {
      return;
    }
    acceptClient();
    readClient();
  }

 private:
  static constexpr size_t kReadBufferSize = 256;
  static constexpr size_t kMaxCommandSize = 8192;

  void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
      fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
  }

  void acceptClient() {
    while (true) {
      int fd = accept(serverFd, nullptr, nullptr);
      if (fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
          SUPLA_LOG_ERROR("Debug socket accept failed: %s", strerror(errno));
        }
        return;
      }
      setNonBlocking(fd);
      if (clientFd >= 0) {
        static const char busy[] = "{\"ok\":false,\"error\":\"busy\"}\n";
        send(fd, busy, strlen(busy), MSG_NOSIGNAL);
        close(fd);
        continue;
      }
      clientFd = fd;
      command.clear();
    }
  }

  void readClient() {
    if (clientFd < 0) {
      return;
    }

    char buffer[kReadBufferSize] = {};
    while (true) {
      ssize_t count = read(clientFd, buffer, sizeof(buffer));
      if (count < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
          return;
        }
        closeClient();
        return;
      }
      if (count == 0) {
        closeClient();
        return;
      }
      for (ssize_t i = 0; i < count; i++) {
        char c = buffer[i];
        if (c == '\r') {
          continue;
        }
        if (c == '\n') {
          processCommand();
          command.clear();
          continue;
        }
        if (command.size() >= kMaxCommandSize) {
          SocketResponseWriter writer(clientFd);
          writer.write("{\"ok\":false,\"error\":\"command_too_large\"}\n");
          command.clear();
          continue;
        }
        command.push_back(c);
      }
    }
  }

  void processCommand() {
    if (command.empty()) {
      return;
    }
    SocketResponseWriter writer(clientFd);
    processor.processLine(command.c_str(), &writer);
  }

  void closeClient() {
    if (clientFd >= 0) {
      close(clientFd);
      clientFd = -1;
    }
    command.clear();
  }

  std::string path;
  int serverFd = -1;
  int clientFd = -1;
  std::string command;
  Supla::Debug::CommandProcessor processor;
};

std::unique_ptr<DebugUnixSocket> debugSocket;

}  // namespace

bool setupLinuxSupletRuntime(Supla::Config *config) {
  static Supla::Suplet::Registry registry;
  static Supla::Suplet::CapabilityRegistry capabilityRegistry;
  static Supla::Suplet::Manager manager(config);
  static LinuxSupletSha256Provider sha256Provider;
  static Supla::Suplet::DefinitionCache definitionCache(config,
                                                        &sha256Provider);
  static Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  static Supla::Suplet::ServerConfigHandler serverConfigHandler(
      &manager, &registry, &definitionCache, &downloadedDefinitions);

  static Supla::Suplet::ChannelDefinition relayChannels[1] = {};
  relayChannels[0].channelId = 1;
  relayChannels[0].kind = Supla::Suplet::ChannelKind::VirtualRelay;
  relayChannels[0].defaultFunction = SUPLA_CHANNELFNC_POWERSWITCH;
  relayChannels[0].caption = "Suplet virtual relay";

  static Supla::Suplet::Definition relayDefinition = {};
  relayDefinition.category = Supla::Suplet::Category::Virtual;
  relayDefinition.kind = Supla::Suplet::Kind::VirtualRelay;
  relayDefinition.schemaVersion = 1;
  relayDefinition.handlerVersion = 1;
  relayDefinition.definitionId = 1000;
  relayDefinition.definitionVersion = 1;
  relayDefinition.name = "sd4linux virtual relay";
  relayDefinition.channels = relayChannels;
  relayDefinition.channelCount = 1;

  if (!registry.add(&relayDefinition)) {
    SUPLA_LOG_ERROR("Suplet runtime: failed to register virtual relay");
    return false;
  }

  Supla::Suplet::Capability relayCapability = {};
  relayCapability.category = Supla::Suplet::Category::Virtual;
  relayCapability.kind = Supla::Suplet::Kind::VirtualRelay;
  relayCapability.minSchemaVersion = 1;
  relayCapability.maxSchemaVersion = 1;
  relayCapability.handlerVersion = 1;
  relayCapability.maxInstances = 8;
  relayCapability.supportsDownloadedDefinition = 1;
  if (!capabilityRegistry.add(relayCapability)) {
    SUPLA_LOG_ERROR(
        "Suplet runtime: failed to register virtual relay capability");
    return false;
  }

  static Supla::Suplet::ChannelDefinition binaryChannels[1] = {};
  binaryChannels[0].channelId = 1;
  binaryChannels[0].kind = Supla::Suplet::ChannelKind::VirtualBinarySensor;
  binaryChannels[0].defaultFunction = SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR;
  binaryChannels[0].caption = "Suplet virtual binary";

  static Supla::Suplet::Definition binaryDefinition = {};
  binaryDefinition.category = Supla::Suplet::Category::Virtual;
  binaryDefinition.kind = Supla::Suplet::Kind::VirtualBinarySensor;
  binaryDefinition.schemaVersion = 1;
  binaryDefinition.handlerVersion = 1;
  binaryDefinition.definitionId = 1002;
  binaryDefinition.definitionVersion = 1;
  binaryDefinition.name = "sd4linux virtual binary sensor";
  binaryDefinition.channels = binaryChannels;
  binaryDefinition.channelCount = 1;

  if (!registry.add(&binaryDefinition, 8)) {
    SUPLA_LOG_ERROR("Suplet runtime: failed to register virtual binary sensor");
    return false;
  }

  Supla::Suplet::Capability binaryCapability = {};
  binaryCapability.category = Supla::Suplet::Category::Virtual;
  binaryCapability.kind = Supla::Suplet::Kind::VirtualBinarySensor;
  binaryCapability.minSchemaVersion = 1;
  binaryCapability.maxSchemaVersion = 1;
  binaryCapability.handlerVersion = 1;
  binaryCapability.maxInstances = 8;
  binaryCapability.supportsDownloadedDefinition = 1;
  if (!capabilityRegistry.add(binaryCapability)) {
    SUPLA_LOG_ERROR(
        "Suplet runtime: failed to register virtual binary sensor capability");
    return false;
  }

  static Supla::Suplet::ChannelDefinition thermometerChannels[1] = {};
  thermometerChannels[0].channelId = 1;
  thermometerChannels[0].kind =
      Supla::Suplet::ChannelKind::VirtualThermometer;
  thermometerChannels[0].defaultFunction = SUPLA_CHANNELFNC_THERMOMETER;
  thermometerChannels[0].caption = "Suplet thermometer aggregate";

  static Supla::Suplet::Definition thermometerDefinition = {};
  thermometerDefinition.category = Supla::Suplet::Category::Aggregate;
  thermometerDefinition.kind = Supla::Suplet::Kind::ThermometerGroup;
  thermometerDefinition.schemaVersion = 1;
  thermometerDefinition.handlerVersion = 1;
  thermometerDefinition.definitionId = 1001;
  thermometerDefinition.definitionVersion = 1;
  thermometerDefinition.name = "sd4linux thermometer aggregate";
  thermometerDefinition.channels = thermometerChannels;
  thermometerDefinition.channelCount = 1;

  if (!registry.add(&thermometerDefinition, 8)) {
    SUPLA_LOG_ERROR("Suplet runtime: failed to register thermometer aggregate");
    return false;
  }

  Supla::Suplet::Capability thermometerCapability = {};
  thermometerCapability.category = Supla::Suplet::Category::Aggregate;
  thermometerCapability.kind = Supla::Suplet::Kind::ThermometerGroup;
  thermometerCapability.minSchemaVersion = 1;
  thermometerCapability.maxSchemaVersion = 1;
  thermometerCapability.handlerVersion = 1;
  thermometerCapability.maxInstances = 8;
  thermometerCapability.supportsDownloadedDefinition = 1;
  if (!capabilityRegistry.add(thermometerCapability)) {
    SUPLA_LOG_ERROR(
        "Suplet runtime: failed to register thermometer aggregate capability");
    return false;
  }

  serverConfigHandler.loadDownloadedDefinitions();

  SuplaDevice.setSupletRuntime(&manager, &registry);
  SuplaDevice.setSupletCapabilityRegistry(&capabilityRegistry);
  SuplaDevice.setSupletServerConfigHandler(&serverConfigHandler);

  return true;
}

bool initLinuxDebugSocket(const std::string &path) {
  debugSocket = std::make_unique<DebugUnixSocket>(path);
  return debugSocket->init();
}

void iterateLinuxDebugSocket() {
  if (debugSocket) {
    debugSocket->iterate();
  }
}
