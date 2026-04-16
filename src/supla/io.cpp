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

namespace {
constexpr uint8_t DefaultAnalogWriteResolutionBits() {
#if defined(ARDUINO_ARCH_AVR)
  return 8;
#elif defined(ARDUINO_ARCH_ESP8266)
  return 10;
#elif defined(ARDUINO_ARCH_ESP32)
  return 8;
#else
  return 10;
#endif
}

constexpr uint16_t DefaultPwmFrequencyHz() {
#if defined(ARDUINO_ARCH_AVR)
  return 490;
#else
  return 1000;
#endif
}
}  // namespace

namespace Supla {
namespace Io {
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

void setPwmFrequency(uint8_t pin, uint16_t pwmFrequency, Io::Base *io) {
  if (io) {
    io->customSetPwmFrequency(pwmFrequency);
    return;
  }
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  analogWriteFrequency(pin, pwmFrequency);
#else
  (void)(pin);
  (void)(pwmFrequency);
#endif
}

void setPwmResolutionBits(uint8_t pin, uint8_t resolutionBits, Io::Base *io) {
  if (io) {
    io->customSetPwmResolutionBits(pin, resolutionBits);
    return;
  }
#ifdef ARDUINO_ARCH_ESP32
  analogWriteResolution(pin, resolutionBits);
#elif defined(ARDUINO_ARCH_ESP8266)
  analogWriteRange(1UL << resolutionBits);
#elif defined(ARDUINO_ARCH_AVR)
  (void)(pin);
  (void)(resolutionBits);
#else
  (void)(pin);
  (void)(resolutionBits);
#endif
}

uint8_t pwmResolutionBits(uint8_t pin, Io::Base *io) {
  if (io != nullptr) {
    return io->customPwmResolutionBits(pin);
  }
  return DefaultAnalogWriteResolutionBits();
}

uint32_t pwmMaxValue(uint8_t pin, Io::Base *io) {
  uint8_t bits = pwmResolutionBits(pin, io);
  if (bits == 0) {
    return 0;
  }
  return (1UL << bits) - 1;
}

uint8_t pwmResolutionBits(Io::Base *io) {
  return pwmResolutionBits(0, io);
}

uint32_t pwmMaxValue(Io::Base *io) {
  return pwmMaxValue(0, io);
}

uint16_t pwmFrequency(Io::Base *io) {
  if (io != nullptr) {
    return io->customPwmFrequency();
  }
  return DefaultPwmFrequencyHz();
}

Base::Base(bool useAsSingleton)
    : useAsSingleton(useAsSingleton),
      pwmResolutionBitsValue(DefaultAnalogWriteResolutionBits()),
      pwmFrequencyHzValue(DefaultPwmFrequencyHz()) {
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

void Base::customSetPwmResolutionBits(uint8_t pin, uint8_t resolutionBits) {
  (void)(pin);
  pwmResolutionBitsValue = resolutionBits;
}

void Base::customConfigureAnalogOutput(int, uint8_t, bool) {
}

void Base::customSetPwmFrequency(uint16_t freq) {
  pwmFrequencyHzValue = freq;
}

uint8_t Base::customPwmResolutionBits(uint8_t pin) const {
  (void)(pin);
  return pwmResolutionBitsValue;
}

uint32_t Base::customPwmMaxValue(uint8_t pin) const {
  uint8_t bits = customPwmResolutionBits(pin);
  if (bits == 0) {
    return 0;
  }
  return (1UL << bits) - 1;
}

uint16_t Base::customPwmFrequency() const {
  return pwmFrequencyHzValue;
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
