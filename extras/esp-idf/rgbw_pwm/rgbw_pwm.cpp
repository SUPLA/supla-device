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

#include "rgbw_pwm.h"

#include <driver/ledc.h>
#include <supla/io.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

using Supla::Control::RGBWLedsEspIdf;

int RGBWLedsEspIdf::pwmChannelCounter = 0;

RGBWLedsEspIdf::RGBWLedsEspIdf(Supla::Control::RGBWLedsEspIdf *parent,
                               int ledcTimerId,
                               int redPin,
                               int greenPin,
                               int bluePin,
                               int w1BrightnessPin,
                               int w2BrightnessPin)
    : RGBCCTBase(parent), ledcTimerId(ledcTimerId), parentLedsEspIdf(parent) {
  gpios[0] = redPin;
  gpios[1] = greenPin;
  gpios[2] = bluePin;
  gpios[3] = w1BrightnessPin;
  gpios[4] = w2BrightnessPin;

  setMaxHwValue(8192 - 1);
}

void RGBWLedsEspIdf::setRGBCCTValueOnDevice(uint32_t red,
                                            uint32_t green,
                                            uint32_t blue,
                                            uint32_t colorBrightness,
                                            uint32_t w1Brightness,
                                            uint32_t w2Brightness) {
  if (!initDone || !enabled) {
    return;
  }
  uint32_t valueAdj[RGB_CCT_MAX] = {0};
  int usedChannels = 0;
  switch (getChannel()->getDefaultFunction()) {
    case SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB: {
      valueAdj[0] = red * colorBrightness / maxHwValue;
      valueAdj[1] = green * colorBrightness / maxHwValue;
      valueAdj[2] = blue * colorBrightness / maxHwValue;
      valueAdj[3] = w1Brightness;
      valueAdj[4] = w2Brightness;
      usedChannels = 5;
      break;
    }
    case SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING: {
      valueAdj[0] = red * colorBrightness / maxHwValue;
      valueAdj[1] = green * colorBrightness / maxHwValue;
      valueAdj[2] = blue * colorBrightness / maxHwValue;
      valueAdj[3] = w1Brightness;
      usedChannels = 4;
      break;
    }
    case SUPLA_CHANNELFNC_RGBLIGHTING: {
      valueAdj[0] = red * colorBrightness / maxHwValue;
      valueAdj[1] = green * colorBrightness / maxHwValue;
      valueAdj[2] = blue * colorBrightness / maxHwValue;
      usedChannels = 3;
      break;
    }
    case SUPLA_CHANNELFNC_DIMMER_CCT: {
      valueAdj[0] = w1Brightness;
      valueAdj[1] = w2Brightness;
      usedChannels = 2;
      break;
    }
    case SUPLA_CHANNELFNC_DIMMER: {
      valueAdj[0] = w1Brightness;
      usedChannels = 1;
      break;
    }
    default:
      break;
  }

  bool changed = false;
  for (int i = 0; i < usedChannels; i++) {
    if (channelPrevValue[i] != valueAdj[i]) {
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

  for (int i = 0; i < usedChannels; i++) {
    channelPrevValue[i] = valueAdj[i];
    if (gpios[i] >= 0) {
      ledc_set_duty(LEDC_HIGH_SPEED_MODE, channels[i], valueAdj[i]);
      ledc_update_duty(LEDC_HIGH_SPEED_MODE, channels[i]);
      if (valueAdj[i] == 0) {
        ledc_stop(LEDC_HIGH_SPEED_MODE, channels[i], 0);
      }
    }
  }
}

ledc_channel_t RGBWLedsEspIdf::initChannel(int gpio) {
  ledc_channel_t channelId = static_cast<ledc_channel_t>(pwmChannelCounter);
  pwmChannelCounter++;

  ledc_channel_config_t ledcChannel = {};

  ledcChannel.gpio_num = gpio;
  ledcChannel.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledcChannel.channel = channelId;
  ledcChannel.timer_sel = static_cast<ledc_timer_t>(ledcTimerId);
  ledcChannel.duty = 0;
  ledcChannel.hpoint = 0;

  ledcChannel.flags.output_invert = outputInvert ? 1 : 0;

  ESP_ERROR_CHECK(ledc_channel_config(&ledcChannel));
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, channelId, 0));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, channelId));
  ESP_ERROR_CHECK(ledc_stop(LEDC_HIGH_SPEED_MODE, channelId, 0));

  SUPLA_LOG_DEBUG("RGBW[%d]: LEDC PWM channel %d initialized for GPIO %d",
                  getChannelNumber(),  // Supla channel
                  channelId,           // ESP-IDF PWM channel
                  gpio);

  return channelId;
}

void RGBWLedsEspIdf::onInit() {
  if (initDone) {
    return;
  }
  if (hasParent()) {
    SUPLA_LOG_DEBUG("RGBW[%d]: initialize parent RGBW", getChannelNumber());
    parentLedsEspIdf->onInit();
    for (int i = 0; i < RGB_CCT_MAX; i++) {
      if (gpios[i] >= 0) {
        channels[i] = parentLedsEspIdf->getLedcChannel(gpios[i]);
        SUPLA_LOG_DEBUG("RGBW[%d]: channel %d assigned to GPIO %d",
                        getChannelNumber(),
                        i,
                        gpios[i]);
      }
    }
  } else {
    if (pwmChannelCounter == 0) {
      SUPLA_LOG_DEBUG("RGBW[%d]: initialize LEDC timer %d",
                      getChannelNumber(),
                      ledcTimerId);
      ledc_timer_config_t ledcTimer = {
          .speed_mode = LEDC_HIGH_SPEED_MODE,
          .duty_resolution = LEDC_TIMER_13_BIT,
          .timer_num = static_cast<ledc_timer_t>(ledcTimerId),
          .freq_hz = pwmFrequency,
          .clk_cfg = LEDC_AUTO_CLK,
          .deconfigure = false};
      ESP_ERROR_CHECK(ledc_timer_config(&ledcTimer));
    }

    for (int i = 0; i < RGB_CCT_MAX; i++) {
      if (gpios[i] >= 0) {
        channels[i] = initChannel(gpios[i]);
      }
    }
  }

  Supla::Control::RGBCCTBase::onInit();
}

ledc_channel_t RGBWLedsEspIdf::getLedcChannel(int gpio) const {
  if (parentLedsEspIdf) {
    return parentLedsEspIdf->getLedcChannel(gpio);
  }
  for (int i = 0; i < RGB_CCT_MAX; i++) {
    if (gpios[i] == gpio) {
      return channels[i];
    }
  }
  return LEDC_CHANNEL_0;
}

