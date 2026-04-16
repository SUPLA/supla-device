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

#include "io_pin.h"

#include <supla/log_wrapper.h>

#include "../io.h"

namespace Supla {
namespace Io {

void IoPin::configureAnalogOutput(int channelNumber) const {
  if (!isSet()) {
    return;
  }

  if (io != nullptr) {
    io->customConfigureAnalogOutput(
        channelNumber, static_cast<uint8_t>(pin), !isActiveHigh());
    return;
  }

  if (!isActiveHigh()) {
    SUPLA_LOG_WARNING(
        "Analog output invert is not supported for default IO, GPIO %d", pin);
  }
}

void IoPin::setPwmResolutionBits(uint8_t resolutionBits) {
  if (!isSet()) {
    return;
  }
  Supla::Io::setPwmResolutionBits(
      static_cast<uint8_t>(pin), resolutionBits, io);
}

void IoPin::setPwmFrequency(uint32_t frequencyHz) {
  if (!isSet()) {
    return;
  }
  Supla::Io::setPwmFrequency(
      static_cast<uint8_t>(pin), static_cast<uint16_t>(frequencyHz), io);
}

void IoPin::pinMode(int channelNumber) const {
  if (!isSet()) {
    return;
  }
  uint8_t effectiveMode = mode;
  if (effectiveMode == INPUT && isPullUp()) {
    effectiveMode = INPUT_PULLUP;
  }
  Supla::Io::pinMode(
      channelNumber, static_cast<uint8_t>(pin), effectiveMode, io);
}

int IoPin::digitalRead(int channelNumber) const {
  if (!isSet()) {
    return 0;
  }
  return Supla::Io::digitalRead(channelNumber, static_cast<uint8_t>(pin), io);
}

void IoPin::digitalWrite(uint8_t value, int channelNumber) const {
  if (isSet()) {
    Supla::Io::digitalWrite(
        channelNumber, static_cast<uint8_t>(pin), value, io);
  }
}

void IoPin::analogWrite(int value, int channelNumber) const {
  if (isSet()) {
    Supla::Io::analogWrite(channelNumber, static_cast<uint8_t>(pin), value, io);
  }
}

uint8_t IoPin::pwmResolutionBits() const {
  return Supla::Io::pwmResolutionBits(static_cast<uint8_t>(pin), io);
}

uint32_t IoPin::pwmMaxValue() const {
  return Supla::Io::pwmMaxValue(static_cast<uint8_t>(pin), io);
}

void IoPin::writeActive(int channelNumber) const {
  digitalWrite(isActiveHigh() ? 1 : 0, channelNumber);
}

void IoPin::writeInactive(int channelNumber) const {
  digitalWrite(isActiveHigh() ? 0 : 1, channelNumber);
}

bool IoPin::readActive(int channelNumber) const {
  return digitalRead(channelNumber) == (isActiveHigh() ? 1 : 0);
}

}  // namespace Io
}  // namespace Supla
