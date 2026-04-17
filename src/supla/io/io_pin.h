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

#ifndef SRC_SUPLA_IO_IO_PIN_H_
#define SRC_SUPLA_IO_IO_PIN_H_

#include <stdint.h>

namespace Supla {
namespace Io {

class Base;

struct IoPin {
  enum Flag : uint8_t {
    IsSet = 1 << 0,
    PullUp = 1 << 1,
    ActiveHigh = 1 << 2,
  };

  uint8_t pin = 0;
  uint8_t flags = ActiveHigh;
  uint8_t mode = 0;
  Base *io = nullptr;

  IoPin() = default;
  explicit IoPin(int pin, Base *io = nullptr) : io(io) {
    setPin(pin);
  }

  bool isSet() const {
    return (flags & IsSet) != 0;
  }
  void setIsSet(bool value) {
    if (value) {
      flags |= IsSet;
    } else {
      flags &= ~IsSet;
    }
  }

  int getPin() const {
    return isSet() ? static_cast<int>(pin) : -1;
  }
  void setPin(int value) {
    if (value < 0) {
      pin = 0;
      setIsSet(false);
    } else {
      pin = static_cast<uint8_t>(value);
      setIsSet(true);
    }
  }

  bool isPullUp() const {
    return (flags & PullUp) != 0;
  }
  void setPullUp(bool value) {
    if (value) {
      flags |= PullUp;
    } else {
      flags &= ~PullUp;
    }
  }

  bool isActiveHigh() const {
    return (flags & ActiveHigh) != 0;
  }
  void setActiveHigh(bool value) {
    if (value) {
      flags |= ActiveHigh;
    } else {
      flags &= ~ActiveHigh;
    }
  }

  void setMode(uint8_t value) {
    mode = value;
  }
  uint8_t getMode() const {
    return mode;
  }

  bool operator==(const IoPin &other) const {
    return io == other.io && getPin() == other.getPin();
  }
  bool operator!=(const IoPin &other) const {
    return !(*this == other);
  }

  void setPwmResolutionBits(uint8_t resolutionBits);
  void setPwmFrequency(uint32_t frequencyHz);
  void configureAnalogOutput(int channelNumber = -1) const;
  void pinMode(int channelNumber = -1) const;
  int digitalRead(int channelNumber = -1) const;
  void digitalWrite(uint8_t value, int channelNumber = -1) const;
  void analogWrite(int value, int channelNumber = -1) const;
  uint8_t pwmResolutionBits() const;
  uint32_t pwmMaxValue() const;
  void writeActive(int channelNumber = -1) const;
  void writeInactive(int channelNumber = -1) const;
  bool readActive(int channelNumber = -1) const;
};

}  // namespace Io
}  // namespace Supla

#endif  // SRC_SUPLA_IO_IO_PIN_H_
