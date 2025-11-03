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

#include <supla/element.h>
#include <supla/io.h>
#include <supla/log_wrapper.h>
#include <vector>

namespace Supla {
namespace Io {

class SR74HC595 : public Supla::Io::Base, Supla::Element {
 public:
  explicit SR74HC595(const uint8_t size,
                     const uint8_t serialDataPin,
                     const uint8_t clockPin,
                     const uint8_t latchPin)
      : Supla::Io::Base(false),
        _size(size),
        _clockPin(clockPin),
        _serialDataPin(serialDataPin),
        _latchPin(latchPin) {
    pinMode(_clockPin, OUTPUT);
    pinMode(_serialDataPin, OUTPUT);
    pinMode(_latchPin, OUTPUT);
    digitalWrite(_clockPin, LOW);
    digitalWrite(_serialDataPin, LOW);
    digitalWrite(_latchPin, LOW);
    _digitalValues.resize(_size, 0);
  }
  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override {}

  void customDigitalWrite(int channelNumber,
                          uint8_t pin,
                          uint8_t val) override {
    (val) ? bitSet(_digitalValues[pin / 8], pin % 8)
          : bitClear(_digitalValues[pin / 8], pin % 8);
    updateRegisters();
  }

  int customDigitalRead(int channelNumber, uint8_t pin) override {
    return (_digitalValues[pin / 8] >> (pin % 8)) & 1;
  }

  unsigned int customPulseIn(int channelNumber,
                             uint8_t pin,
                             uint8_t value,
                             uint64_t timeoutMicro) override {
    return 0;
  }

  void customAnalogWrite(int channelNumber, uint8_t pin, int val) override {}

  int customAnalogRead(int channelNumber, uint8_t pin) override {
    return 0;
  }

  void updateRegisters() {
    for (int i = _size - 1; i >= 0; i--) {
      shiftOut(_serialDataPin, _clockPin, MSBFIRST, _digitalValues[i]);
    }
    digitalWrite(_latchPin, HIGH);
    digitalWrite(_latchPin, LOW);
  }

 protected:
  uint8_t _size;
  uint8_t _clockPin;
  uint8_t _serialDataPin;
  uint8_t _latchPin;
  std::vector<uint8_t> _digitalValues;
};

};  // namespace Io
};  // namespace Supla
