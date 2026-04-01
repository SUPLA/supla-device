/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef SRC_SUPLA_CONTROL_RGBW_PWM_BASE_H_
#define SRC_SUPLA_CONTROL_RGBW_PWM_BASE_H_

#include <stdint.h>

#include "lighting_pwm_base.h"
#include <supla/io.h>

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

  void setOutputIo(int outputIndex, Supla::Io::Base *io);
  Supla::Io::Base *getOutputIo(int outputIndex) const;
  int getOutputPin(int outputIndex) const;

 protected:
  int getPwmChannelForGpio(int gpio) const;
  void applyPwmFrequencyToOutputs();

  Supla::Io::IoPin outputs[kMaxOutputs];
  int32_t channelPrevValue[kMaxOutputs] = {-1, -1, -1, -1, -1};
  int32_t lastAnalogWriteValue[kMaxOutputs] = {-1, -1, -1, -1, -1};
  int tryCounter = 0;
  LightingPwmLeds *parentPwm = nullptr;
};

using RGBWPwmBase = LightingPwmLeds;

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RGBW_PWM_BASE_H_
