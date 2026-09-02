// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_WIND_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_WIND_PARSED_H_

#include <supla/parser/parser.h>
#include <supla/sensor/wind.h>

#include <string>

#include "sensor_parsed.h"

namespace Supla {
namespace Parser {
const char Wind[] = "wind";
};

namespace Sensor {

class WindParsed : public SensorParsed<Wind> {
 public:
  explicit WindParsed(Supla::Parser::Parser *);
  double getValue() override;
  void onInit() override;

 protected:
  bool isDataErrorLogged = false;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_WIND_PARSED_H_
