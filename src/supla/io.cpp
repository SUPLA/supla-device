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

#include "io.h"

#include <supla/log_wrapper.h>

#ifdef ARDUINO
#include <Arduino.h>
#elif defined(ESP_PLATFORM)
#include <esp_idf_gpio.h>
// methods implemented in extras/porting/esp-idf/esp_idf_gpio.cpp
#else
// TODO(klew): implement those methods or extract them to separate interface
void pinMode(uint8_t pin, uint8_t mode) {
  (void)(pin);
  (void)(mode);
}

int digitalRead(uint8_t pin) {
  (void)(pin);
  return 0;
}
void digitalWrite(uint8_t pin, uint8_t val) {
  (void)(pin);
  (void)(val);
}

void analogWrite(uint8_t pin, int val) {
  (void)(pin);
  (void)(val);
}

int analogRead(uint8_t pin) {
  (void)(pin);
  return 0;
}

unsigned int pulseIn(uint8_t pin, uint8_t val, uint64_t timeoutMicro) {
  (void)(pin);
  (void)(val);
  (void)(timeoutMicro);
  return 0;
}

void attachInterrupt(uint8_t pin, void (*func)(void), int mode) {
  (void)(func);
  SUPLA_LOG_ERROR(
      " *** NOT IMPLEMENTED *** GPIO %d attach interrupt %d", pin, mode);
}

void detachInterrupt(uint8_t pin) {
  SUPLA_LOG_ERROR(" *** NOT IMPLEMENTED *** GPIO %d detach interrupt", pin);
}

uint8_t digitalPinToInterrupt(uint8_t pin) {
  return pin;
}
#endif

