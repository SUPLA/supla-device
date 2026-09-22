// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "io_group.h"

namespace Supla {
namespace Io {

IoGroup::IoGroup(const IoPin *pins, uint8_t count)
    : pins(pins), count(pins ? count : 0) {
}

IoPin IoGroup::getPin() {
  return IoPin(count ? 0 : -1, this);
}

bool IoGroup::isReady() const {
  if (!count) {
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) {
    if (!pins[i].isSet() || (pins[i].io && !pins[i].io->isReady())) {
      return false;
    }
  }
  return true;
}

void IoGroup::customPinMode(int channelNumber, uint8_t, uint8_t mode) {
  for (uint8_t i = 0; i < count; ++i) {
    if (pins[i].isSet()) {
      Io::pinMode(channelNumber, pins[i].pin, mode, pins[i].io);
    }
  }
}

void IoGroup::customDigitalWrite(int channelNumber, uint8_t, uint8_t val) {
  for (uint8_t i = 0; i < count; ++i) {
    if (val) {
      pins[i].writeActive(channelNumber);
    } else {
      pins[i].writeInactive(channelNumber);
    }
  }
}

void IoGroup::customAnalogWrite(int channelNumber, uint8_t, int val) {
  for (uint8_t i = 0; i < count; ++i) {
    pins[i].analogWrite(val, channelNumber);
  }
}

void IoGroup::customConfigureAnalogOutput(int channelNumber,
                                         uint8_t,
                                         bool outputInvert) {
  for (uint8_t i = 0; i < count; ++i) {
    IoPin member = pins[i];
    member.setActiveHigh(member.isActiveHigh() != outputInvert);
    member.configureAnalogOutput(channelNumber);
  }
}

void IoGroup::customSetPwmResolutionBits(uint8_t, uint8_t bits) {
  for (uint8_t i = 0; i < count; ++i) {
    if (pins[i].isSet()) {
      Io::setPwmResolutionBits(pins[i].pin, bits, pins[i].io);
    }
  }
}

void IoGroup::customSetPwmFrequency(uint16_t frequency) {
  for (uint8_t i = 0; i < count; ++i) {
    if (pins[i].isSet()) {
      Io::setPwmFrequency(pins[i].pin, frequency, pins[i].io);
    }
  }
}

int IoGroup::customDigitalRead(int channelNumber, uint8_t) {
  return count && pins[0].isSet() ? pins[0].readActive(channelNumber) : 0;
}

int IoGroup::customAnalogRead(int channelNumber, uint8_t) {
  return count && pins[0].isSet()
             ? Io::analogRead(channelNumber, pins[0].pin, pins[0].io)
             : 0;
}

unsigned int IoGroup::customPulseIn(int channelNumber,
                                    uint8_t,
                                    uint8_t value,
                                    uint64_t timeoutMicro) {
  return count && pins[0].isSet()
             ? Io::pulseIn(channelNumber, pins[0].pin, value, timeoutMicro,
                           pins[0].io)
             : 0;
}

uint8_t IoGroup::customDefaultPwmResolutionBits(uint8_t) const {
  return count && pins[0].isSet()
             ? Io::defaultPwmResolutionBits(pins[0].pin, pins[0].io)
             : 0;
}

bool IoGroup::customCanSetPwmResolutionBits(uint8_t) const {
  return count && pins[0].isSet() &&
         Io::canSetPwmResolutionBits(pins[0].pin, pins[0].io);
}

uint8_t IoGroup::customPwmResolutionBits(uint8_t) const {
  return count && pins[0].isSet() ? pins[0].pwmResolutionBits() : 0;
}

uint32_t IoGroup::customPwmMaxValue(uint8_t) const {
  return count && pins[0].isSet() ? pins[0].pwmMaxValue() : 0;
}

uint16_t IoGroup::customPwmFrequency() const {
  return count && pins[0].isSet() ? Io::pwmFrequency(pins[0].io) : 0;
}

void IoGroup::customAttachInterrupt(uint8_t interrupt,
                                    void (*func)(void),
                                    int mode) {
  if (count && pins[0].isSet()) {
    Io::attachInterrupt(interrupt, func, mode, pins[0].io);
  }
}

void IoGroup::customDetachInterrupt(uint8_t interrupt) {
  if (count && pins[0].isSet()) {
    Io::detachInterrupt(interrupt, pins[0].io);
  }
}

uint8_t IoGroup::customPinToInterrupt(uint8_t) {
  return count && pins[0].isSet()
             ? Io::pinToInterrupt(pins[0].pin, pins[0].io)
             : 0;
}

}  // namespace Io
}  // namespace Supla
