/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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
  bool turnOffWhenEmpty = true;
  int8_t lastValueSend = -1;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_HVAC_AGGREGATOR_H_
