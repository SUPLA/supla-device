// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_ROLLER_SHUTTER_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_ROLLER_SHUTTER_H_

#include <supla/control/roller_shutter.h>
#include <supla/sensor/sensor_parsed.h>

#include <string>

namespace Supla {
namespace Control {
class CmdRollerShutter : public Sensor::SensorParsed<RollerShutter> {
 public:
  explicit CmdRollerShutter(Supla::Parser::Parser *parser);

  void relayDownOn() override;
  void relayUpOn() override;
  void relayDownOff() override;
  void relayUpOff() override;

  void iterateAlways() override;

  void setCmdUpOn(const std::string &);
  void setCmdUpOff(const std::string &);
  void setCmdDownOn(const std::string &);
  void setCmdDownOff(const std::string &);

 protected:
  std::string cmdUpOn;
  std::string cmdUpOff;
  std::string cmdDownOn;
  std::string cmdDownOff;
  uint32_t lastReadTime = 0;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_ROLLER_SHUTTER_H_
