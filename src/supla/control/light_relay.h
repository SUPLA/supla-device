// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_LIGHT_RELAY_H_
#define SRC_SUPLA_CONTROL_LIGHT_RELAY_H_

#include <supla-common/proto.h>
#include <stdint.h>
#include <supla/io.h>
#include "relay.h"

namespace Supla {
namespace Control {
class LightRelay : public Relay {
 public:
  explicit LightRelay(Supla::Io::IoPin outputPin);
  explicit LightRelay(int pin, bool highIsOn = true);
  void handleGetChannelState(TDSC_ChannelState *channelState);
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request);
  void onLoadState();
  void onSaveState();
  void turnOn(_supla_int_t duration = 0);
  void iterateAlways();

 protected:
  uint16_t lifespan;
  _supla_int_t turnOnSecondsCumulative;
  uint32_t turnOnTimestamp;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_LIGHT_RELAY_H_
