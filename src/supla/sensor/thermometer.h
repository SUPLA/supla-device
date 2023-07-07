/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_SENSOR_THERMOMETER_H_
#define SRC_SUPLA_SENSOR_THERMOMETER_H_

#include "supla/channel_element.h"

#define TEMPERATURE_NOT_AVAILABLE -275.0

namespace Supla {
namespace Sensor {
class Thermometer : public ChannelElement {
 public:
  Thermometer();
  virtual double getValue();
  void onInit() override;
  void onLoadConfig(SuplaDeviceClass *) override;
  void iterateAlways() override;

 protected:
  uint32_t lastReadTime;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THERMOMETER_H_
