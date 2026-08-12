// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ARDUINO_MOCK_H_
#define EXTRAS_TEST_DOUBLES_ARDUINO_MOCK_H_

#include <gmock/gmock.h>

#include "Arduino.h"

class DigitalInterface {
 public:
  DigitalInterface();
  virtual ~DigitalInterface();
  virtual void digitalWrite(uint8_t, uint8_t) = 0;
  virtual int digitalRead(uint8_t) = 0;
  virtual void analogWrite(uint8_t pin, int val) = 0;
  virtual void pinMode(uint8_t, uint8_t) = 0;
  virtual unsigned int pulseIn(uint8_t, uint8_t, uint64_t) = 0;
  virtual void analogWriteResolution(uint8_t, uint8_t) {
  }
  virtual void analogWriteFrequency(uint8_t, uint32_t) {
  }
  virtual void analogWriteFreq(uint32_t) {
  }
  virtual void analogWriteRange(uint32_t) {
  }

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
  MOCK_METHOD(unsigned int,
              pulseIn,
              (uint8_t, uint8_t, uint64_t),
              (override));
  MOCK_METHOD(void, analogWriteResolution, (uint8_t, uint8_t), (override));
  MOCK_METHOD(void, analogWriteFrequency, (uint8_t, uint32_t), (override));
  MOCK_METHOD(void, analogWriteFreq, (uint32_t), (override));
  MOCK_METHOD(void, analogWriteRange, (uint32_t), (override));
};

class TimeInterfaceMock : public TimeInterface {
 public:
  MOCK_METHOD(uint32_t, millis, (), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_ARDUINO_MOCK_H_
