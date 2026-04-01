/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include <gtest/gtest.h>

#include <supla/control/lighting_pwm_leds.h>
#include <supla/io.h>
#include <simple_time.h>
#include <supla_io_mock.h>

class RgbwPwmBaseForTest : public Supla::Control::RGBWPwmBase {
 public:
  RgbwPwmBaseForTest(int out1,
                     int out2,
                     int out3,
                     int out4,
                     int out5)
      : LightingPwmLeds(nullptr, out1, out2, out3, out4, out5) {}

  RgbwPwmBaseForTest(Supla::Io::IoPin out1,
                     Supla::Io::IoPin out2,
                     Supla::Io::IoPin out3,
                     Supla::Io::IoPin out4,
                     Supla::Io::IoPin out5)
      : LightingPwmLeds(nullptr, out1, out2, out3, out4, out5) {}
};

class ResolvedPwmIo : public Supla::Io::Base {
 public:
  ResolvedPwmIo(uint8_t resolutionBits, uint32_t maxDuty)
      : Base(false), resolutionBits(resolutionBits), maxDuty(maxDuty) {}

  void customPinMode(int, uint8_t, uint8_t) override {}
  void customDigitalWrite(int, uint8_t, uint8_t) override {}
  void customAnalogWrite(int, uint8_t, int val) override {
    lastValue = static_cast<uint32_t>(val);
    writeCount++;
  }
  void customConfigureAnalogOutput(int, uint8_t, bool) override {}
  void customSetPwmFrequency(uint16_t pwmFrequency) override {
    lastFrequency = pwmFrequency;
  }

  uint8_t customAnalogWriteResolutionBits() const override {
    return resolutionBits;
  }

  uint32_t customAnalogWriteMaxValue() const override { return maxDuty; }

  uint8_t resolutionBits = 0;
  uint32_t maxDuty = 0;
  uint32_t lastValue = 0;
  uint32_t writeCount = 0;
  uint16_t lastFrequency = 0;
};

TEST(RgbwPwmBaseTests, StoresPerOutputIoSeparately) {
  RgbwPwmBaseForTest pwm(11, 12, 13, 14, 15);
  SuplaIoMock io1;
  SuplaIoMock io2;

  EXPECT_EQ(pwm.getOutputPin(0), 11);
  EXPECT_EQ(pwm.getOutputPin(4), 15);
  EXPECT_EQ(pwm.getOutputIo(0), nullptr);
  EXPECT_EQ(pwm.getOutputIo(4), nullptr);

  pwm.setOutputIo(0, &io1);
  pwm.setOutputIo(3, &io2);

  EXPECT_EQ(pwm.getOutputIo(0), &io1);
  EXPECT_EQ(pwm.getOutputIo(1), nullptr);
  EXPECT_EQ(pwm.getOutputIo(3), &io2);
  EXPECT_EQ(pwm.getOutputIo(4), nullptr);
}

TEST(RgbwPwmBaseTests, IoPinConstructorStoresSeparateIoAndPins) {
  SuplaIoMock io1;
  SuplaIoMock io2;

  RgbwPwmBaseForTest pwm(Supla::Io::IoPin(21, &io1),
                         Supla::Io::IoPin(22, &io2),
                         Supla::Io::IoPin(23),
                         Supla::Io::IoPin(),
                         Supla::Io::IoPin(25, &io1));

  EXPECT_EQ(pwm.getOutputPin(0), 21);
  EXPECT_EQ(pwm.getOutputPin(1), 22);
  EXPECT_EQ(pwm.getOutputPin(2), 23);
  EXPECT_EQ(pwm.getOutputPin(3), -1);
  EXPECT_EQ(pwm.getOutputPin(4), 25);
  EXPECT_EQ(pwm.getOutputIo(0), &io1);
  EXPECT_EQ(pwm.getOutputIo(1), &io2);
  EXPECT_EQ(pwm.getOutputIo(2), nullptr);
  EXPECT_EQ(pwm.getOutputIo(3), nullptr);
  EXPECT_EQ(pwm.getOutputIo(4), &io1);
}

TEST(RgbwPwmBaseTests, InitAndWriteUseInjectedIoPerOutput) {
  SimpleTime time;
  SuplaIoMock io;

  RgbwPwmBaseForTest pwm(Supla::Io::IoPin(21, &io),
                         Supla::Io::IoPin(22, &io),
                         Supla::Io::IoPin(),
                         Supla::Io::IoPin(),
                         Supla::Io::IoPin());

  time.advance(1000);
  EXPECT_CALL(io, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io, customConfigureAnalogOutput(-1, 21, false)).Times(1);
  EXPECT_CALL(io, customPinMode(-1, 21, OUTPUT)).Times(1);
  EXPECT_CALL(io, customConfigureAnalogOutput(-1, 22, false)).Times(1);
  EXPECT_CALL(io, customPinMode(-1, 22, OUTPUT)).Times(1);
  pwm.onInit();

  uint32_t output[5] = {100, 200, 0, 0, 0};
  EXPECT_CALL(io, customAnalogWrite(-1, 21, 100)).Times(1);
  EXPECT_CALL(io, customAnalogWrite(-1, 22, 200)).Times(1);
  pwm.setRGBCCTValueOnDevice(output, 2);
}

