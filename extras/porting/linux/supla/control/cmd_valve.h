// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_VALVE_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_VALVE_H_

#include <supla/control/valve_base.h>
#include <supla/sensor/sensor_parsed.h>

#include <string>

namespace Supla {
namespace Control {
class CmdValve : public Sensor::SensorParsed<ValveBase> {
 public:
  explicit CmdValve(Supla::Parser::Parser *parser);

  void onInit() override;

  void setValueOnDevice(uint8_t openLevel) override;
  uint8_t getValueOpenStateFromDevice() override;

  void setCmdOpen(const std::string &);
  void setCmdClose(const std::string &);

 protected:
  std::string cmdOpen;
  std::string cmdClose;
  uint32_t lastReadTime = 0;
};

}  // namespace Control
}  // namespace Supla
#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_VALVE_H_
