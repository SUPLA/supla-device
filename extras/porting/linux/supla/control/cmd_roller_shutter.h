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

  bool isOffline();  // add override

  void setCmdUpOn(const std::string &);
  void setCmdUpOff(const std::string &);
  void setCmdDownOn(const std::string &);
  void setCmdDownOff(const std::string &);

  void setUseOfflineOnInvalidState(bool useOfflineOnInvalidState);

 protected:
  std::string cmdUpOn;
  std::string cmdUpOff;
  std::string cmdDownOn;
  std::string cmdDownOff;
  uint32_t lastReadTime = 0;
  bool useOfflineOnInvalidState = false;
};

};  // namespace Control
};  // namespace Supla


#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CMD_ROLLER_SHUTTER_H_
