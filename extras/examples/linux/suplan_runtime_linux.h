// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_EXAMPLES_LINUX_SUPLAN_RUNTIME_LINUX_H_
#define EXTRAS_EXAMPLES_LINUX_SUPLAN_RUNTIME_LINUX_H_

#include <supla/protocol/suplan_protocol.h>
#include <supla/debug/command_processor.h>
#include <suplan/suplan_runtime.h>

#include <linux_yaml_config.h>
#include <suplan_crypto_openssl.h>
#include <suplan_udp_linux.h>

#include <cstdint>
#include <memory>

namespace Supla {

class LinuxSupLanRuntime {
 public:
  explicit LinuxSupLanRuntime(const LinuxSupLanConfig& config);
  ~LinuxSupLanRuntime();

  bool initialize();
  bool processDebugCommand(const char* line,
                           Debug::ResponseWriter* writer);
  Protocol::SupLan* protocol() const;
  bool isTransportOpen() const;
  bool setTransportEnabledForTest(bool enabled);

 private:
  class TransportLifecycle;

  static void onApplicationEvent(
      void* context, Protocol::SupLanApplicationEvent event,
      uint8_t peerIndex, const SupLan::ResourceId& resource,
      uint32_t messageType, const uint8_t* payload, size_t payloadLength);
  void writeCounters(Debug::ResponseWriter* writer) const;
  void writePools(Debug::ResponseWriter* writer) const;
  void writeStatus(Debug::ResponseWriter* writer) const;
  bool debugRead(uint32_t resourceId);
  bool debugControl(uint32_t resourceId, uint32_t value);

  LinuxSupLanConfig config_;
  bool nodeA_ = false;
  uint8_t primaryPeer_ = 0;
  uint8_t actionPeer_ = 1;
  SupLan::OpenSslCryptoPort crypto_;
  SupLan::OpenSslRandomPort random_;
  SupLan::LinuxUdpPort datagrams_;
  SupLan::PeerTable peers_;
  Protocol::SupLanResourceMapping mapping_ = {};
  std::unique_ptr<TransportLifecycle> transport_;
  std::unique_ptr<Protocol::SupLan> protocol_;
  std::unique_ptr<SupLan::Runtime> runtime_;
};

}  // namespace Supla

#endif  // EXTRAS_EXAMPLES_LINUX_SUPLAN_RUNTIME_LINUX_H_
