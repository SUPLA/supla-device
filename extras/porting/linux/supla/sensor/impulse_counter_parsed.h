// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_IMPULSE_COUNTER_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_IMPULSE_COUNTER_PARSED_H_

#include <supla/parser/parser.h>
#include <supla/sensor/virtual_impulse_counter.h>

#include <string>

#include "sensor_parsed.h"

namespace Supla {
namespace Parser {
const char Counter[] = "counter";
};

namespace Sensor {

class ImpulseCounterParsed : public SensorParsed<VirtualImpulseCounter> {
 public:
  explicit ImpulseCounterParsed(Supla::Parser::Parser *);

  virtual uint64_t getValue();
  void onInit() override;
  void iterateAlways() override;

 protected:
  uint32_t lastReadTime = 0;
  bool isDataErrorLogged = false;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_IMPULSE_COUNTER_PARSED_H_
