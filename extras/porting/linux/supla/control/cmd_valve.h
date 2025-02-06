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

};  // namespace Control
};  // namespace Supla
#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_VALVE_H_
