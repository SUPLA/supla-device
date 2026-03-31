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

#include "rgbw_pwm_base.h"

#include <supla/log_wrapper.h>

namespace Supla {
namespace Control {

#ifdef ARDUINO_ARCH_ESP32
extern int esp32PwmChannelCounter;
#endif

int &RGBWPwmBase::pwmChannelCounter() {
#ifdef ARDUINO_ARCH_ESP32
  return esp32PwmChannelCounter;
#else
  static int counter = 0;
  return counter;
#endif
}

RGBWPwmBase::RGBWPwmBase(RGBWPwmBase *parent,
                         int out1,
                         int out2,
                         int out3,
                         int out4,
                         int out5)
    : RGBCCTBase(parent), parentPwm(parent) {
  outputs[0].pin = out1;
  outputs[1].pin = out2;
  outputs[2].pin = out3;
  outputs[3].pin = out4;
  outputs[4].pin = out5;
}

void RGBWPwmBase::setOutputIo(int outputIndex, Supla::Io::Base *io) {
  if (outputIndex < 0 || outputIndex >= kMaxOutputs) {
    return;
  }
  outputs[outputIndex].io = io;
}

Supla::Io::Base *RGBWPwmBase::getOutputIo(int outputIndex) const {
  if (outputIndex < 0 || outputIndex >= kMaxOutputs) {
    return nullptr;
  }
  return outputs[outputIndex].io;
}

int RGBWPwmBase::getOutputPin(int outputIndex) const {
  if (outputIndex < 0 || outputIndex >= kMaxOutputs) {
    return -1;
  }
  return outputs[outputIndex].pin;
}

void RGBWPwmBase::setRGBCCTValueOnDevice(uint32_t output[5], int usedOutputs) {
  if (!initDone || !enabled) {
    return;
  }

  bool changed = false;
  for (int i = 0; i < usedOutputs; i++) {
    if (channelPrevValue[i] != static_cast<int32_t>(output[i])) {
      tryCounter = 0;
      changed = true;
      break;
    }
  }

  tryCounter++;

  if (!changed && tryCounter > 10) {
    tryCounter = 10;
    return;
  }

  for (int i = 0; i < usedOutputs; i++) {
    channelPrevValue[i] = static_cast<int32_t>(output[i]);
    if (channels[i] >= 0) {
      writePwmValue(channels[i], output[i]);
      if (output[i] == 0) {
        stopPwmChannel(channels[i]);
      }
    }
  }
}

void RGBWPwmBase::onInit() {
  if (initDone) {
    return;
  }

  if (hasParent()) {
    SUPLA_LOG_DEBUG("RGBW[%d]: initialize parent PWM", getChannelNumber());
    parentPwm->onInit();
    for (int i = 0; i < kMaxOutputs; i++) {
      if (outputs[i].pin >= 0) {
        channels[i] = parentPwm->getPwmChannelForGpio(outputs[i].pin);
        SUPLA_LOG_DEBUG("RGBW[%d]: channel %d assigned to GPIO %d",
                        getChannelNumber(),
                        channels[i],
                        outputs[i].pin);
      }
    }
  } else {
    if (pwmChannelCounter() == 0) {
      SUPLA_LOG_DEBUG("RGBW[%d]: initialize PWM backend", getChannelNumber());
      initializePwmTimer();
    }

    for (int i = 0; i < kMaxOutputs; i++) {
      if (outputs[i].pin >= 0) {
        channels[i] = initializePwmChannel(i);
      }
    }
  }

  RGBCCTBase::onInit();
}

int RGBWPwmBase::getPwmChannelForGpio(int gpio) const {
  if (parentPwm) {
    return parentPwm->getPwmChannelForGpio(gpio);
  }

  for (int i = 0; i < kMaxOutputs; i++) {
    if (outputs[i].pin == gpio) {
      return channels[i];
    }
  }

  SUPLA_LOG_WARNING("RGBW[%d]: no PWM channel for GPIO %d",
                    getChannelNumber(),
                    gpio);
  return -1;
}

}  // namespace Control
}  // namespace Supla
