// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_RGBCCT_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_RGBCCT_PARSED_H_

#include <supla/control/lighting_pwm_base.h>
#include <supla/sensor/sensor_parsed.h>

namespace Supla {
namespace Control {
class RgbCctParsed : public Sensor::SensorParsed<LightingPwmBase> {
 public:
  explicit RgbCctParsed(Supla::Parser::Parser *parser);

  void iterateAlways() override;

  void setRGBCCTValueOnDevice(uint32_t output[5], int usedOutputs) override;

 protected:
  uint32_t lastReadTime = 0;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_RGBCCT_PARSED_H_
