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

#include <arduino_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/control/rgbw_leds.h>
#include <supla_io_mock.h>

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

  EXPECT_CALL(redIo, customSetPwmResolutionBits(10));
  EXPECT_CALL(redIo, customSetPwmFrequency(1000));
  EXPECT_CALL(greenIo, customSetPwmResolutionBits(10));
  EXPECT_CALL(greenIo, customSetPwmFrequency(1000));
  EXPECT_CALL(blueIo, customSetPwmResolutionBits(10));
  EXPECT_CALL(blueIo, customSetPwmFrequency(1000));
  EXPECT_CALL(brightnessIo, customSetPwmResolutionBits(10));
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
