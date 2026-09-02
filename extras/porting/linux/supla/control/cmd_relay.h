// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_RELAY_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_RELAY_H_

#include <supla/control/virtual_relay.h>
#include <supla/sensor/sensor_parsed.h>

#include <string>

namespace Supla {
namespace Control {
class CmdRelay : public Sensor::SensorParsed<VirtualRelay> {
 public:
  CmdRelay(Supla::Parser::Parser *parser, _supla_int_t functions =
                   (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  void onInit() override;
  void turnOn(_supla_int_t duration = 0) override;
  void turnOff(_supla_int_t duration = 0) override;
  bool isOn() override;
  void iterateAlways() override;

  void setCmdOn(const std::string &);
  void setCmdOff(const std::string &);

 protected:
  std::string cmdOn;
  std::string cmdOff;
  uint32_t lastReadTime = 0;
};

};  // namespace Control
};  // namespace Supla


#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_RELAY_H_
