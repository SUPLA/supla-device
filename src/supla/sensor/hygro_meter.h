// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_HYGRO_METER_H_
#define SRC_SUPLA_SENSOR_HYGRO_METER_H_

#include "therm_hygro_meter.h"


namespace Supla {
namespace Sensor {
class HygroMeter : public ThermHygroMeter {
 public:
  HygroMeter();
 protected:
  void setTemperatureCorrection(int32_t correction) override;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_HYGRO_METER_H_
