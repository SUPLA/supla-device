// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "pwm_frequency_parameters.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <supla/clock/clock.h>
#include <supla/control/lighting_pwm_base.h>
#include <supla/log_wrapper.h>
#include <supla/network/html_element.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

using Supla::Html::PwmFrequencyParameters;

constexpr uint32_t PWM_FREQUENCY_MIN = 100;
constexpr uint32_t PWM_FREQUENCY_MAX = 9000;
constexpr uint32_t PWM_FREQUENCY_STEP = 1;
constexpr uint32_t PWM_FREQUENCY_DEFAULT = 500;

PwmFrequencyParameters::PwmFrequencyParameters(
    Supla::Control::LightingPwmBase* rgbCct)
    : HtmlElement(HTML_SECTION_FORM), rgbCct(rgbCct) {
}

PwmFrequencyParameters::~PwmFrequencyParameters() {
}

void PwmFrequencyParameters::send(Supla::WebSender* sender) {
  uint16_t minFrequency = PWM_FREQUENCY_MIN;
  uint16_t maxFrequency = PWM_FREQUENCY_MAX;
  uint16_t frequency = PWM_FREQUENCY_DEFAULT;
  uint16_t frequencyStep = PWM_FREQUENCY_STEP;
  if (rgbCct) {
    minFrequency = rgbCct->getMinPwmFrequency();
    maxFrequency = rgbCct->getMaxPwmFrequency();
    frequency = rgbCct->getPwmFrequency();
    frequencyStep = rgbCct->getStepPwmFrequency();
  }
  sender->labeledField(Supla::ConfigTag::PwmFrequencyTag,
                       "PWM frequency [Hz] (reboot required to take effect)",
                       [&]() {
                         sender->numberInput(
                             Supla::ConfigTag::PwmFrequencyTag,
                             Supla::NumericInputSpec{
                                 .min = static_cast<int>(minFrequency),
                                 .max = static_cast<int>(maxFrequency),
                                 .value = static_cast<int>(frequency),
                                 .step = static_cast<int>(frequencyStep),
                             });
                       });
}

bool PwmFrequencyParameters::handleResponse(const char* key,
                                            const char* value) {
  if (strcmp(key, Supla::ConfigTag::PwmFrequencyTag) == 0) {
    uint32_t pwmFrequency = stringToUInt(value);
    if (pwmFrequency > UINT16_MAX) {
      pwmFrequency = UINT16_MAX;
    }

    if (rgbCct) {
      // setPwmFrequency() will apply validation and will correct
      // pwmFrequency to allowed value
      rgbCct->setPwmFrequency(pwmFrequency);
      pwmFrequency = rgbCct->getPwmFrequency();
    } else {
      pwmFrequency = Supla::Control::LightingPwmBase::normalizePwmFrequency(
          static_cast<uint16_t>(pwmFrequency),
          PWM_FREQUENCY_MIN,
          PWM_FREQUENCY_MAX,
          PWM_FREQUENCY_STEP);
    }

    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      if (!cfg->setUInt32(Supla::ConfigTag::PwmFrequencyTag, pwmFrequency)) {
        SUPLA_LOG_ERROR("Failed to save PWM frequency to config: %d",
                        pwmFrequency);
      } else {
        SUPLA_LOG_INFO("PWM frequency saved to config: %d", pwmFrequency);
      }
    }

    return true;
  }

  return false;
}

#endif  // ARDUINO_ARCH_AVR