TEST(RgbwPwmBaseTests, DisabledChildDoesNotReconfigureParentOutputs) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  SuplaIoMock io;

  Supla::Control::LightingPwmLeds parent(nullptr,
                                         Supla::Io::IoPin(27, &io),
                                         Supla::Io::IoPin(26, &io),
                                         Supla::Io::IoPin(13, &io),
                                         Supla::Io::IoPin(14, &io));
  Supla::Control::LightingPwmLeds child(&parent,
                                        Supla::Io::IoPin(26, &io),
                                        Supla::Io::IoPin(13, &io),
                                        Supla::Io::IoPin(14, &io));

  parent.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_RGBLIGHTING);
  child.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER);

  EXPECT_CALL(io, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io, customConfigureAnalogOutput(-1, 27, false)).Times(1);
  EXPECT_CALL(io, customConfigureAnalogOutput(-1, 26, false)).Times(1);
  EXPECT_CALL(io, customConfigureAnalogOutput(-1, 13, false)).Times(1);
  EXPECT_CALL(io, customConfigureAnalogOutput(-1, 14, false)).Times(1);
  EXPECT_CALL(io, customPinMode(-1, 27, OUTPUT)).Times(1);
  EXPECT_CALL(io, customPinMode(-1, 26, OUTPUT)).Times(1);
  EXPECT_CALL(io, customPinMode(-1, 13, OUTPUT)).Times(1);
  EXPECT_CALL(io, customPinMode(-1, 14, OUTPUT)).Times(1);

  time.advance(1000);
  parent.onInit();
  child.onInit();
}

TEST(RgbwPwmBaseTests, ChildUsesHardwareMaxValueForDutyScaling) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ResolvedPwmIo io(13, 8191);

  Supla::Control::LightingPwmLeds parent(
      nullptr,
      Supla::Io::IoPin(21, &io),
      Supla::Io::IoPin(22, &io),
      Supla::Io::IoPin(23, &io),
      Supla::Io::IoPin(24, &io));
  Supla::Control::LightingPwmLeds child(&parent,
                                        Supla::Io::IoPin(22, &io),
                                        Supla::Io::IoPin(23, &io),
                                        Supla::Io::IoPin(24, &io));

  parent.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER);
  child.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER);
  child.setBrightnessRatioLimits(0.0f, 1.0f);
  child.setColorBrightnessRatioLimits(0.0f, 1.0f);

  time.advance(1000);
  parent.onInit();
  child.onInit();

  uint32_t output[5] = {100, 0, 0, 0, 0};
  child.setRGBCCTValueOnDevice(output, 1);

  EXPECT_EQ(io.lastValue, 100U);
}

TEST(RgbwPwmBaseTests, IdenticalScaledWritesAreSuppressed) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ResolvedPwmIo io(13, 8191);

  Supla::Control::LightingPwmLeds pwm(
      nullptr, Supla::Io::IoPin(21, &io), Supla::Io::IoPin());
  ASSERT_NE(pwm.getChannel(), nullptr);
  pwm.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER);

  time.advance(1000);
  pwm.onInit();

  uint32_t output[5] = {106, 0, 0, 0, 0};
  const uint32_t writesAfterInit = io.writeCount;
  pwm.setRGBCCTValueOnDevice(output, 1);
  pwm.setRGBCCTValueOnDevice(output, 1);

  EXPECT_EQ(io.writeCount, writesAfterInit + 1);
  EXPECT_EQ(io.lastValue, 106U);
}

TEST(RgbwPwmBaseTests, UsesIoResolutionAsHardwareMax) {
  SimpleTime time;
  ResolvedPwmIo io(13, 8191);

  RgbwPwmBaseForTest pwm(Supla::Io::IoPin(21, &io),
                         Supla::Io::IoPin(),
                         Supla::Io::IoPin(),
                         Supla::Io::IoPin(),
                         Supla::Io::IoPin());

  time.advance(1000);
  pwm.onInit();

  uint32_t output[5] = {8191, 0, 0, 0, 0};
  pwm.setRGBCCTValueOnDevice(output, 1);

  EXPECT_EQ(io.lastValue, 8191U);
}

