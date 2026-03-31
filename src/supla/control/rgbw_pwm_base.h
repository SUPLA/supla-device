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

#include "rgb_cct_base.h"

namespace Supla {
namespace Io {
class Base;
}
}  // namespace Supla

namespace Supla {
namespace Control {

class RGBWPwmBase : public RGBCCTBase {
 public:
  static constexpr int kMaxOutputs = 5;

  struct Output {
    int pin = -1;
    Supla::Io::Base *io = nullptr;
  };

  RGBWPwmBase(RGBWPwmBase *parent,
              int out1,
              int out2,
              int out3,
              int out4,
              int out5);

  void setRGBCCTValueOnDevice(uint32_t output[5], int usedOutputs) override;
  void onInit() override;

  void setOutputInvert(bool outputInvert) { this->outputInvert = outputInvert; }
  void setOutputIo(int outputIndex, Supla::Io::Base *io);
  Supla::Io::Base *getOutputIo(int outputIndex) const;
  int getOutputPin(int outputIndex) const;

 protected:
  virtual void initializePwmTimer() = 0;
  virtual int initializePwmChannel(int outputIndex) = 0;
  virtual void writePwmValue(int channel, uint32_t value) = 0;
  virtual void stopPwmChannel(int channel) = 0;

  int getPwmChannelForGpio(int gpio) const;
  static int &pwmChannelCounter();

  Output outputs[kMaxOutputs];
  int channels[kMaxOutputs] = {};
  int32_t channelPrevValue[kMaxOutputs] = {-1, -1, -1, -1, -1};
  int tryCounter = 0;
  RGBWPwmBase *parentPwm = nullptr;
  bool outputInvert = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RGBW_PWM_BASE_H_
