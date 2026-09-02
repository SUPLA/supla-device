// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_LEDC_IO_LEDC_IO_H_
#define EXTRAS_ESP_IDF_LEDC_IO_LEDC_IO_H_

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
  void customSetPwmResolutionBits(uint8_t pin, uint8_t resolutionBits) override;
  void customConfigureAnalogOutput(int channelNumber,
                                   uint8_t pin,
                                   bool outputInvert) override;
  void customSetPwmFrequency(uint16_t pwmFrequency) override;
  uint8_t customPwmResolutionBits(uint8_t pin) const override;
  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override;

 private:
  bool ensureTimerConfigured();
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

#endif  // EXTRAS_ESP_IDF_LEDC_IO_LEDC_IO_H_
