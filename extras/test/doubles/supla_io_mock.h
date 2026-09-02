// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_SUPLA_IO_MOCK_H_
#define EXTRAS_TEST_DOUBLES_SUPLA_IO_MOCK_H_

#include <gmock/gmock.h>
#include <supla/io.h>

class SuplaIoMock : public Supla::Io::Base {
 public:
  SuplaIoMock() : Supla::Io::Base() {
    EXPECT_CALL(*this, customPwmResolutionBits(testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(10));
    EXPECT_CALL(*this, customPwmMaxValue(testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(1023));
  }

  MOCK_METHOD(void,
              customPinMode,
              (int channelNumber, uint8_t pin, uint8_t mode));
  MOCK_METHOD(int, customDigitalRead, (int channelNumber, uint8_t pin));
  MOCK_METHOD(void,
              customDigitalWrite,
              (int channelNumber, uint8_t pin, uint8_t val));
  MOCK_METHOD(
      unsigned int,
      customPulseIn,
      (int channelNumber, uint8_t pin, uint8_t value, uint64_t timeoutMicro));
  MOCK_METHOD(void,
              customAnalogWrite,
              (int channelNumber, uint8_t pin, int val));
  MOCK_METHOD(void,
              customSetPwmResolutionBits,
              (uint8_t pin, uint8_t resolutionBits));
  MOCK_METHOD(void,
              customConfigureAnalogOutput,
              (int channelNumber, uint8_t pin, bool outputInvert));
  MOCK_METHOD(void, customSetPwmFrequency, (uint16_t pwmFrequency));
  MOCK_METHOD(int, customAnalogRead, (int channelNumber, uint8_t pin));
  MOCK_METHOD(uint8_t,
              customPwmResolutionBits,
              (uint8_t pin),
              (const, override));
  MOCK_METHOD(uint32_t, customPwmMaxValue, (uint8_t pin), (const, override));
  MOCK_METHOD(void,
              customAttachInterrupt,
              (uint8_t pin, void (*func)(void), int mode));
  MOCK_METHOD(void, customDetachInterrupt, (uint8_t pin));
  MOCK_METHOD(uint8_t, customPinToInterrupt, (uint8_t pin));
};

#endif  // EXTRAS_TEST_DOUBLES_SUPLA_IO_MOCK_H_
