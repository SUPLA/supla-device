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

#ifndef EXTRAS_ESP_IDF_RGBW_PWM_LEDC_IO_H_
#define EXTRAS_ESP_IDF_RGBW_PWM_LEDC_IO_H_

#include <driver/ledc.h>

#include <supla/io.h>

namespace Supla::Io {

class LedcIo : public Supla::Io::Base {
 public:
  LedcIo();
  explicit LedcIo(int ledcTimerId);

  void setPwmFrequency(uint16_t pwmFrequency);
  void setResolutionBits(uint8_t resolutionBits);

  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override;
  void customDigitalWrite(int channelNumber, uint8_t pin, uint8_t val) override;
  void customSetPwmResolutionBits(uint8_t resolutionBits) override;
  void customConfigureAnalogOutput(int channelNumber,
                                   uint8_t pin,
                                   bool outputInvert) override;
  void customSetPwmFrequency(uint16_t pwmFrequency) override;
  uint8_t customAnalogWriteResolutionBits() const override;
  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override;

 private:
  void ensureTimerConfigured();
  int ensureChannelConfigured(uint8_t pin, bool outputInvert);
  int getConfiguredChannel(uint8_t pin) const;
  uint32_t getMaxDuty() const;

  int ledcTimerId = 0;
  uint16_t pwmFrequency = 500;
  uint8_t resolutionBits = 10;
  bool timerConfigured = false;
  int nextChannel = 0;
  int8_t pinToChannel[256];
};

}  // namespace Supla::Io

namespace Supla {
using LedcIo = Io::LedcIo;
}

#endif  // EXTRAS_ESP_IDF_RGBW_PWM_LEDC_IO_H_
