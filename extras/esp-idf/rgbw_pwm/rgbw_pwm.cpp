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
#include <supla/log_wrapper.h>

using Supla::Control::RGBWLedsEspIdf;

RGBWLedsEspIdf::RGBWLedsEspIdf(Supla::Control::RGBWPwmBase *parent,
                               int ledcTimerId,
                               int out1Gpio,
                               int out2Gpio,
                               int out3Gpio,
                               int out4Gpio,
                               int out5Gpio)
    : RGBWPwmBase(parent, out1Gpio, out2Gpio, out3Gpio, out4Gpio, out5Gpio),
      ledcTimerId(ledcTimerId) {
  setMaxHwValue(8192 - 1);
}

void RGBWLedsEspIdf::initializePwmTimer() {
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

int RGBWLedsEspIdf::initializePwmChannel(int outputIndex) {
  int gpio = getOutputPin(outputIndex);
  ledc_channel_t channelId =
      static_cast<ledc_channel_t>(RGBWPwmBase::pwmChannelCounter()++);

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
                  getChannelNumber(),
                  channelId,
                  gpio);

  return static_cast<int>(channelId);
}

void RGBWLedsEspIdf::writePwmValue(int channel, uint32_t value) {
  auto channelId = static_cast<ledc_channel_t>(channel);
  ledc_set_duty(LEDC_HIGH_SPEED_MODE, channelId, value);
  ledc_update_duty(LEDC_HIGH_SPEED_MODE, channelId);
}

void RGBWLedsEspIdf::stopPwmChannel(int channel) {
  ledc_stop(LEDC_HIGH_SPEED_MODE, static_cast<ledc_channel_t>(channel), 0);
}
