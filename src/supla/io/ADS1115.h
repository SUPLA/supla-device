// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/*
Dependency: https://github.com/RobTillaart/ADS1X15
Use library manager to install it
*/

#include <ADS1X15.h>

#include <supla/io.h>
#include <supla/mutex.h>
#include <supla/element.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Io {

class ADS1115 : public Supla::Io::Base {
 public:
  explicit ADS1115(uint8_t address = 0x48,
                   Supla::Mutex *mutex = nullptr,
                   TwoWire *wire = &Wire,
                   uint8_t dataRrate = 7)
      : Supla::Io::Base(), ads_(address, wire), mutex_(mutex) {
    if (!ads_.begin()) {
      SUPLA_LOG_ERROR("Unable to find ADS1115 at address 0x%x", address);
    } else {
      ads_.setDataRate(dataRrate);
      ads_.setMode(0);
      ads_.readADC(0);
      SUPLA_LOG_DEBUG("ADS1115 is connected at address: 0x%x, Gain: %d, "
                  "DataRate: %d", address, ads_.getGain(), ads_.getDataRate());
    }
  }

  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override {}

  void customDigitalWrite(int channelNumber, uint8_t pin,
                                                       uint8_t val) override {}

  int customDigitalRead(int channelNumber, uint8_t pin) override {
    return 0;
  }

  unsigned int customPulseIn(int channelNumber, uint8_t pin, uint8_t value,
                                              uint64_t timeoutMicro) override {
    return 0;
  }

  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override {}

  int customAnalogRead(int channelNumber, uint8_t pin) override {
    if (pin > 3) {
      SUPLA_LOG_WARNING("[ADS1115] invalid pin %d", pin);
      return -1;
    }
    if (mutex_) mutex_->lock();
    if (ads_.isConnected()) {
      ads_.setGain(gain_);
      readValue_[pin] = ads_.readADC(pin);
    }
    if (mutex_) mutex_->unlock();
    return readValue_[pin];
  }

  void setGain(uint8_t value) {
    gain_ = value;
  }

 protected:
  ::ADS1115 ads_;
  uint8_t gain_ = 0;
  int16_t readValue_[4] = {-1, -1, -1, -1};
  Supla::Mutex *mutex_ = nullptr;
};

};  // namespace Io
};  // namespace Supla