TEST(RgbwPwmBaseTests, DimmerUsesNineBitDutyRange) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ResolvedPwmIo io(9, 511);

  Supla::Io::IoPin pin(21, &io);
  EXPECT_EQ(pin.analogWriteMaxValue(), 511U);

  Supla::Control::LightingPwmLeds pwm(nullptr, pin);
  ASSERT_NE(pwm.getChannel(), nullptr);
  pwm.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER);
  pwm.setFadeEffectTime(0);

  time.advance(1000);
  pwm.onInit();

  auto sendAndFlush = [&](uint8_t whiteBrightness) {
    TSD_SuplaChannelNewValue msg = {};
    msg.value[0] = whiteBrightness;
    msg.value[5] = 0;

    pwm.handleNewValueFromServer(&msg);

    time.advance(1000);
    pwm.iterateAlways();
    time.advance(1);
    pwm.onFastTimer();
    time.advance(1000);
    pwm.iterateAlways();
    time.advance(1);
    pwm.onFastTimer();
  };

  sendAndFlush(0);
  EXPECT_EQ(io.lastValue, 0U);

  sendAndFlush(50);
  EXPECT_EQ(io.lastValue, 254U);

  sendAndFlush(100);
  EXPECT_EQ(io.lastValue, 511U);
}

TEST(RgbwPwmBaseTests, LoweringHwMaxClampsBrightnessLimits) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ResolvedPwmIo io(9, 511);

  Supla::Io::IoPin pin(21, &io);
  EXPECT_EQ(pin.analogWriteMaxValue(), 511U);

  Supla::Control::LightingPwmLeds pwm(nullptr, pin);
  ASSERT_NE(pwm.getChannel(), nullptr);
  pwm.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER);
  pwm.setFadeEffectTime(0);

  time.advance(1000);
  pwm.onInit();

  pwm.setBrightnessRatioLimits(0.0f, 1.0f);
  pwm.setMaxHwValue(255);

  TSD_SuplaChannelNewValue msg = {};
  msg.value[0] = 100;
  msg.value[5] = 0;

  pwm.handleNewValueFromServer(&msg);

  time.advance(1000);
  pwm.iterateAlways();
  time.advance(1);
  pwm.onFastTimer();
  time.advance(1000);
  pwm.iterateAlways();
  time.advance(1);
  pwm.onFastTimer();

  EXPECT_EQ(io.lastValue, 511U);
}

TEST(RgbwPwmBaseTests, LoadConfigForwardsPwmFrequencyToIo) {
  SimpleTime time;
  ResolvedPwmIo io(9, 511);

  Supla::Control::LightingPwmLeds pwm(
      nullptr, Supla::Io::IoPin(21, &io), Supla::Io::IoPin());
  pwm.setPwmFrequency(2345);

  time.advance(1000);
  pwm.onLoadConfig(nullptr);

  EXPECT_EQ(io.lastFrequency, 2345U);
}

TEST(RgbwPwmBaseTests, TinyTimerStepsAccumulateIntoDutyUpdates) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ResolvedPwmIo io(9, 511);

  Supla::Control::LightingPwmLeds pwm(
      nullptr, Supla::Io::IoPin(21, &io), Supla::Io::IoPin());
  ASSERT_NE(pwm.getChannel(), nullptr);
  pwm.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER);
  pwm.setFadeEffectTime(1000);

  time.advance(1000);
  pwm.onInit();

  TSD_SuplaChannelNewValue msg = {};
  msg.value[0] = 100;
  msg.value[5] = 0;
  pwm.handleNewValueFromServer(&msg);

  time.advance(1);
  pwm.onFastTimer();

  for (int i = 0; i < 25; i++) {
    time.advance(1);
    pwm.iterateAlways();
    pwm.onFastTimer();
  }

  EXPECT_GT(io.lastValue, 0U);
}

TEST(RgbwPwmBaseTests, CctOutputsRespectConfiguredBrightnessFloor) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ResolvedPwmIo warmIo(13, 8191);
  ResolvedPwmIo coldIo(13, 8191);

  Supla::Control::LightingPwmLeds pwm(
      nullptr,
      Supla::Io::IoPin(21, &warmIo),
      Supla::Io::IoPin(22, &coldIo));
  ASSERT_NE(pwm.getChannel(), nullptr);
  pwm.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER_CCT);
  pwm.setBrightnessRatioLimits(100.0f / 8191.0f, 1.0f);
  pwm.setColorBrightnessRatioLimits(100.0f / 8191.0f, 1.0f);
  pwm.setFadeEffectTime(0);

  time.advance(1000);
  pwm.onInit();

  TSD_SuplaChannelNewValue msg = {};
  msg.value[0] = 1;
  msg.value[7] = 90;
  msg.value[5] = 0;

  pwm.handleNewValueFromServer(&msg);

  time.advance(1);
  pwm.iterateAlways();
  time.advance(1);
  pwm.onFastTimer();
  time.advance(1);
  pwm.iterateAlways();
  time.advance(1);
  pwm.onFastTimer();

  EXPECT_GE(warmIo.lastValue, 100U);
  EXPECT_GE(coldIo.lastValue, 100U);
}
