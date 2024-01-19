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

// DEPRECATED: please use channel based on GPM instead (gpm_esp_free_heap.h)

#ifndef SRC_SUPLA_SENSOR_ESP_FREE_HEAP_H_
#define SRC_SUPLA_SENSOR_ESP_FREE_HEAP_H_

#include "supla/sensor/thermometer.h"

namespace Supla {
namespace Sensor {
class EspFreeHeap : public Thermometer {
 public:
  void onInit() {
    channel.setNewValue(getValue());
  }

  double getValue() {
    return ESP.getFreeHeap() / 1024.0;
  }

 protected:
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_ESP_FREE_HEAP_H_
