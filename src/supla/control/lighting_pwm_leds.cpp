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

#include "lighting_pwm_leds.h"

#include <supla/log_wrapper.h>

namespace Supla {
namespace Control {

LightingPwmLeds::LightingPwmLeds(
    LightingPwmLeds *parent, int out1, int out2, int out3, int out4, int out5)
    : LightingPwmBase(parent), parentPwm(parent) {
  outputs[0].pin.setPin(out1);
  outputs[1].pin.setPin(out2);
  outputs[2].pin.setPin(out3);
  outputs[3].pin.setPin(out4);
  outputs[4].pin.setPin(out5);
  for (auto &output : outputs) {
    output.pin.setMode(OUTPUT);
  }
}

LightingPwmLeds::LightingPwmLeds(LightingPwmLeds *parent,
                                 Supla::Io::IoPin out1,
                                 Supla::Io::IoPin out2,
                                 Supla::Io::IoPin out3,
                                 Supla::Io::IoPin out4,
                                 Supla::Io::IoPin out5)
    : LightingPwmBase(parent), parentPwm(parent) {
  outputs[0].pin = out1;
  outputs[1].pin = out2;
  outputs[2].pin = out3;
  outputs[3].pin = out4;
  outputs[4].pin = out5;
  for (auto &output : outputs) {
    output.pin.setMode(OUTPUT);
  }
}

void LightingPwmLeds::setOutputIo(int outputIndex, Supla::Io::Base *io) {
  if (outputIndex < 0 || outputIndex >= kMaxOutputs) {
    return;
  }
  outputs[outputIndex].pin.io = io;
}

Supla::Io::Base *LightingPwmLeds::getOutputIo(int outputIndex) const {
  if (outputIndex < 0 || outputIndex >= kMaxOutputs) {
    return nullptr;
  }
  return outputs[outputIndex].pin.io;
}

int LightingPwmLeds::getOutputPin(int outputIndex) const {
  if (outputIndex < 0 || outputIndex >= kMaxOutputs) {
    return -1;
  }
  return outputs[outputIndex].pin.getPin();
}

void LightingPwmLeds::setRGBCCTValueOnDevice(uint32_t output[5],
                                             int usedOutputs) {
  if (!initDone || !enabled) {
    return;
  }

  bool changed = false;
  for (int i = 0; i < usedOutputs; i++) {
    if (outputs[i].lastSourceValue != static_cast<int32_t>(output[i])) {
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
    outputs[i].lastSourceValue = static_cast<int32_t>(output[i]);
    uint32_t value = output[i];
    uint32_t outputMax = outputs[i].pin.analogWriteMaxValue();
    if (outputMax > 0 && outputMax != maxHwValue) {
      value = static_cast<uint32_t>(
          (static_cast<uint64_t>(value) * outputMax + maxHwValue / 2) /
          maxHwValue);
    }
    if (outputs[i].lastDutyValue == static_cast<int32_t>(value)) {
      continue;
    }
    outputs[i].lastDutyValue = static_cast<int32_t>(value);
    outputs[i].pin.analogWrite(value);
  }
}

void LightingPwmLeds::applyPwmFrequencyToOutputs() {
  const uint16_t frequency = getPwmFrequency();
  Supla::Io::Base *configuredIo[kMaxOutputs] = {};
  int configuredIoCount = 0;
  for (auto &output : outputs) {
    if (output.pin.io != nullptr) {
      bool alreadyConfigured = false;
      for (int i = 0; i < configuredIoCount; i++) {
        if (configuredIo[i] == output.pin.io) {
          alreadyConfigured = true;
          break;
        }
      }
      if (!alreadyConfigured) {
        configuredIo[configuredIoCount++] = output.pin.io;
        output.pin.setAnalogOutputFrequency(frequency);
      }
    }
  }
}

void LightingPwmLeds::onLoadConfig(SuplaDeviceClass *sdc) {
  LightingPwmBase::onLoadConfig(sdc);
  applyPwmFrequencyToOutputs();
}

void LightingPwmLeds::onInit() {
  if (initDone) {
    return;
  }

  uint32_t outputMaxValue = 0;
  for (const auto &output : outputs) {
    uint32_t value = output.pin.analogWriteMaxValue();
    if (value > outputMaxValue) {
      outputMaxValue = value;
    }
  }
  if (outputMaxValue > 0) {
    setMaxHwValue(static_cast<int>(outputMaxValue));
  }

  if (hasParent()) {
    SUPLA_LOG_DEBUG("Light[%d]: initialize parent PWM", getChannelNumber());
    parentPwm->onInit();
    LightingPwmBase::onInit();
    return;
  }

  applyPwmFrequencyToOutputs();

  for (auto &output : outputs) {
    output.pin.configureAnalogOutput();
    output.pin.pinMode();
  }

  LightingPwmBase::onInit();
}

}  // namespace Control
}  // namespace Supla
