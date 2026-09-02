// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_VIRTUAL_RELAY_H_
#define SRC_SUPLA_CONTROL_VIRTUAL_RELAY_H_

#include "relay.h"

namespace Supla {
namespace Control {
class VirtualRelay : public Relay {
 public:
  VirtualRelay(_supla_int_t functions =
                   (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  void turnOn(_supla_int_t duration = 0) override;
  void turnOff(_supla_int_t duration = 0) override;
  bool isOn() override;

 protected:
  void setNewChannelValue(bool value) override;
  bool state = false;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_VIRTUAL_RELAY_H_
