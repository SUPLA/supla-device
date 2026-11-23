// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_EXAMPLES_LINUX_SUPLAN_RUNTIME_LINUX_H_
#define EXTRAS_EXAMPLES_LINUX_SUPLAN_RUNTIME_LINUX_H_

#include <supla/protocol/suplan_protocol.h>
#include <supla/debug/command_processor.h>
#include <suplan/suplan_runtime.h>
#include <supla/suplan/suplan_server_associations.h>

#include <linux_yaml_config.h>
#include <suplan_crypto_openssl.h>
#include <suplan_udp_linux.h>

#include <cstdint>
#include <memory>
#include <vector>

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
  // nullptr means durable identity and a live READ grant for this channel.
  const char* sleepySourceBlocker(uint8_t channelNumber) const;
  const SupLan::Diagnostics& diagnostics() const;
  uint32_t awakeHintsSent() const;
  uint32_t resetsSent() const;
  void announceSleep(uint32_t durationSec);
  bool sleepAnnouncementSent() const;
#ifdef SUPLAN_TESTING
  bool setTransportEnabled(bool enabled);
#endif

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
#ifdef SUPLAN_TESTING
  bool nodeA_ = false;
#endif  // SUPLAN_TESTING
#ifdef SUPLAN_TESTING
  uint8_t primaryPeer_ = 0;
#endif  // SUPLAN_TESTING
#ifdef SUPLAN_TESTING
  uint8_t actionPeer_ = 1;
#endif  // SUPLAN_TESTING
  SupLan::OpenSslCryptoPort crypto_;
  class DiagnosticUdpPort : public SupLan::LinuxUdpPort {
   public:
    bool sendUnicast(const SupLan::Endpoint& endpoint,
                     const uint8_t* data, size_t length) override;
    bool sendLocateMulticast(const uint8_t* data, size_t length) override;
    uint32_t awakeHints = 0;
    uint32_t resets = 0;
    bool announcingSleep = false;
    bool sleepSent = false;

   private:
    // Only failed unicast sends made synchronously by announceSleep().
    std::vector<std::vector<uint8_t>> pendingSleepFrames_;
    void multicastMembershipChanged(uint32_t address, int8_t result) override;
  };
  DiagnosticUdpPort datagrams_;
  SupLan::PeerTable peers_;
#ifdef SUPLAN_TESTING
  Protocol::SupLanResourceMapping mapping_ = {};
#endif  // SUPLAN_TESTING
  std::unique_ptr<TransportLifecycle> transport_;
  std::unique_ptr<Protocol::SupLan> protocol_;
  std::unique_ptr<SupLan::Runtime> runtime_;
  std::unique_ptr<Device::ServerAssociations> associations_;
};

}  // namespace Supla

#endif  // EXTRAS_EXAMPLES_LINUX_SUPLAN_RUNTIME_LINUX_H_
