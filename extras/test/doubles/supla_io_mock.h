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

#ifndef EXTRAS_TEST_DOUBLES_SUPLA_IO_MOCK_H_
#define EXTRAS_TEST_DOUBLES_SUPLA_IO_MOCK_H_

#include <gmock/gmock.h>
#include <supla/io.h>

class SuplaIoMock : public Supla::Io::Base {
 public:
  explicit SuplaIoMock(bool useAsSingleton = false)
      : Supla::Io::Base(useAsSingleton) {
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
