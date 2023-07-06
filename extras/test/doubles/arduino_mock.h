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

#ifndef EXTRAS_TEST_DOUBLES_ARDUINO_MOCK_H_
#define EXTRAS_TEST_DOUBLES_ARDUINO_MOCK_H_

#include "Arduino.h"
#include <gmock/gmock.h>

class DigitalInterface {
  public:
    DigitalInterface();
    virtual ~DigitalInterface();
    virtual void digitalWrite(uint8_t, uint8_t) = 0;
    virtual int digitalRead(uint8_t) = 0;
    virtual void analogWrite(uint8_t pin, int val) = 0;
    virtual void pinMode(uint8_t, uint8_t) = 0;

    static DigitalInterface *instance;
};

class TimeInterface {
  public:
    TimeInterface();
    virtual ~TimeInterface();
    virtual uint32_t millis() = 0;

    static TimeInterface *instance;
};

class DigitalInterfaceMock : public DigitalInterface {
 public:
  DigitalInterfaceMock();
  virtual ~DigitalInterfaceMock();
  MOCK_METHOD(void, digitalWrite, (uint8_t, uint8_t), (override));
  MOCK_METHOD(void, analogWrite, (uint8_t, int), (override));
  MOCK_METHOD(int, digitalRead, (uint8_t), (override));
  MOCK_METHOD(void, pinMode, (uint8_t, uint8_t), (override));
};

class TimeInterfaceMock : public TimeInterface {
  public:
    MOCK_METHOD(uint32_t, millis, (), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_ARDUINO_MOCK_H_
