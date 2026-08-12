// SPDX-FileCopyrightText: malarz
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_THERM_PRESS_METER_H_
#define SRC_SUPLA_SENSOR_THERM_PRESS_METER_H_

#include "therm_hygro_press_meter.h"

namespace Supla {
namespace Sensor {
class ThermPressMeter : public ThermHygroPressMeter {
 public:
  ThermPressMeter();
  void iterateAlways() override;

 protected:
  void setHumidityCorrection(int32_t correction) override;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THERM_PRESS_METER_H_
