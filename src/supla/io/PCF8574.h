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
Dependency: https://github.com/RobTillaart/PCF8574
Use library manager to install it
*/

#include <PCF8574.h>

#include <supla/io.h>
#include <supla/mutex.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Io {

class PCF8574 : public Supla::Io::Base {
 public:
  explicit PCF8574(uint8_t address = 0x20,
                   Supla::Mutex *mutex = nullptr,
                   uint8_t initialPinState = 0xFF,
                   TwoWire *wire = &Wire)
      : Supla::Io::Base(false), pcf_(address, wire), mutex_(mutex) {
    if (!pcf_.begin(initialPinState)) {
      SUPLA_LOG_ERROR("Unable to find PCF8574 at address 0x%x", address);
    } else {
      SUPLA_LOG_DEBUG("PCF8574 is connected at address: 0x%x", address);
    }
  }

  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override {
    if (mutex_) mutex_->lock();
    if (mode == INPUT_PULLUP && pcf_.isConnected()) {
      pcf_.write(pin, HIGH);
    }
    if (mutex_) mutex_->unlock();
  }

  void customDigitalWrite(int channelNumber, uint8_t pin,
                                                        uint8_t val) override {
    if (mutex_) mutex_->lock();
    if (pcf_.isConnected()) {
      pcf_.write(pin, val);
    }
    if (mutex_) mutex_->unlock();
  }

  int customDigitalRead(int channelNumber, uint8_t pin) override {
    uint8_t val;
    if (mutex_) mutex_->lock();
    val =  pcf_.isConnected() ? pcf_.read(pin) : 0;
    if (mutex_) mutex_->unlock();
    return val;
  }

  unsigned int customPulseIn(int channelNumber, uint8_t pin, uint8_t value,
                                              uint64_t timeoutMicro) override {
    return 0;
  }

  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override {}

  int customAnalogRead(int channelNumber, uint8_t pin) override {
    return 0;
  }

 protected:
  ::PCF8574 pcf_;
  Supla::Mutex *mutex_ = nullptr;
};

};  // namespace Io
};  // namespace Supla
