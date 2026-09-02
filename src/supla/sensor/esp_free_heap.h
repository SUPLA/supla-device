// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
