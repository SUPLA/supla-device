// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_GPM_ESP_FREE_HEAP_H_
#define SRC_SUPLA_SENSOR_GPM_ESP_FREE_HEAP_H_

#include "general_purpose_measurement.h"

#include <Esp.h>

namespace Supla {
namespace Sensor {
class GpmEspFreeHeap : public GeneralPurposeMeasurement {
 public:
  GpmEspFreeHeap() : GeneralPurposeMeasurement(nullptr, false) {
    setDefaultUnitAfterValue("B");
  }

  void useKB() {
    setDefaultUnitAfterValue("KB");
    setDefaultValueDivider(1024000);
  }

 protected:
  double getValue() override {
    return ESP.getFreeHeap();
  }
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_GPM_ESP_FREE_HEAP_H_
