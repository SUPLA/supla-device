// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_GENERAL_PURPOSE_MEASUREMENT_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_GENERAL_PURPOSE_MEASUREMENT_PARSED_H_

#include <supla/parser/parser.h>
#include <supla/sensor/general_purpose_measurement.h>

#include <string>

#include "sensor_parsed.h"

namespace Supla {
namespace Parser {
const char Value[] = "value";
};

namespace Sensor {

class GeneralPurposeMeasurementParsed
    : public SensorParsed<GeneralPurposeMeasurement> {
 public:
  explicit GeneralPurposeMeasurementParsed(Supla::Parser::Parser *);
  double getValue() override;

 protected:
  bool isDataErrorLogged = false;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_GENERAL_PURPOSE_MEASUREMENT_PARSED_H_
