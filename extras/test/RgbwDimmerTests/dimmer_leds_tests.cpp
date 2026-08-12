// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <arduino_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/control/dimmer_leds.h>
#include <supla_io_mock.h>

#include "legacy_pwm_test_io.h"

using ::testing::Return;

class TimeInterfaceStub : public TimeInterface {
 public:
  uint32_t millis() override {
    static uint32_t value = 0;
    value += 1000;
    return value;
  }
};

class UnsetPinPwmIo : public Supla::Io::Base {
 public:
  MOCK_METHOD(void, customAnalogWrite, (int, uint8_t, int));
  MOCK_METHOD(uint8_t,
              customDefaultPwmResolutionBits,
              (uint8_t),
              (const, override));
  MOCK_METHOD(bool,
              customCanSetPwmResolutionBits,
              (uint8_t),
              (const, override));
  MOCK_METHOD(uint8_t,
              customPwmResolutionBits,
              (uint8_t),
              (const, override));
};

TEST(DimmerLedsTests, SettingNewDimValue) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  DigitalInterfaceMock ioMock;

  EXPECT_CALL(ioMock, analogWriteResolution(1, 10)).Times(1);
  EXPECT_CALL(ioMock, analogWriteFrequency(1, 1000)).Times(1);
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  EXPECT_CALL(ioMock, analogWrite(1, 0)).Times(1);

  EXPECT_CALL(ioMock, analogWrite(1, 101)).Times(1);

  Supla::Control::DimmerLeds dim(1);

  auto ch = dim.getChannel();
  // disable fading effect so we'll get instant setting value on device call
  dim.setFadeEffectTime(0);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  dim.onInit();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  dim.iterateAlways();
  time.advance(1000);
  dim.onFastTimer();
  time.advance(1);
  dim.onFastTimer();
  time.advance(1);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  dim.setRGBW(1, 2, 3, 4, 10);

  dim.iterateAlways();
  time.advance(1000);
  dim.onFastTimer();
  time.advance(1000);
  dim.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 10);
}

TEST(DimmerLedsTests, IoPinConstructorUsesSeparateIoForBrightness) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  SuplaIoMock brightnessIo;

  EXPECT_CALL(brightnessIo, customSetPwmResolutionBits(7, 10));
  EXPECT_CALL(brightnessIo, customSetPwmFrequency(1000));
  EXPECT_CALL(brightnessIo, customConfigureAnalogOutput(-1, 7, false));
  EXPECT_CALL(brightnessIo, customPinMode(-1, 7, OUTPUT));
  EXPECT_CALL(brightnessIo, customAnalogWrite(-1, 7, 123));

  Supla::Control::DimmerLeds dim(Supla::Io::IoPin(7, &brightnessIo));
  time.advance(1000);
  dim.onInit();
  dim.setRGBWValueOnDevice(0, 0, 0, 123);
}

TEST(DimmerLedsTests, ScalesValuesForFixedEightBitOutput) {
  FixedEightBitPwmIo io;
  Supla::Control::DimmerLeds dim(Supla::Io::IoPin(7, &io));

  dim.setRGBWValueOnDevice(0, 0, 0, 511);
  dim.setRGBWValueOnDevice(0, 0, 0, 767);
  dim.setRGBWValueOnDevice(0, 0, 0, 1023);
  dim.setRGBWValueOnDevice(0, 0, 0, 2000);

  EXPECT_THAT(io.values, ::testing::ElementsAre(127, 191, 255, 255));
}

TEST(DimmerLedsTests,
     ScalesAfterFixedBackendRejectsLegacyTenBitResolution) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  FixedEightBitPwmIo io;

  Supla::Control::DimmerLeds dim(Supla::Io::IoPin(7, &io));

  dim.setFadeEffectTime(0);
  dim.onInit();

  EXPECT_EQ(io.setResolutionCallCount, 1);
  EXPECT_EQ(io.requestedResolutionPin, 7);
  EXPECT_EQ(io.requestedResolutionBits, 10);
  EXPECT_EQ(io.customPwmResolutionBits(7), 8);

  io.clearAnalogWrites();

  dim.setRGBWValueOnDevice(0, 0, 0, 511);
  dim.setRGBWValueOnDevice(0, 0, 0, 767);
  dim.setRGBWValueOnDevice(0, 0, 0, 1023);

  EXPECT_THAT(io.values, ::testing::ElementsAre(127, 191, 255));
}

TEST(DimmerLedsTests, MutableTenBitBackendKeepsFullDutyRange) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  MutableTenBitPwmIo io;
  Supla::Control::DimmerLeds dim(Supla::Io::IoPin(7, &io));

  dim.setFadeEffectTime(0);
  dim.onInit();

  EXPECT_EQ(io.setResolutionCallCount, 1);
  EXPECT_EQ(io.requestedResolutionPin, 7);
  EXPECT_EQ(io.requestedResolutionBits, 10);
  EXPECT_EQ(io.customPwmResolutionBits(7), 10);

  io.clearAnalogWrites();

  dim.setRGBWValueOnDevice(0, 0, 0, 511);
  dim.setRGBWValueOnDevice(0, 0, 0, 767);
  dim.setRGBWValueOnDevice(0, 0, 0, 1023);
  dim.setRGBWValueOnDevice(0, 0, 0, 2000);

  EXPECT_THAT(io.values, ::testing::ElementsAre(511, 767, 1023, 1023));
}

TEST(DimmerLedsTests, UnsetOutputDoesNotQueryPwmBackend) {
  ::testing::StrictMock<UnsetPinPwmIo> io;
  Supla::Control::DimmerLeds dim(Supla::Io::IoPin(-1, &io));

  dim.setRGBWValueOnDevice(0, 0, 0, 1023);
}
