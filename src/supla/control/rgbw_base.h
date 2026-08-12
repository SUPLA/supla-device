// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_RGBW_BASE_H_
#define SRC_SUPLA_CONTROL_RGBW_BASE_H_

#include <stdint.h>

#include "lighting_pwm_base.h"
#include "../action_handler.h"
#include "../channel_element.h"

#define RGBW_STATE_ON_INIT_RESTORE -1
#define RGBW_STATE_ON_INIT_OFF     0
#define RGBW_STATE_ON_INIT_ON      1

namespace Supla {
namespace Control {

class Button;

class RGBWBase : public LightingPwmBase {
 public:
  RGBWBase();

  virtual void setRGBWValueOnDevice(uint32_t red,
                                    uint32_t green,
                                    uint32_t blue,
                                    uint32_t whiteBrightness) = 0;

  void setRGBCCTValueOnDevice(uint32_t output[5], int usedOutputs) override;

  void onLoadState() override;
  void onSaveState() override;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RGBW_BASE_H_
