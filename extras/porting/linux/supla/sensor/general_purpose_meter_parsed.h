// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_GENERAL_PURPOSE_METER_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_GENERAL_PURPOSE_METER_PARSED_H_

#include <supla/parser/parser.h>
#include <supla/sensor/general_purpose_meter.h>
#include <supla/sensor/general_purpose_measurement_parsed.h>

#include <string>

#include "sensor_parsed.h"

namespace Supla {

namespace Sensor {

class GeneralPurposeMeterParsed
    : public SensorParsed<GeneralPurposeMeter> {
 public:
  explicit GeneralPurposeMeterParsed(Supla::Parser::Parser *);
  double getValue() override;

 protected:
  bool isDataErrorLogged = false;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_GENERAL_PURPOSE_METER_PARSED_H_
