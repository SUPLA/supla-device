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

#include "ledc_io.h"

#include <string.h>
#include <supla/log_wrapper.h>

namespace {
constexpr int MaxLedcChannels = LEDC_CHANNEL_MAX;
constexpr int MaxLedcTimers = LEDC_TIMER_MAX;
constexpr int MaxPins = 256;
constexpr ledc_mode_t kLedcSpeedMode = LEDC_LOW_SPEED_MODE;

int &LedcTimerCounter() {
  static int counter = 0;
  return counter;
}
}  // namespace

namespace Supla::Io {

LedcIo::LedcIo() : Base(false), ledcTimerId(LedcTimerCounter()++) {
  memset(pinToChannel, -1, sizeof(pinToChannel));
  if (ledcTimerId < 0 || ledcTimerId >= MaxLedcTimers) {
    SUPLA_LOG_ERROR("[LEDC] timer %d out of range", ledcTimerId);
  }
}

LedcIo::LedcIo(int ledcTimerId) : Base(false), ledcTimerId(ledcTimerId) {
  memset(pinToChannel, -1, sizeof(pinToChannel));
  if (ledcTimerId < 0 || ledcTimerId >= MaxLedcTimers) {
    SUPLA_LOG_ERROR("[LEDC] timer %d out of range", ledcTimerId);
  }
}

void LedcIo::setPwmFrequency(uint16_t pwmFrequency) {
  if (this->pwmFrequency == pwmFrequency) {
    return;
  }
  this->pwmFrequency = pwmFrequency;
  if (timerConfigured) {
    timerConfigured = false;
    ensureTimerConfigured();
  }
}

void LedcIo::setResolutionBits(uint8_t resolutionBits) {
  if (this->resolutionBits == resolutionBits) {
    return;
  }
  if (resolutionBits < 1) {
    resolutionBits = 1;
  }
  if (resolutionBits > 13) {
    resolutionBits = 13;
  }
  this->resolutionBits = resolutionBits;
  if (timerConfigured) {
    timerConfigured = false;
    ensureTimerConfigured();
  }
}

uint32_t LedcIo::getMaxDuty() const {
  return (1UL << resolutionBits) - 1;
}

bool LedcIo::ensureTimerConfigured() {
  if (timerConfigured) {
    return true;
  }

  if (ledcTimerId < 0 || ledcTimerId >= MaxLedcTimers) {
    SUPLA_LOG_ERROR("[LEDC] timer %d out of range", ledcTimerId);
    return false;
  }

  SUPLA_LOG_DEBUG("[LEDC] configuring timer %d, freq %d, res %d",
                  ledcTimerId,
                  pwmFrequency,
                  resolutionBits);

  ledc_timer_config_t ledcTimer = {};
  ledcTimer.speed_mode = kLedcSpeedMode;
  ledcTimer.duty_resolution = static_cast<ledc_timer_bit_t>(resolutionBits);
  ledcTimer.timer_num = static_cast<ledc_timer_t>(ledcTimerId);
  ledcTimer.freq_hz = pwmFrequency;
  ledcTimer.clk_cfg = LEDC_AUTO_CLK;
  ledcTimer.deconfigure = false;

  esp_err_t err = ledc_timer_config(&ledcTimer);
  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("[LEDC] timer %d configuration failed: %d",
                    ledcTimerId,
                    static_cast<int>(err));
    return false;
  }
  timerConfigured = true;
  SUPLA_LOG_DEBUG("[LEDC] timer %d configured", ledcTimerId);
  return true;
}

int LedcIo::getConfiguredChannel(uint8_t pin) const {
  if (pin >= MaxPins) {
    return -1;
  }
  if (pinToChannel[pin] >= 0) {
    return pinToChannel[pin];
  }
  return -1;
}

