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

#pragma once

/*
Dependency: https://github.com/RobTillaart/PCA9685_RT
Use library manager to install it
*/

#include <PCA9685.h>
#include <supla/io.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Io {

class PCA9685 : public Supla::Io::Base {
 public:
  explicit PCA9685(uint8_t address = 0x40, TwoWire *wire = &Wire)
      : Supla::Io::Base(false), pca_(address, wire) {
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
  }
  int customDigitalRead(int channelNumber, uint8_t pin) override {
    return 0;
  }
  unsigned int customPulseIn(int channelNumber, uint8_t pin, uint8_t value,
                                            uint64_t timeoutMicro) override {
    return 0;
  }
  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override {
    if (pca_.isConnected()) {
      val = map(val, 0, 1023, 0, 4095);
      pca_.setPWM(pin, val);
    }
  }

  int customAnalogRead(int channelNumber, uint8_t pin) override {
    return 0;
  }

  // Default frequency: 200 Hz
  void setPWMFrequency(uint16_t frequency) {
    if (pca_.isConnected()) {
      pca_.setFrequency(frequency);
      SUPLA_LOG_DEBUG("[PCA9685] set PWM frequency: %d Hz",
                                                          pca_.getFrequency());
    }
  }

 protected:
  ::PCA9685 pca_;
};

};  // namespace Io
};  // namespace Supla
