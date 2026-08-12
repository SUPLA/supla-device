// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_CONTAINER_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_CONTAINER_PARSED_H_

#include <supla/parser/parser.h>
#include <supla/sensor/container.h>

#include "sensor_parsed.h"

namespace Supla {
namespace Parser {
const char Level[] = "level";
};

namespace Sensor {

class ContainerParsed : public SensorParsed<Container> {
 public:
  explicit ContainerParsed(Supla::Parser::Parser *);
  int readNewValue() override;
  void onInit() override;

 protected:
  bool isDataErrorLogged = false;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_CONTAINER_PARSED_H_
