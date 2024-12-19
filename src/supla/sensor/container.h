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

#ifndef SRC_SUPLA_SENSOR_CONTAINER_H_
#define SRC_SUPLA_SENSOR_CONTAINER_H_

#include <supla/channel_element.h>

namespace Supla {
namespace Sensor {
class Container : public ChannelElement {
 public:
  Container();

  void iterateAlways() override;

  virtual int getValue();
  void setValue(int value);

  void setAlarmActive(bool alarmActive);
  bool isAlarmActive() const;

  void setWarningActive(bool warningActive);
  bool isWarningActive() const;

  void setInvalidSensorStateActive(bool invalidSensorStateActive);
  bool isInvalidSensorStateActive() const;

  void setSoundAlarmOn(bool soundAlarmOn);
  bool isSoundAlarmOn() const;

  virtual void setReadIntervalMs(uint32_t timeMs);

 protected:
  uint32_t lastReadTime = 0;
  uint32_t readIntervalMs = 1000;
  uint8_t fillLevel = 0;
};

};  // namespace Sensor
};  // namespace Supla


#endif  // SRC_SUPLA_SENSOR_CONTAINER_H_
