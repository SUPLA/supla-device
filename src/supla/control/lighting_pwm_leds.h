// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_LIGHTING_PWM_LEDS_H_
#define SRC_SUPLA_CONTROL_LIGHTING_PWM_LEDS_H_

#include <stdint.h>
#include <supla/io.h>

#include "lighting_pwm_base.h"

namespace Supla {
namespace Control {

class LightingPwmLeds : public LightingPwmBase {
 public:
  static constexpr int kMaxOutputs = 5;

  LightingPwmLeds(LightingPwmLeds *parent,
                  int out1,
                  int out2,
                  int out3,
                  int out4,
                  int out5);
  LightingPwmLeds(LightingPwmLeds *parent,
                  Supla::Io::IoPin out1,
                  Supla::Io::IoPin out2 = {},
                  Supla::Io::IoPin out3 = {},
                  Supla::Io::IoPin out4 = {},
                  Supla::Io::IoPin out5 = {});

  void setRGBCCTValueOnDevice(uint32_t output[5], int usedOutputs) override;
  void onInit() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;

 protected:
  struct OutputState {
    Supla::Io::IoPin pin;
    int32_t lastSourceValue = -1;
    int32_t lastDutyValue = -1;
  };

  void applyPwmResolutionBitsToOutputs();
  void applyPwmFrequencyToOutputs();
  void applyDefaultChannelFunctions();
  int getConfiguredOutputsCount() const;
  uint8_t getPwmResolutionBitsForOutput(const OutputState &output) const;
  uint32_t getPwmMaxValueForOutput(const OutputState &output) const;
  bool isOutputSharedWithParent(const OutputState &output) const;

  OutputState outputs[kMaxOutputs];
  int lastUsedOutputs = 0;
  int tryCounter = 0;
  LightingPwmLeds *parentPwm = nullptr;
};

using RGBWPwmBase = LightingPwmLeds;

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_LIGHTING_PWM_LEDS_H_
