// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef SRC_SUPLA_PROTOCOL_SUPLAN_PROTOCOL_H_
#define SRC_SUPLA_PROTOCOL_SUPLAN_PROTOCOL_H_

#ifndef ARDUINO_ARCH_AVR

#include <stddef.h>
#include <stdint.h>

#include <supla/protocol/protocol_layer.h>
#include <suplan/suplan_runtime.h>

class SuplaDeviceClass;
namespace Supla {
class Channel;
namespace Device {
class ServerAssociations;
}
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

// Device-side adapter for restartable platform transports. This interface
// stays outside the portable SupLAN core and is driven by ProtocolLayer's
// normal SuplaDevice lifecycle.
class SupLanTransportLifecycle {
 public:
  virtual ~SupLanTransportLifecycle() = default;
  virtual bool networkReady(uint32_t nowMs) = 0;
  virtual bool isOpen() const = 0;
  virtual bool open() = 0;
  virtual void close() = 0;

  // Keep retry timing with the transport adapter rather than the ProtocolLayer
  // so SupLan's embedded object stays within its existing size budget.
  bool openIfDue(uint32_t nowMs) {
    if (openAttempted_ &&
        static_cast<uint32_t>(nowMs - lastOpenAttemptMs_) < 1000) {
      return false;
    }
    openAttempted_ = true;
    lastOpenAttemptMs_ = nowMs;
    return open();
  }

  void resetOpenRetry() { openAttempted_ = false; }

 private:
  bool openAttempted_ = false;
  uint32_t lastOpenAttemptMs_ = 0;
};

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

  // Production SERVER mode: caller owns one bounded manager, peers and runtime.
  // With no PoC mappings, resource resolution uses the M1B SERVER identity.
  void attachServerAssociations(Supla::Device::ServerAssociations *state);
  bool setSuplanSourceAssociation(
      const TSDS_SuplaSetSuplanSourceAssociation &request,
      TDS_SuplaSetSuplanSourceAssociationResult *result) override;
  bool setSuplanDestinationAssociation(
      const TSDS_SuplaSetSuplanDestinationAssociation &request,
      TDS_SuplaSetSuplanDestinationAssociationResult *result) override;
  void suplanIdentityChanged() override;
  void attachRuntime(Supla::SupLan::Runtime *runtime);
  void attachTransportLifecycle(SupLanTransportLifecycle *transport);
  void setEnabled(bool enabled);
  void onInit() override;
  bool onLoadConfig() override;
  bool verifyConfig() override;
  bool isEnabled() override;
  void disconnect() override;
  bool isConfigEmpty() override;
  bool iterate(uint32_t millis) override;
  bool isNetworkRestartRequested() override;
  bool protectsNetworkFromPeerRestart() override;
  uint32_t getConnectionFailTime() override;
  bool isRegisteredAndReady() override;
  bool isTransportOpen() const;
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
  bool localChannel(uint32_t id, uint8_t *number, bool *eventOnly) const;
  const SupLanResourceMapping *findResource(uint32_t resourceId) const;
  bool mapIsValid() const;
  void clearPeerTransportState();
  static void putSuplaUint32(uint8_t output[4], uint32_t value);
  static uint32_t getSuplaUint32(const uint8_t input[4]);
  static uint8_t channelOfflineState(const Supla::Channel *channel);

  Supla::Device::ServerAssociations *associations_ = nullptr;
  Supla::SupLan::PeerTable *peers_;
  Supla::SupLan::Runtime *runtime_;
  SupLanTransportLifecycle *transport_;
  const SupLanResourceMapping *mappings_;
  SupLanApplicationEventHandler eventHandler_;
  void *eventContext_;
  uint8_t mappingCount_;
  bool enabled_;
  bool transportWasOpen_;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // !ARDUINO_ARCH_AVR

#endif  // SRC_SUPLA_PROTOCOL_SUPLAN_PROTOCOL_H_
