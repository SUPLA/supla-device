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

#ifndef SRC_SUPLA_IO_H_
#define SRC_SUPLA_IO_H_

#include <stdint.h>

#include "definitions.h"

namespace Supla {
// This class can be used to override digitalRead and digitalWrite methods.
// If you want to add custom behavior i.e. during read/write from some
// digital pin, you can inherit from Supla::Io class, implement your
// own customDigitalRead and customDigitalWrite methods and create instance
// of this class. It will automatically register and SuplaDevice will use it.
//
// Example use: implement some additional logic, when relay state is
// changed.
class Io {
 public:
  static void pinMode(uint8_t pin, uint8_t mode, Supla::Io *io = ioInstance);
  static int digitalRead(uint8_t pin, Supla::Io *io = ioInstance);
  static void digitalWrite(uint8_t pin,
                           uint8_t val,
                           Supla::Io *io = ioInstance);
  static void analogWrite(uint8_t pin, int value, Supla::Io *io = ioInstance);
  static int analogRead(uint8_t pin, Supla::Io *io = ioInstance);
  static unsigned int pulseIn(uint8_t pin,
                              uint8_t value,
                              uint64_t timeoutMicro,
                              Supla::Io *io = ioInstance);

  static void pinMode(int channelNumber,
                      uint8_t pin,
                      uint8_t mode,
                      Supla::Io *io = ioInstance);
  static int digitalRead(int channelNumber,
                         uint8_t pin,
                         Supla::Io *io = ioInstance);
  static void digitalWrite(int channelNumber,
                           uint8_t pin,
                           uint8_t val,
                           Supla::Io *io = ioInstance);
  static void analogWrite(int channelNumber,
                          uint8_t pin,
                          int value,
                          Supla::Io *io = ioInstance);
  static int analogRead(int channelNumber,
                        uint8_t pin,
                        Supla::Io *io = ioInstance);
  static unsigned int pulseIn(int channelNumber,
                              uint8_t pin,
                              uint8_t value,
                              uint64_t timeoutMicro,
                              Supla::Io *io = ioInstance);

  static Io *ioInstance;

  explicit Io(bool useAsSingleton = true);
  virtual ~Io();
  virtual void customPinMode(int channelNumber, uint8_t pin, uint8_t mode);
  virtual int customDigitalRead(int channelNumber, uint8_t pin);
  virtual unsigned int customPulseIn(int channelNumber,
      uint8_t pin,
      uint8_t value,
      uint64_t timeoutMicro);
  virtual void customDigitalWrite(int channelNumber, uint8_t pin, uint8_t val);
  virtual void customAnalogWrite(int channelNumber, uint8_t pin, int val);
  virtual int customAnalogRead(int channelNumber, uint8_t pin);

 private:
  bool useAsSingleton = true;
};
};  // namespace Supla

#endif  // SRC_SUPLA_IO_H_