int LedcIo::ensureChannelConfigured(uint8_t pin, bool outputInvert) {
  if (pin >= MaxPins) {
    SUPLA_LOG_WARNING("[LEDC] pin %d out of range", pin);
    return -1;
  }

  int channel = getConfiguredChannel(pin);
  if (channel < 0) {
    channel = nextChannel++;
    if (channel >= MaxLedcChannels) {
      SUPLA_LOG_ERROR("[LEDC] no free channel for pin %d", pin);
      return -1;
    }
    pinToChannel[pin] = static_cast<int8_t>(channel);
  }

  if (channel >= MaxLedcChannels) {
    SUPLA_LOG_ERROR("[LEDC] no free channel for pin %d", pin);
    return -1;
  }

  if (!ensureTimerConfigured()) {
    return -1;
  }

  ledc_channel_config_t ledcChannel = {};
  ledcChannel.gpio_num = pin;
  ledcChannel.speed_mode = kLedcSpeedMode;
  ledcChannel.channel = static_cast<ledc_channel_t>(channel);
  ledcChannel.timer_sel = static_cast<ledc_timer_t>(ledcTimerId);
  ledcChannel.duty = 0;
  ledcChannel.hpoint = 0;
  ledcChannel.flags.output_invert = outputInvert ? 1 : 0;

  esp_err_t err = ledc_channel_config(&ledcChannel);
  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("[LEDC] channel %d configuration failed for GPIO %d: %d",
                    channel,
                    pin,
                    static_cast<int>(err));
    return -1;
  }
  ESP_ERROR_CHECK(ledc_set_duty(kLedcSpeedMode,
                                static_cast<ledc_channel_t>(channel),
                                0));
  ESP_ERROR_CHECK(ledc_update_duty(kLedcSpeedMode,
                                   static_cast<ledc_channel_t>(channel)));
  ESP_ERROR_CHECK(
      ledc_stop(kLedcSpeedMode, static_cast<ledc_channel_t>(channel), 0));

  SUPLA_LOG_DEBUG("[LEDC] channel %d initialized for GPIO %d", channel, pin);
  return channel;
}

void LedcIo::customPinMode(int channelNumber, uint8_t pin, uint8_t mode) {
  (void)(channelNumber);
  (void)(pin);
  (void)(mode);
}

void LedcIo::customDigitalWrite(int channelNumber, uint8_t pin, uint8_t val) {
  (void)(channelNumber);
  (void)(pin);
  (void)(val);
}

void LedcIo::customSetPwmResolutionBits(uint8_t pin, uint8_t resolutionBits) {
  (void)(pin);
  setResolutionBits(resolutionBits);
}

void LedcIo::customConfigureAnalogOutput(int channelNumber,
                                         uint8_t pin,
                                         bool outputInvert) {
  (void)(channelNumber);
  if (!ensureTimerConfigured()) {
    return;
  }
  ensureChannelConfigured(pin, outputInvert);
}

void LedcIo::customSetPwmFrequency(uint16_t pwmFrequency) {
  setPwmFrequency(pwmFrequency);
}

uint8_t LedcIo::customPwmResolutionBits(uint8_t pin) const {
  (void)(pin);
  return resolutionBits;
}

void LedcIo::customAnalogWrite(int channelNumber, uint8_t pin, int val) {
  (void)(channelNumber);
  int channel = getConfiguredChannel(pin);
  if (channel < 0) {
    return;
  }

  if (val < 0) {
    val = 0;
  }

  const uint32_t maxDuty = getMaxDuty();
  if (static_cast<uint32_t>(val) > maxDuty) {
    val = static_cast<int>(maxDuty);
  }
  const uint32_t duty = static_cast<uint32_t>(val);

  auto channelId = static_cast<ledc_channel_t>(channel);
  ledc_set_duty(kLedcSpeedMode, channelId, duty);
  ledc_update_duty(kLedcSpeedMode, channelId);
  if (duty == 0) {
    ledc_stop(kLedcSpeedMode, channelId, 0);
  }
}

}  // namespace Supla::Io
