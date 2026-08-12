// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_ONE_PHASE_ELECTRICITY_METER_H_
#define SRC_SUPLA_SENSOR_ONE_PHASE_ELECTRICITY_METER_H_

#include "electricity_meter.h"

namespace Supla {
namespace Sensor {
class OnePhaseElectricityMeter : public ElectricityMeter {
 public:
  OnePhaseElectricityMeter() {
    extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE2_UNSUPPORTED);
    extChannel.setFlag(SUPLA_CHANNEL_FLAG_PHASE3_UNSUPPORTED);
  }

  virtual void readValuesFromDevice() {
  }

  void onInit() {
  }
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_ONE_PHASE_ELECTRICITY_METER_H_
