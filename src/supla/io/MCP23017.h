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
 Dependency: https://github.com/RobTillaart/MCP23017_RT
 Use library manager to install it
*/

#include <MCP23017.h>

#include <supla/io.h>
#include <supla/mutex.h>
#include <supla/element.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Io {

class MCP23017 : public Supla::Io::Base, Supla::Element {
 public:
  explicit MCP23017(uint8_t address = 0x20,
                    Supla::Mutex *mutex = nullptr,
                    TwoWire *wire = &Wire,
                    bool pullUp = false)
      : Supla::Io::Base(false), mcp_(address, wire), mutex_(mutex) {
    if (!mcp_.begin(pullUp)) {
      SUPLA_LOG_ERROR("Unable to find MCP23017 at address: 0x%x", address);
    } else {
      SUPLA_LOG_DEBUG("MCP23017 is connected at address: 0x%x", address);
    }
  }

  void onInit() {
    read16FromMCP();
    outState_ = inState_;
    lastOutState_ = inState_;
  }

  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override {
    if (mutex_) mutex_->lock();
    if (mcp_.isConnected()) {
      mcp_.pinMode1(pin, mode);
    }
    if (mutex_) mutex_->unlock();
  }

  void customDigitalWrite(int channelNumber, uint8_t pin,
                                                         uint8_t val) override {
    if (pin >= 16) {
      SUPLA_LOG_WARNING("[MCP23017] can't write, pin %d out of range", pin);
      return;
    }
    if (val) {
      outState_ |= (1 << pin);
    } else {
      outState_ &= ~(1 << pin);
    }
  }

  int customDigitalRead(int channelNumber, uint8_t pin) override {
    if (pin >= 16) {
      SUPLA_LOG_WARNING("[MCP23017] can't read, pin %d out of range", pin);
      return 0;
    }
    return (inState_ >> pin) & 0x01;
  }

  unsigned int customPulseIn(int channelNumber, uint8_t pin, uint8_t value,
                                               uint64_t timeoutMicro) override {
    return 0;
  }

  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override {}

  int customAnalogRead(int channelNumber, uint8_t pin) override {
    return 0;
  }

  void onTimer() override {
    read16FromMCP();
    write16ToMCP();
  }

  void read16FromMCP() {
    if (mutex_) mutex_->lock();
    if (mcp_.isConnected()) {
      uint16_t data = mcp_.read16();
      inState_ = (data >> 8) | (data << 8);
    }
    if (mutex_) mutex_->unlock();
  }

  void write16ToMCP() {
    if (mutex_) mutex_->lock();
    if (mcp_.isConnected() && outState_ != lastOutState_) {
      uint16_t data = (outState_ >> 8) | (outState_ << 8);
      mcp_.write16(data);
      lastOutState_ = outState_;
    }
    if (mutex_) mutex_->unlock();
  }

 protected:
  ::MCP23017 mcp_;
  uint16_t outState_ = 0;
  uint16_t inState_ = 0;
  uint16_t lastOutState_ = 0;
  Supla::Mutex *mutex_ = nullptr;
};

};  // namespace Io
};  // namespace Supla
