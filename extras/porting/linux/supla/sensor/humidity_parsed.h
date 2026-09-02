// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_HUMIDITY_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_HUMIDITY_PARSED_H_

#include <supla/parser/parser.h>
#include <supla/sensor/hygro_meter.h>

#include <string>

#include "sensor_parsed.h"

namespace Supla {
namespace Parser {
const char Humidity[] = "humidity";
};

namespace Sensor {

class HumidityParsed : public SensorParsed<HygroMeter> {
 public:
  explicit HumidityParsed(Supla::Parser::Parser *);
  double getHumi() override;
  void onInit() override;

 protected:
  bool isDataErrorLogged = false;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_HUMIDITY_PARSED_H_
