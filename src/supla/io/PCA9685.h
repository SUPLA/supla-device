// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/*
Version: 25.11.24
Dependency: https://github.com/RobTillaart/PCA9685_RT
Use library manager to install it
*/

#include <PCA9685.h>

#include <supla/io.h>
#include <supla/mutex.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Io {

class PCA9685 : public Supla::Io::Base {
 public:
  explicit PCA9685(uint8_t address = 0x40,
                   Supla::Mutex *mutex = nullptr,
                   TwoWire *wire = &Wire)
      : Supla::Io::Base(), pca_(address, wire), mutex_(mutex) {
    if (!pca_.begin()) {
      SUPLA_LOG_ERROR("Unable to find PCA9685 at address 0x%x", address);
    } else {
      SUPLA_LOG_DEBUG("PCA9685 is connected at address: 0x%x, "
                          "with PWM freq: %d Hz", address, pca_.getFrequency());
    }
  }

  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override {
  }
  void customDigitalWrite(int channelNumber, uint8_t pin,
                                                         uint8_t val) override {
    if (mutex_) mutex_->lock();
    if (pca_.isConnected()) {
      pca_.write1(pin, val);
    }
    if (mutex_) mutex_->unlock();
  }
  int customDigitalRead(int channelNumber, uint8_t pin) override {
    uint8_t val = 0;
    if (mutex_) mutex_->lock();
    if (pca_.isConnected()) {
      val = pca_.read1(pin);
    }
    if (mutex_) mutex_->unlock();
    return (val == 1) ? 1 : 0;
  }
  unsigned int customPulseIn(int channelNumber, uint8_t pin, uint8_t value,
                                               uint64_t timeoutMicro) override {
    return 0;
  }
  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override {
    if (mutex_) mutex_->lock();
    if (pca_.isConnected()) {
      val = map(val, 0, 1023, 0, 4095);
      pca_.setPWM(pin, val);
    }
    if (mutex_) mutex_->unlock();
  }

  int customAnalogRead(int channelNumber, uint8_t pin) override {
    return 0;
  }

  // Default frequency: 200 Hz
  void setPWMFrequency(uint16_t frequency) {
    if (mutex_) mutex_->lock();
    if (pca_.isConnected()) {
      pca_.setFrequency(frequency);
      SUPLA_LOG_DEBUG("[PCA9685] set PWM frequency: %d Hz",
                                                           pca_.getFrequency());
    }
    if (mutex_) mutex_->unlock();
  }

 protected:
  ::PCA9685 pca_;
  Supla::Mutex *mutex_ = nullptr;
};

};  // namespace Io
};  // namespace Supla
