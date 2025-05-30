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
void Io::pinMode(uint8_t pin, uint8_t mode, Supla::Io *io) {
  return pinMode(-1, pin, mode, io);
}

int Io::digitalRead(uint8_t pin, Supla::Io *io) {
  return digitalRead(-1, pin, io);
}

void Io::digitalWrite(uint8_t pin, uint8_t val, Supla::Io *io) {
  digitalWrite(-1, pin, val, io);
}

void Io::analogWrite(uint8_t pin, int val, Supla::Io *io) {
  analogWrite(-1, pin, val, io);
}

int Io::analogRead(uint8_t pin, Supla::Io *io) {
  return analogRead(-1, pin, io);
}

unsigned int Io::pulseIn(uint8_t pin,
                         uint8_t value,
                         uint64_t timeoutMicro,
                         Supla::Io *io) {
  return pulseIn(-1, pin, value, timeoutMicro, io);
}

void Io::pinMode(int channelNumber, uint8_t pin, uint8_t mode, Supla::Io *io) {
  if (io) {
    io->customPinMode(channelNumber, pin, mode);
    return;
  }
  ::pinMode(pin, mode);
}

int Io::digitalRead(int channelNumber, uint8_t pin, Supla::Io *io) {
  if (io) {
    return io->customDigitalRead(channelNumber, pin);
  }
  return ::digitalRead(pin);
}

void Io::digitalWrite(int channelNumber,
                      uint8_t pin,
                      uint8_t val,
                      Supla::Io *io) {
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

void Io::analogWrite(int channelNumber, uint8_t pin, int val, Supla::Io *io) {
  SUPLA_LOG_VERBOSE(
      " **** Analog write[%d], gpio: %d; value %d", channelNumber, pin, val);
  if (io) {
    io->customAnalogWrite(channelNumber, pin, val);
    return;
  }
  ::analogWrite(pin, val);
}

int Io::analogRead(int channelNumber, uint8_t pin, Supla::Io *io) {
  if (io) {
    return io->customAnalogRead(channelNumber, pin);
  }
  return ::analogRead(pin);
}

unsigned int Io::pulseIn(int channelNumber,
                         uint8_t pin,
                         uint8_t value,
                         uint64_t timeoutMicro,
                         Supla::Io *io) {
  (void)(channelNumber);
  if (io) {
    return io->customPulseIn(channelNumber, pin, value, timeoutMicro);
  }
  return ::pulseIn(pin, value, timeoutMicro);
}

void Io::attachInterrupt(uint8_t pin,
                         void (*func)(void),
                         int mode,
                         Supla::Io *io) {
  if (io) {
    io->customAttachInterrupt(pin, func, mode);
    return;
  }
  ::attachInterrupt(pin, func, mode);
}

void Io::detachInterrupt(uint8_t pin, Io *io) {
  if (io) {
    io->customDetachInterrupt(pin);
    return;
  }
  ::detachInterrupt(pin);
}


uint8_t Io::pinToInterrupt(uint8_t pin, Io *io) {
  if (io) {
    return io->customPinToInterrupt(pin);
  }
  return digitalPinToInterrupt(pin);
}

Io::Io(bool useAsSingleton) : useAsSingleton(useAsSingleton) {
  if (useAsSingleton) {
    if (ioInstance != nullptr) {
      delete ioInstance;
    }
    ioInstance = this;
  }
}

Io::~Io() {
  if (useAsSingleton) {
    ioInstance = nullptr;
  }
}

void Io::customPinMode(int , uint8_t, uint8_t) {
}

int Io::customDigitalRead(int, uint8_t) {
  return 0;
}

void Io::customDigitalWrite(int, uint8_t, uint8_t) {
}

void Io::customAnalogWrite(int, uint8_t, int) {
}

int Io::customAnalogRead(int, uint8_t) {
  return 0;
}

unsigned int Io::customPulseIn(int, uint8_t, uint8_t, uint64_t) {
  return 0;
}

void Io::customAttachInterrupt(uint8_t, void (*)(void), int) {
}

void Io::customDetachInterrupt(uint8_t) {
}

uint8_t Io::customPinToInterrupt(uint8_t) {
  return 0;
}

Io *Io::ioInstance = nullptr;

};  // namespace Supla
