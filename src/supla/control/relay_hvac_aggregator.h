// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_RELAY_HVAC_AGGREGATOR_H_
#define SRC_SUPLA_CONTROL_RELAY_HVAC_AGGREGATOR_H_

#include <supla/element.h>

namespace Supla {
namespace Control {

class Relay;
class HvacBase;

class RelayHvacAggregator : public Element {
 public:
  struct HvacPtr {
    HvacBase *hvac = nullptr;
    HvacPtr *nextPtr = nullptr;
    uint32_t lastSeenTimestamp = 0;
  };

  static RelayHvacAggregator *GetInstance(int relayChannelNumber);
  static RelayHvacAggregator *Add(int relayChannelNumber, Relay *relay);
  static bool Remove(int relayChannelNumber);
  static void UnregisterHvac(HvacBase *hvac);

  void registerHvac(HvacBase *hvac);
  void unregisterHvac(HvacBase *hvac);
  bool isHvacRegistered(HvacBase *hvac) const;
  int getHvacCount() const;

  void iterateAlways() override;
  void setTurnOffWhenEmpty(bool turnOffWhenEmpty);

  void setInternalStateCheckInterval(uint32_t intervalMs);

 protected:
  explicit RelayHvacAggregator(int relayChannelNumber, Relay *relay);
  virtual ~RelayHvacAggregator();

 private:
  RelayHvacAggregator *nextPtr = nullptr;
  HvacPtr *firstHvacPtr = nullptr;
  Relay *relay = nullptr;
  int relayChannelNumber = 0;
  uint32_t lastUpdateTimestamp = 0;
  uint32_t lastStateUpdateTimestamp = 0;
  uint32_t relayInternalStateCheckIntervalMs = 10000;
  bool turnOffWhenEmpty = true;
  int8_t lastValueSend = -1;
  int8_t lastRelayState = -1;
  int8_t turnOffSendOnEmpty = 0;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_HVAC_AGGREGATOR_H_
