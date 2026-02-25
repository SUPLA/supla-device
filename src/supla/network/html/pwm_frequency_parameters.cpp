/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "pwm_frequency_parameters.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <supla/clock/clock.h>
#include <supla/control/rgb_cct_base.h>
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
    Supla::Control::RGBCCTBase* rgbCct)
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

  // form-field BEGIN
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(Supla::ConfigTag::PwmFrequencyTag,
                       "PWM frequency [Hz] (reboot required to take effect)");
  sender->send("<input type=\"number\" min=\"");
  sender->send(minFrequency, 0);
  sender->send("\"\" max=\"");
  sender->send(maxFrequency, 0);
  sender->send("\" step =\"");
  sender->send(frequencyStep, 0);
  sender->send("\" ");
  sender->sendNameAndId(Supla::ConfigTag::PwmFrequencyTag);
  sender->send(" value=\"");
  sender->send(frequency, 0);
  sender->send("\">");
  sender->send("</div>");
  // form-field END
}

bool PwmFrequencyParameters::handleResponse(const char* key,
                                            const char* value) {
  if (strcmp(key, Supla::ConfigTag::PwmFrequencyTag) == 0) {
    uint32_t pwmFrequency = stringToUInt(value);
    // setPwmFrequency() will apply validation and will correct pwmFrequency
    // to allowed value
    if (pwmFrequency > UINT16_MAX) {
      pwmFrequency = UINT16_MAX;
    }
    rgbCct->setPwmFrequency(pwmFrequency);
    pwmFrequency = rgbCct->getPwmFrequency();

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
