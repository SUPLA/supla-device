// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_BINARY_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_BINARY_PARSED_H_

#include <supla/parser/parser.h>
#include <supla/sensor/virtual_binary.h>

#include <string>

#include "sensor_parsed.h"

namespace Supla {
namespace Parser {
const char State[] = "state";
const char StateOnValues[] = "state_on_values";
const char ActionTrigger[] = "action_trigger";
};  // namespace Parser

namespace Sensor {

class BinaryParsed : public SensorParsed<VirtualBinary> {
 public:
  explicit BinaryParsed(Supla::Parser::Parser *);

  void onInit() override;
  bool getValue() override;
  void iterateAlways() override;

 protected:
  uint32_t lastOfflineReadTime = 0;
  bool lastSourceState = false;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_BINARY_PARSED_H_
