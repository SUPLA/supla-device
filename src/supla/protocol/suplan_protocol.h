// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef SRC_SUPLA_PROTOCOL_SUPLAN_PROTOCOL_H_
#define SRC_SUPLA_PROTOCOL_SUPLAN_PROTOCOL_H_

#include <stddef.h>
#include <stdint.h>

#include <supla/protocol/protocol_layer.h>
#include <suplan/suplan_runtime.h>

class SuplaDeviceClass;
namespace Supla {
class Channel;
}

namespace Supla {
namespace Protocol {

struct SupLanResourceMapping {
  uint32_t resourceId;
  uint8_t channelNumber;
  uint8_t peerIndex;
  bool eventOnly;
};

enum SupLanApplicationEvent : uint8_t {
  kSupLanRemoteState = 1,
  kSupLanRemoteAction = 2,
  kSupLanOperationAck = 3,
  kSupLanReadInterest = 4,
};

typedef void (*SupLanApplicationEventHandler)(
    void *context, SupLanApplicationEvent event, uint8_t peerIndex,
    const Supla::SupLan::ResourceId &resource, uint32_t messageType,
    const uint8_t *payload, size_t payloadLength);

// The adapter uses the existing SUPLA Element/Channel model for local READ
// and CONTROL dispatch. Remote state/events are handed to the application
// through a callback because supla-device has no client-side channel cache.
class SupLan : public ProtocolLayer, public Supla::SupLan::ApplicationPort {
 public:
  static const uint8_t kMaxResourceMappings = 8;

  SupLan(SuplaDeviceClass *sdc, Supla::SupLan::PeerTable *peers,
         const SupLanResourceMapping *mappings, uint8_t mappingCount,
         SupLanApplicationEventHandler eventHandler = nullptr,
         void *eventContext = nullptr);
  ~SupLan() override;

  void attachRuntime(Supla::SupLan::Runtime *runtime);
  void setEnabled(bool enabled);
  void onInit() override;
  bool onLoadConfig() override;
  bool verifyConfig() override;
  bool isEnabled() override;
  void disconnect() override;
  bool isConfigEmpty() override;
  bool iterate(uint32_t millis) override;
  bool isNetworkRestartRequested() override;
  uint32_t getConnectionFailTime() override;
  bool isRegisteredAndReady() override;
  void sendActionTrigger(uint8_t channelNumber, uint32_t actionId) override;
  void sendChannelValueChanged(uint8_t channelNumber, int8_t *value,
                              uint8_t offline,
                              uint32_t validityTimeSec) override;
  void sendExtendedChannelValueChanged(
      uint8_t channelNumber, TSuplaChannelExtendedValue *value) override;

  bool readResource(const Supla::SupLan::ResourceId &resource,
                    bool *eventOnly, uint8_t *payload, size_t capacity,
                    size_t *payloadLength) override;
  uint8_t dispatchControl(const Supla::SupLan::ResourceId &resource,
                          uint32_t messageType, const uint8_t *payload,
                          size_t payloadLength) override;
  void receiveState(uint8_t peerIndex,
                    const Supla::SupLan::ResourceId &resource,
                    uint32_t messageType, const uint8_t *payload,
                    size_t payloadLength) override;
  void receiveAction(uint8_t peerIndex,
                     const Supla::SupLan::ResourceId &resource,
                     uint32_t messageType, const uint8_t *payload,
                     size_t payloadLength) override;
  void operationAcknowledged(uint8_t peerIndex,
                             const Supla::SupLan::ResourceId &resource,
                             uint32_t sequence, uint8_t result) override;
  void readInterestRefreshed(
      uint8_t peerIndex, const Supla::SupLan::ResourceId &resource,
      bool eventOnly) override;

 private:
  const SupLanResourceMapping *findResource(uint32_t resourceId) const;
  bool mapIsValid() const;
  static void putSuplaUint32(uint8_t output[4], uint32_t value);
  static uint32_t getSuplaUint32(const uint8_t input[4]);
  static uint8_t channelOfflineState(const Supla::Channel *channel);

  Supla::SupLan::PeerTable *peers_;
  Supla::SupLan::Runtime *runtime_;
  const SupLanResourceMapping *mappings_;
  uint8_t mappingCount_;
  SupLanApplicationEventHandler eventHandler_;
  void *eventContext_;
  bool enabled_;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_SUPLAN_PROTOCOL_H_