namespace Supla {
namespace Io {
void IoPin::configureAnalogOutput(int channelNumber) const {
  if (!isSet()) {
    return;
  }

  if (io != nullptr) {
    io->customConfigureAnalogOutput(channelNumber,
                                    static_cast<uint8_t>(pin),
                                    !isActiveHigh());
    return;
  }

  if (!isActiveHigh()) {
    SUPLA_LOG_WARNING(
        "Analog output invert is not supported for default IO, GPIO %d",
        pin);
  }

#ifdef ARDUINO_ARCH_ESP32
#elif defined(ARDUINO_ARCH_ESP8266)
#endif
}

void IoPin::setAnalogOutputResolutionBits(uint8_t resolutionBits) {
  if (io != nullptr) {
    io->customSetPwmResolutionBits(resolutionBits);
    return;
  }

  analogWriteResolutionBitsValue = resolutionBits;

#ifdef ARDUINO_ARCH_ESP32
  analogWriteResolution(static_cast<uint8_t>(pin), resolutionBits);
#elif defined(ARDUINO_ARCH_ESP8266)
  analogWriteRange(1UL << resolutionBits);
#else
  (void)(resolutionBits);
#endif
}

void IoPin::setAnalogOutputFrequency(uint32_t frequencyHz) {
  if (io != nullptr) {
    io->customSetPwmFrequency(static_cast<uint16_t>(frequencyHz));
    return;
  }

#ifdef ARDUINO_ARCH_ESP32
  analogWriteFrequency(static_cast<uint8_t>(pin), frequencyHz);
#elif defined(ARDUINO_ARCH_ESP8266)
  analogWriteFreq(frequencyHz);
#else
  (void)(frequencyHz);
#endif
}

void IoPin::pinMode(int channelNumber) const {
  if (isSet()) {
    uint8_t effectiveMode = mode;
    if (effectiveMode == INPUT && isPullUp()) {
      effectiveMode = INPUT_PULLUP;
    }
    Supla::Io::pinMode(channelNumber,
                       static_cast<uint8_t>(pin),
                       effectiveMode,
                       io);
  }
}

int IoPin::digitalRead(int channelNumber) const {
  if (!isSet()) {
    return 0;
  }
  return Supla::Io::digitalRead(channelNumber,
                                static_cast<uint8_t>(pin),
                                io);
}

void IoPin::digitalWrite(uint8_t value, int channelNumber) const {
  if (isSet()) {
    Supla::Io::digitalWrite(channelNumber,
                            static_cast<uint8_t>(pin),
                            value,
                            io);
  }
}

void IoPin::analogWrite(int value, int channelNumber) const {
  if (isSet()) {
#ifdef ARDUINO_ARCH_AVR
    if (io == nullptr) {
      value = map(value, 0, 1023, 0, 255);
    }
#endif
    Supla::Io::analogWrite(channelNumber,
                           static_cast<uint8_t>(pin),
                           value,
                           io);
  }
}

uint8_t IoPin::analogWriteResolutionBits() const {
  if (analogWriteResolutionBitsValue != 0) {
    return analogWriteResolutionBitsValue;
  }
  if (io != nullptr) {
    return io->customAnalogWriteResolutionBits();
  }
  return 0;
}

uint32_t IoPin::analogWriteMaxValue() const {
  uint8_t bits = analogWriteResolutionBits();
  if (bits == 0) {
    return 0;
  }
  return (1UL << bits) - 1;
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

void pinMode(uint8_t pin, uint8_t mode, Supla::Io::Base *io) {
  return pinMode(-1, pin, mode, io);
}

int digitalRead(uint8_t pin, Supla::Io::Base *io) {
  return digitalRead(-1, pin, io);
}

void digitalWrite(uint8_t pin, uint8_t val, Supla::Io::Base *io) {
  digitalWrite(-1, pin, val, io);
}

void analogWrite(uint8_t pin, int val, Supla::Io::Base *io) {
  analogWrite(-1, pin, val, io);
}

int analogRead(uint8_t pin, Supla::Io::Base *io) {
  return analogRead(-1, pin, io);
}

unsigned int pulseIn(uint8_t pin,
                         uint8_t value,
                         uint64_t timeoutMicro,
                         Supla::Io::Base *io) {
  return pulseIn(-1, pin, value, timeoutMicro, io);
}

void pinMode(int channelNumber,
             uint8_t pin,
             uint8_t mode,
             Supla::Io::Base *io) {
  if (io) {
    io->customPinMode(channelNumber, pin, mode);
    return;
  }
  ::pinMode(pin, mode);
}

int digitalRead(int channelNumber, uint8_t pin, Supla::Io::Base *io) {
  if (io) {
    return io->customDigitalRead(channelNumber, pin);
  }
  return ::digitalRead(pin);
}

void digitalWrite(int channelNumber,
                      uint8_t pin,
                      uint8_t val,
                      Supla::Io::Base *io) {
  if (channelNumber >= 0) {
    SUPLA_LOG_VERBOSE(
        " **** Digital write[%d], gpio: %d; value %d", channelNumber, pin, val);
  }

  if (io) {
    io->customDigitalWrite(channelNumber, pin, val);
    return;
  }
  ::digitalWrite(pin, val);
}

void analogWrite(int channelNumber, uint8_t pin, int val, Supla::Io::Base *io) {
  SUPLA_LOG_VERBOSE(
      " **** Analog write[%d], gpio: %d; value %d", channelNumber, pin, val);
  if (io) {
    io->customAnalogWrite(channelNumber, pin, val);
    return;
  }
  ::analogWrite(pin, val);
}

int analogRead(int channelNumber, uint8_t pin, Supla::Io::Base *io) {
  if (io) {
    return io->customAnalogRead(channelNumber, pin);
  }
  return ::analogRead(pin);
}

unsigned int pulseIn(int channelNumber,
                         uint8_t pin,
                         uint8_t value,
                         uint64_t timeoutMicro,
                         Supla::Io::Base *io) {
  (void)(channelNumber);
  if (io) {
    return io->customPulseIn(channelNumber, pin, value, timeoutMicro);
  }
  return ::pulseIn(pin, value, timeoutMicro);
}

void attachInterrupt(uint8_t pin,
                         void (*func)(void),
                         int mode,
                         Supla::Io::Base *io) {
  if (io) {
    io->customAttachInterrupt(pin, func, mode);
    return;
  }
  ::attachInterrupt(pin, func, mode);
}

void detachInterrupt(uint8_t pin, Io::Base *io) {
  if (io) {
    io->customDetachInterrupt(pin);
    return;
  }
  ::detachInterrupt(pin);
}


uint8_t pinToInterrupt(uint8_t pin, Io::Base *io) {
  if (io) {
    return io->customPinToInterrupt(pin);
  }
  return digitalPinToInterrupt(pin);
}

Base::Base(bool useAsSingleton) : useAsSingleton(useAsSingleton) {
  if (useAsSingleton) {
    if (ioInstance != nullptr) {
      delete ioInstance;
    }
    ioInstance = this;
  }
}

Base::~Base() {
  if (useAsSingleton) {
    ioInstance = nullptr;
  }
}

bool Base::isReady() const {
  return true;
}

void Base::customPinMode(int , uint8_t, uint8_t) {
}

int Base::customDigitalRead(int, uint8_t) {
  return 0;
}

void Base::customDigitalWrite(int, uint8_t, uint8_t) {
}

void Base::customAnalogWrite(int, uint8_t, int) {
}

void Base::customSetPwmResolutionBits(uint8_t) {
}

void Base::customConfigureAnalogOutput(int, uint8_t, bool) {
}

void Base::customSetPwmFrequency(uint16_t) {
}

uint8_t Base::customAnalogWriteResolutionBits() const {
  return 0;
}

uint32_t Base::customAnalogWriteMaxValue() const {
  uint8_t bits = customAnalogWriteResolutionBits();
  if (bits == 0) {
    return 0;
  }
  return (1UL << bits) - 1;
}

int Base::customAnalogRead(int, uint8_t) {
  return 0;
}

unsigned int Base::customPulseIn(int, uint8_t, uint8_t, uint64_t) {
  return 0;
}

void Base::customAttachInterrupt(uint8_t, void (*)(void), int) {
}

void Base::customDetachInterrupt(uint8_t) {
}

uint8_t Base::customPinToInterrupt(uint8_t) {
  return 0;
}

Base *Base::ioInstance = nullptr;

}  // namespace Io
}  // namespace Supla
