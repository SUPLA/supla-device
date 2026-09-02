// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <arduino_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/control/rgbw_leds.h>
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

TEST(RgbwLedsTests, SettingNewRGBWValue) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  DigitalInterfaceMock ioMock;

  EXPECT_CALL(ioMock, analogWriteResolution(1, 10)).Times(1);
  EXPECT_CALL(ioMock, analogWriteFrequency(1, 1000)).Times(1);
  EXPECT_CALL(ioMock, analogWriteResolution(2, 10)).Times(1);
  EXPECT_CALL(ioMock, analogWriteFrequency(2, 1000)).Times(1);
  EXPECT_CALL(ioMock, analogWriteResolution(3, 10)).Times(1);
  EXPECT_CALL(ioMock, analogWriteFrequency(3, 1000)).Times(1);
  EXPECT_CALL(ioMock, analogWriteResolution(4, 10)).Times(1);
  EXPECT_CALL(ioMock, analogWriteFrequency(4, 1000)).Times(1);
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(2, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(3, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(4, OUTPUT));
  EXPECT_CALL(ioMock, analogWrite(1, 0));
  EXPECT_CALL(ioMock, analogWrite(2, 0));
  EXPECT_CALL(ioMock, analogWrite(3, 0));
  EXPECT_CALL(ioMock, analogWrite(4, 0));

  EXPECT_CALL(ioMock, analogWrite(1, (1 * 1023 / 255)));
  EXPECT_CALL(ioMock, analogWrite(2, (2 * 1023 / 255)));
  EXPECT_CALL(ioMock, analogWrite(3, (3 * 1023 / 255)));
  EXPECT_CALL(ioMock, analogWrite(4, 1023));

  Supla::Control::RGBWLeds rgbw(1, 2, 3, 4);

  auto ch = rgbw.getChannel();
  // disable fading effect so we'll get instant setting value on device call
  rgbw.setFadeEffectTime(0);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(1000);
  rgbw.onInit();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(1000);
  rgbw.iterateAlways();
  time.advance(1);
  rgbw.onFastTimer();
  time.advance(1);
  rgbw.onFastTimer();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgbw.setRGBW(1, 2, 3, 100, 100);

  time.advance(1000);
  rgbw.iterateAlways();
  time.advance(1);
  rgbw.onFastTimer();
  time.advance(1);
  rgbw.onFastTimer();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);
}

TEST(RgbwLedsTests, IoPinConstructorUsesSeparateIoForOutputs) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  SuplaIoMock redIo;
  SuplaIoMock greenIo;
  SuplaIoMock blueIo;
  SuplaIoMock brightnessIo;

  EXPECT_CALL(redIo, customSetPwmResolutionBits(1, 10));
  EXPECT_CALL(redIo, customSetPwmFrequency(1000));
  EXPECT_CALL(greenIo, customSetPwmResolutionBits(2, 10));
  EXPECT_CALL(greenIo, customSetPwmFrequency(1000));
  EXPECT_CALL(blueIo, customSetPwmResolutionBits(3, 10));
  EXPECT_CALL(blueIo, customSetPwmFrequency(1000));
  EXPECT_CALL(brightnessIo, customSetPwmResolutionBits(4, 10));
  EXPECT_CALL(brightnessIo, customSetPwmFrequency(1000));
  EXPECT_CALL(redIo, customConfigureAnalogOutput(-1, 1, false));
  EXPECT_CALL(greenIo, customConfigureAnalogOutput(-1, 2, false));
  EXPECT_CALL(blueIo, customConfigureAnalogOutput(-1, 3, false));
  EXPECT_CALL(brightnessIo, customConfigureAnalogOutput(-1, 4, false));
  EXPECT_CALL(redIo, customPinMode(-1, 1, OUTPUT));
  EXPECT_CALL(greenIo, customPinMode(-1, 2, OUTPUT));
  EXPECT_CALL(blueIo, customPinMode(-1, 3, OUTPUT));
  EXPECT_CALL(brightnessIo, customPinMode(-1, 4, OUTPUT));

  EXPECT_CALL(redIo, customAnalogWrite(-1, 1, 1));
  EXPECT_CALL(greenIo, customAnalogWrite(-1, 2, 2));
  EXPECT_CALL(blueIo, customAnalogWrite(-1, 3, 3));
  EXPECT_CALL(brightnessIo, customAnalogWrite(-1, 4, 100));

  Supla::Control::RGBWLeds rgbw(Supla::Io::IoPin(1, &redIo),
                                Supla::Io::IoPin(2, &greenIo),
                                Supla::Io::IoPin(3, &blueIo),
                                Supla::Io::IoPin(4, &brightnessIo));

  time.advance(1000);
  rgbw.onInit();
  rgbw.setRGBWValueOnDevice(1, 2, 3, 100);
}

TEST(RgbwLedsTests, ScalesRgbAndBrightnessIndependentlyForFixedOutputs) {
  FixedEightBitPwmIo redIo;
  FixedEightBitPwmIo greenIo;
  FixedEightBitPwmIo blueIo;
  FixedEightBitPwmIo brightnessIo;
  Supla::Control::RGBWLeds rgbw(Supla::Io::IoPin(1, &redIo),
                                Supla::Io::IoPin(2, &greenIo),
                                Supla::Io::IoPin(3, &blueIo),
                                Supla::Io::IoPin(4, &brightnessIo));

  rgbw.setRGBWValueOnDevice(511, 767, 1023, 511);

  EXPECT_THAT(redIo.values, ::testing::ElementsAre(127));
  EXPECT_THAT(greenIo.values, ::testing::ElementsAre(191));
  EXPECT_THAT(blueIo.values, ::testing::ElementsAre(255));
  EXPECT_THAT(brightnessIo.values, ::testing::ElementsAre(127));
}

TEST(RgbwLedsTests, UsesTheEffectiveRangeOfEachOutputBackend) {
  FixedEightBitPwmIo redIo;
  MutableTenBitPwmIo greenIo;
  FixedEightBitPwmIo blueIo;
  MutableTenBitPwmIo brightnessIo;
  Supla::Control::RGBWLeds rgbw(Supla::Io::IoPin(1, &redIo),
                                Supla::Io::IoPin(2, &greenIo),
                                Supla::Io::IoPin(3, &blueIo),
                                Supla::Io::IoPin(4, &brightnessIo));

  rgbw.setRGBWValueOnDevice(1023, 1023, 1023, 1023);

  EXPECT_THAT(redIo.values, ::testing::ElementsAre(255));
  EXPECT_THAT(greenIo.values, ::testing::ElementsAre(1023));
  EXPECT_THAT(blueIo.values, ::testing::ElementsAre(255));
  EXPECT_THAT(brightnessIo.values, ::testing::ElementsAre(1023));
}
