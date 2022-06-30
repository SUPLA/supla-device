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
// methods implemented in extras/porting/esp-idf/gpio.cpp
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

unsigned int pulseIn(uint8_t pin, uint8_t val, uint64_t timeoutMicro) {
  (void)(pin);
  (void)(val);
  (void)(timeoutMicro);
  return 0;
}
#endif

namespace Supla {
void Io::pinMode(uint8_t pin, uint8_t mode) {
  return pinMode(-1, pin, mode);
}

int Io::digitalRead(uint8_t pin) {
  return digitalRead(-1, pin);
}

void Io::digitalWrite(uint8_t pin, uint8_t val) {
  digitalWrite(-1, pin, val);
}

void Io::pinMode(int channelNumber, uint8_t pin, uint8_t mode) {
  if (ioInstance) {
    ioInstance->customPinMode(channelNumber, pin, mode);
  } else {
    ::pinMode(pin, mode);
  }
}

int Io::digitalRead(int channelNumber, uint8_t pin) {
  if (ioInstance) {
    return ioInstance->customDigitalRead(channelNumber, pin);
  }
  return ::digitalRead(pin);
}

void Io::digitalWrite(int channelNumber, uint8_t pin, uint8_t val) {
  if (channelNumber >= 0) {
    SUPLA_LOG_VERBOSE(" **** Digital write[%d], gpio: %d; value %d",
        channelNumber, pin, val);
  }

  if (ioInstance) {
    ioInstance->customDigitalWrite(channelNumber, pin, val);
    return;
  }
  ::digitalWrite(pin, val);
}

Io *Io::ioInstance = 0;

Io::Io() {
  ioInstance = this;
}

Io::~Io() {
  ioInstance = nullptr;
}

int Io::customDigitalRead(int channelNumber, uint8_t pin) {
  (void)(channelNumber);
  return ::digitalRead(pin);
}

void Io::customDigitalWrite(int channelNumber, uint8_t pin, uint8_t val) {
  (void)(channelNumber);
  ::digitalWrite(pin, val);
}

void Io::customPinMode(int channelNumber, uint8_t pin, uint8_t mode) {
  (void)(channelNumber);
  ::pinMode(pin, mode);
}

void Io::analogWrite(int channelNumber, uint8_t pin, int val) {
  SUPLA_LOG_VERBOSE(" **** Analog write[%d], gpio: %d; value %d",
      channelNumber, pin, val);

  if (ioInstance) {
    ioInstance->customAnalogWrite(channelNumber, pin, val);
    return;
  }
  ::analogWrite(pin, val);
}

void Io::analogWrite(uint8_t pin, int val) {
  analogWrite(-1, pin, val);
}

void Io::customAnalogWrite(int channelNumber, uint8_t pin, int val) {
  (void)(channelNumber);
  ::analogWrite(pin, val);
}

unsigned int Io::pulseIn(uint8_t pin, uint8_t value, uint64_t timeoutMicro) {
  return pulseIn(-1, pin, value, timeoutMicro);
}

unsigned int Io::pulseIn(int channelNumber, uint8_t pin, uint8_t value,
      uint64_t timeoutMicro) {
  (void)(channelNumber);
  return ::pulseIn(pin, value, timeoutMicro);
}

unsigned int Io::customPulseIn(int channelNumber, uint8_t pin, uint8_t value,
      uint64_t timeoutMicro) {
  (void)(channelNumber);
  return ::pulseIn(pin, value, timeoutMicro);
}

};  // namespace Supla
