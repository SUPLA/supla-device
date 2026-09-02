// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CUSTOM_RELAY_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CUSTOM_RELAY_H_

#include <supla/control/control_payload.h>
#include <supla/control/virtual_relay.h>
#include <supla/sensor/sensor_parsed.h>
#include <supla/payload/payload.h>

#include <string>

#include "custom_virtual_relay.h"

namespace Supla {
namespace Payload {
const char State[] = "set_state";
const char TurnOnPayload[] = "turn_on_payload";
const char TurnOffPayload[] = "turn_off_payload";
};  // namespace Payload

namespace Control {
class CustomRelay : public Sensor::SensorParsed<CustomVirtualRelay>,
                    public Payload::ControlPayload<CustomVirtualRelay> {
 public:
  CustomRelay(Supla::Parser::Parser *parser,
              Supla::Payload::Payload *payload,
              _supla_int_t functions =
                  (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  void onInit() override;
  void turnOn(_supla_int_t duration = 0) override;
  void turnOff(_supla_int_t duration = 0) override;
  bool isOn() override;
  void iterateAlways() override;

 protected:
  uint32_t lastReadTime = 0;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CUSTOM_RELAY_H_
