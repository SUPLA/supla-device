// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_THERM_HYGRO_METER_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_THERM_HYGRO_METER_PARSED_H_

#include <supla/parser/parser.h>
#include <supla/sensor/therm_hygro_meter.h>

#include <string>

#include "sensor_parsed.h"
#include "thermometer_parsed.h"
#include "humidity_parsed.h"

namespace Supla {
namespace Sensor {

class ThermHygroMeterParsed : public SensorParsed<ThermHygroMeter> {
 public:
  explicit ThermHygroMeterParsed(Supla::Parser::Parser *);
  double getTemp() override;
  double getHumi() override;

 protected:
  bool isDataErrorLogged = false;
};
};  // namespace Sensor
};  // namespace Supla


#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_THERM_HYGRO_METER_PARSED_H_
