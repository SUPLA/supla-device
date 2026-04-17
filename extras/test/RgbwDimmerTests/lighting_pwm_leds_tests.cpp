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
#include <simple_time.h>
#include <supla/control/lighting_pwm_leds.h>
#include <supla/io.h>
#include <supla_io_mock.h>

#include <array>
#include <memory>

class RgbwPwmBaseForTest : public Supla::Control::RGBWPwmBase {
 public:
  RgbwPwmBaseForTest(int out1, int out2, int out3, int out4, int out5)
      : LightingPwmLeds(nullptr, out1, out2, out3, out4, out5) {
  }

  RgbwPwmBaseForTest(Supla::Io::IoPin out1,
                     Supla::Io::IoPin out2,
                     Supla::Io::IoPin out3,
                     Supla::Io::IoPin out4,
                     Supla::Io::IoPin out5)
      : LightingPwmLeds(nullptr, out1, out2, out3, out4, out5) {
  }
};

class LightingPwmLedsForTest : public Supla::Control::LightingPwmLeds {
 public:
  using Supla::Control::LightingPwmLeds::LightingPwmLeds;

  bool isEnabledForTest() const {
    return enabled;
  }
  int missingGpioCountForTest() const {
    return getMissingGpioCount();
  }
  const Supla::Io::IoPin &outputPinForTest(int index) const {
    return outputs[index].pin;
  }
};

class ResolvedPwmIo : public Supla::Io::Base {
 public:
  ResolvedPwmIo(uint8_t resolutionBits, uint32_t maxDuty)
      : Base(), resolutionBits(resolutionBits), maxDuty(maxDuty) {
  }

  void customPinMode(int, uint8_t, uint8_t) override {
  }
  void customDigitalWrite(int, uint8_t, uint8_t) override {
  }
  void customAnalogWrite(int, uint8_t, int val) override {
    lastValue = static_cast<uint32_t>(val);
    writeCount++;
  }
  void customConfigureAnalogOutput(int, uint8_t, bool) override {
  }
  void customSetPwmFrequency(uint16_t pwmFrequency) override {
    lastFrequency = pwmFrequency;
  }

  uint8_t customPwmResolutionBits(uint8_t pin) const override {
    (void)(pin);
    return resolutionBits;
  }

  uint32_t customPwmMaxValue(uint8_t pin) const override {
    (void)(pin);
    return maxDuty;
  }

  uint8_t resolutionBits = 0;
  uint32_t maxDuty = 0;
  uint32_t lastValue = 0;
  uint32_t writeCount = 0;
  uint16_t lastFrequency = 0;
};

TEST(RgbwPwmBaseTests, ConstructorUsesSeparateIoForOutputs) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  SuplaIoMock io1;
  SuplaIoMock io2;

  EXPECT_CALL(io1, customSetPwmResolutionBits(21, 10)).Times(1);
  EXPECT_CALL(io2, customSetPwmResolutionBits(22, 10)).Times(1);
  EXPECT_CALL(io1, customSetPwmResolutionBits(23, 10)).Times(1);
  EXPECT_CALL(io2, customSetPwmResolutionBits(24, 10)).Times(1);
  EXPECT_CALL(io1, customSetPwmResolutionBits(25, 10)).Times(1);
  EXPECT_CALL(io1, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io2, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io1, customConfigureAnalogOutput(-1, 21, false)).Times(1);
  EXPECT_CALL(io2, customConfigureAnalogOutput(-1, 22, false)).Times(1);
  EXPECT_CALL(io1, customPinMode(-1, 21, OUTPUT)).Times(1);
  EXPECT_CALL(io2, customPinMode(-1, 22, OUTPUT)).Times(1);
  EXPECT_CALL(io1, customConfigureAnalogOutput(-1, 23, false)).Times(1);
  EXPECT_CALL(io2, customConfigureAnalogOutput(-1, 24, false)).Times(1);
  EXPECT_CALL(io1, customConfigureAnalogOutput(-1, 25, false)).Times(1);
  EXPECT_CALL(io1, customPinMode(-1, 23, OUTPUT)).Times(1);
  EXPECT_CALL(io2, customPinMode(-1, 24, OUTPUT)).Times(1);
  EXPECT_CALL(io1, customPinMode(-1, 25, OUTPUT)).Times(1);

  RgbwPwmBaseForTest pwm(Supla::Io::IoPin(21, &io1),
                         Supla::Io::IoPin(22, &io2),
                         Supla::Io::IoPin(23, &io1),
                         Supla::Io::IoPin(24, &io2),
                         Supla::Io::IoPin(25, &io1));

  time.advance(1000);
  pwm.onInit();
}

TEST(RgbwPwmBaseTests, DefaultFuncListMatchesConfiguredOutputCount) {
  Supla::Channel::resetToDefaults();

  const struct {
    int outputs;
    uint32_t expectedFuncList;
    uint32_t expectedDefault;
  } cases[] = {
      {1, SUPLA_RGBW_BIT_FUNC_DIMMER, SUPLA_CHANNELFNC_DIMMER},
      {2,
       SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_DIMMER_CCT,
       SUPLA_CHANNELFNC_DIMMER_CCT},
      {3,
       SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
           SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING,
       SUPLA_CHANNELFNC_RGBLIGHTING},
      {4,
       SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
           SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
           SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING,
       SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING},
      {5,
       SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
           SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
           SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
           SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB,
       SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB},
  };

  for (const auto &testCase : cases) {
    RgbwPwmBaseForTest pwm(1,
                           testCase.outputs >= 2 ? 2 : -1,
                           testCase.outputs >= 3 ? 3 : -1,
                           testCase.outputs >= 4 ? 4 : -1,
                           testCase.outputs >= 5 ? 5 : -1);

    EXPECT_EQ(pwm.getChannel()->getFuncList(), testCase.expectedFuncList)
        << "outputs=" << testCase.outputs;
    EXPECT_EQ(pwm.getChannel()->getDefaultFunction(), testCase.expectedDefault)
        << "outputs=" << testCase.outputs;
  }
}

TEST(RgbwPwmBaseTests, ConstructorInitializesConfiguredOutputs) {
  SimpleTime time;
  SuplaIoMock io1;
  SuplaIoMock io2;

  EXPECT_CALL(io1, customSetPwmResolutionBits(21, 10)).Times(1);
  EXPECT_CALL(io2, customSetPwmResolutionBits(22, 10)).Times(1);
  EXPECT_CALL(io1, customSetPwmResolutionBits(23, 10)).Times(1);
  EXPECT_CALL(io2, customSetPwmResolutionBits(24, 10)).Times(1);
  EXPECT_CALL(io1, customSetPwmResolutionBits(25, 10)).Times(1);
  EXPECT_CALL(io1, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io2, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io1, customConfigureAnalogOutput(-1, 21, false)).Times(1);
  EXPECT_CALL(io2, customConfigureAnalogOutput(-1, 22, false)).Times(1);
  EXPECT_CALL(io1, customConfigureAnalogOutput(-1, 23, false)).Times(1);
  EXPECT_CALL(io2, customConfigureAnalogOutput(-1, 24, false)).Times(1);
  EXPECT_CALL(io1, customConfigureAnalogOutput(-1, 25, false)).Times(1);
  EXPECT_CALL(io1, customPinMode(-1, 21, OUTPUT)).Times(1);
  EXPECT_CALL(io2, customPinMode(-1, 22, OUTPUT)).Times(1);
  EXPECT_CALL(io1, customPinMode(-1, 23, OUTPUT)).Times(1);
  EXPECT_CALL(io2, customPinMode(-1, 24, OUTPUT)).Times(1);
  EXPECT_CALL(io1, customPinMode(-1, 25, OUTPUT)).Times(1);

  RgbwPwmBaseForTest pwm(Supla::Io::IoPin(21, &io1),
                         Supla::Io::IoPin(22, &io2),
                         Supla::Io::IoPin(23, &io1),
                         Supla::Io::IoPin(24, &io2),
                         Supla::Io::IoPin(25, &io1));

  time.advance(1000);
  pwm.onInit();

  EXPECT_CALL(io1, customAnalogWrite(-1, 21, 1)).Times(1);
  EXPECT_CALL(io2, customAnalogWrite(-1, 22, 2)).Times(1);
  EXPECT_CALL(io1, customAnalogWrite(-1, 23, 3)).Times(1);
  EXPECT_CALL(io2, customAnalogWrite(-1, 24, 4)).Times(1);
  EXPECT_CALL(io1, customAnalogWrite(-1, 25, 5)).Times(1);

  uint32_t output[5] = {1, 2, 3, 4, 5};
  pwm.setRGBCCTValueOnDevice(output, 5);
}

TEST(RgbwPwmBaseTests, InitAndWriteUseInjectedIoPerOutput) {
  SimpleTime time;
  SuplaIoMock io;

  EXPECT_CALL(io, customSetPwmResolutionBits(21, 10)).Times(1);
  EXPECT_CALL(io, customSetPwmResolutionBits(22, 10)).Times(1);
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

TEST(RgbwPwmBaseTests,
     InitUsesEachIoOnceForFrequencyAndEachPinForResolutionBits) {
  SimpleTime time;
  SuplaIoMock io1;
  SuplaIoMock io2;
  SuplaIoMock io3;

  EXPECT_CALL(io1, customSetPwmResolutionBits(21, 10)).Times(1);
  EXPECT_CALL(io2, customSetPwmResolutionBits(22, 10)).Times(1);
  EXPECT_CALL(io3, customSetPwmResolutionBits(23, 10)).Times(1);
  EXPECT_CALL(io1, customSetPwmResolutionBits(24, 10)).Times(1);
  EXPECT_CALL(io2, customSetPwmResolutionBits(25, 10)).Times(1);
  EXPECT_CALL(io1, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io2, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io3, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io1, customConfigureAnalogOutput(-1, 21, false)).Times(1);
  EXPECT_CALL(io2, customConfigureAnalogOutput(-1, 22, false)).Times(1);
  EXPECT_CALL(io3, customConfigureAnalogOutput(-1, 23, false)).Times(1);
  EXPECT_CALL(io1, customConfigureAnalogOutput(-1, 24, false)).Times(1);
  EXPECT_CALL(io2, customConfigureAnalogOutput(-1, 25, false)).Times(1);
  EXPECT_CALL(io1, customPinMode(-1, 21, OUTPUT)).Times(1);
  EXPECT_CALL(io2, customPinMode(-1, 22, OUTPUT)).Times(1);
  EXPECT_CALL(io3, customPinMode(-1, 23, OUTPUT)).Times(1);
  EXPECT_CALL(io1, customPinMode(-1, 24, OUTPUT)).Times(1);
  EXPECT_CALL(io2, customPinMode(-1, 25, OUTPUT)).Times(1);

  RgbwPwmBaseForTest pwm(Supla::Io::IoPin(21, &io1),
                         Supla::Io::IoPin(22, &io2),
                         Supla::Io::IoPin(23, &io3),
                         Supla::Io::IoPin(24, &io1),
                         Supla::Io::IoPin(25, &io2));

  time.advance(1000);
  pwm.onInit();
}

TEST(RgbwPwmBaseTests, ParentChildMixesCustomAndDefaultIoPerPin) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  DigitalInterfaceMock hwInterfaceMock;
  SuplaIoMock io1;
  SuplaIoMock io2;
  SuplaIoMock io3;
  SuplaIoMock io4;
  SuplaIoMock io5;

  EXPECT_CALL(io1, customSetPwmResolutionBits(1, 10)).Times(1);
  EXPECT_CALL(io2, customSetPwmResolutionBits(2, 10)).Times(1);
  EXPECT_CALL(io3, customSetPwmResolutionBits(3, 10)).Times(1);
  EXPECT_CALL(io4, customSetPwmResolutionBits(4, 10)).Times(1);
  EXPECT_CALL(io5, customSetPwmResolutionBits(5, 10)).Times(1);
  EXPECT_CALL(hwInterfaceMock, analogWriteResolution(6, 10)).Times(1);

  EXPECT_CALL(io1, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io2, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io3, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io4, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(io5, customSetPwmFrequency(500)).Times(1);
  EXPECT_CALL(hwInterfaceMock, analogWriteFrequency(6, 500)).Times(1);

  EXPECT_CALL(io1, customConfigureAnalogOutput(-1, 1, false)).Times(1);
  EXPECT_CALL(io2, customConfigureAnalogOutput(-1, 2, false)).Times(1);
  EXPECT_CALL(io3, customConfigureAnalogOutput(-1, 3, false)).Times(1);
  EXPECT_CALL(io4, customConfigureAnalogOutput(-1, 4, false)).Times(1);
  EXPECT_CALL(io5, customConfigureAnalogOutput(-1, 5, false)).Times(1);
  EXPECT_CALL(hwInterfaceMock, pinMode(6, OUTPUT)).Times(1);
  EXPECT_CALL(io1, customPinMode(-1, 1, OUTPUT)).Times(1);
  EXPECT_CALL(io2, customPinMode(-1, 2, OUTPUT)).Times(1);
  EXPECT_CALL(io3, customPinMode(-1, 3, OUTPUT)).Times(1);
  EXPECT_CALL(io4, customPinMode(-1, 4, OUTPUT)).Times(1);
  EXPECT_CALL(io5, customPinMode(-1, 5, OUTPUT)).Times(1);

  LightingPwmLedsForTest parent(nullptr,
                                Supla::Io::IoPin(1, &io1),
                                Supla::Io::IoPin(2, &io2),
                                Supla::Io::IoPin(3, &io3),
                                Supla::Io::IoPin(4, &io4),
                                Supla::Io::IoPin(5, &io5));
  LightingPwmLedsForTest child(&parent,
                               Supla::Io::IoPin(2, &io2),
                               Supla::Io::IoPin(3, &io3),
                               Supla::Io::IoPin(4, &io4),
                               Supla::Io::IoPin(5, &io5),
                               Supla::Io::IoPin(6));

  parent.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB);
  child.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB);

  time.advance(1000);
  parent.onInit();
  child.onInit();
}

TEST(RgbwPwmBaseTests, InitWithoutCustomIoKeepsDefaultPwmState) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  DigitalInterfaceMock hwInterfaceMock;

  LightingPwmLedsForTest pwm(nullptr,
                             Supla::Io::IoPin(21),
                             Supla::Io::IoPin(22),
                             Supla::Io::IoPin(23));
  ASSERT_NE(pwm.getChannel(), nullptr);
  pwm.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_RGBLIGHTING);

  EXPECT_CALL(hwInterfaceMock, pinMode(21, OUTPUT)).Times(1);
  EXPECT_CALL(hwInterfaceMock, pinMode(22, OUTPUT)).Times(1);
  EXPECT_CALL(hwInterfaceMock, pinMode(23, OUTPUT)).Times(1);
  EXPECT_CALL(hwInterfaceMock, analogWriteResolution(21, 10)).Times(1);
  EXPECT_CALL(hwInterfaceMock, analogWriteResolution(22, 10)).Times(1);
  EXPECT_CALL(hwInterfaceMock, analogWriteResolution(23, 10)).Times(1);
  EXPECT_CALL(hwInterfaceMock, analogWriteFrequency(21, 500)).Times(1);
  EXPECT_EQ(pwm.outputPinForTest(0).io, nullptr);
  EXPECT_EQ(pwm.outputPinForTest(1).io, nullptr);
  EXPECT_EQ(pwm.outputPinForTest(2).io, nullptr);

  time.advance(1000);
  pwm.onInit();

  EXPECT_EQ(pwm.outputPinForTest(0).pwmResolutionBits(), 10U);
  EXPECT_EQ(pwm.outputPinForTest(1).pwmResolutionBits(), 10U);
  EXPECT_EQ(pwm.outputPinForTest(2).pwmResolutionBits(), 10U);
  EXPECT_EQ(pwm.outputPinForTest(0).pwmMaxValue(), 1023U);
  EXPECT_EQ(pwm.outputPinForTest(1).pwmMaxValue(), 1023U);
  EXPECT_EQ(pwm.outputPinForTest(2).pwmMaxValue(), 1023U);
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

  EXPECT_CALL(io, customSetPwmResolutionBits(27, 10)).Times(1);
  EXPECT_CALL(io, customSetPwmResolutionBits(26, 10)).Times(1);
  EXPECT_CALL(io, customSetPwmResolutionBits(13, 10)).Times(1);
  EXPECT_CALL(io, customSetPwmResolutionBits(14, 10)).Times(1);
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

  Supla::Control::LightingPwmLeds parent(nullptr,
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
  EXPECT_EQ(pin.pwmMaxValue(), 511U);

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
  EXPECT_EQ(io.lastValue, 255U);

  sendAndFlush(100);
  EXPECT_EQ(io.lastValue, 511U);
}

TEST(RgbwPwmBaseTests, LoweringHwMaxClampsBrightnessLimits) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ResolvedPwmIo io(9, 511);

  Supla::Io::IoPin pin(21, &io);
  EXPECT_EQ(pin.pwmMaxValue(), 511U);

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

TEST(RgbwPwmBaseTests, TenInstanceChainEnablesExpectedStartSlot) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ResolvedPwmIo io(13, 8191);

  std::array<std::unique_ptr<LightingPwmLedsForTest>, 10> lights;
  lights[0] =
      std::make_unique<LightingPwmLedsForTest>(nullptr,
                                               Supla::Io::IoPin(1, &io),
                                               Supla::Io::IoPin(2, &io),
                                               Supla::Io::IoPin(3, &io),
                                               Supla::Io::IoPin(4, &io),
                                               Supla::Io::IoPin(5, &io));
  lights[1] =
      std::make_unique<LightingPwmLedsForTest>(lights[0].get(),
                                               Supla::Io::IoPin(2, &io),
                                               Supla::Io::IoPin(3, &io),
                                               Supla::Io::IoPin(4, &io),
                                               Supla::Io::IoPin(5, &io),
                                               Supla::Io::IoPin(6, &io));
  lights[2] =
      std::make_unique<LightingPwmLedsForTest>(lights[1].get(),
                                               Supla::Io::IoPin(3, &io),
                                               Supla::Io::IoPin(4, &io),
                                               Supla::Io::IoPin(5, &io),
                                               Supla::Io::IoPin(6, &io),
                                               Supla::Io::IoPin(7, &io));
  lights[3] =
      std::make_unique<LightingPwmLedsForTest>(lights[2].get(),
                                               Supla::Io::IoPin(4, &io),
                                               Supla::Io::IoPin(5, &io),
                                               Supla::Io::IoPin(6, &io),
                                               Supla::Io::IoPin(7, &io),
                                               Supla::Io::IoPin(8, &io));
  lights[4] =
      std::make_unique<LightingPwmLedsForTest>(lights[3].get(),
                                               Supla::Io::IoPin(5, &io),
                                               Supla::Io::IoPin(6, &io),
                                               Supla::Io::IoPin(7, &io),
                                               Supla::Io::IoPin(8, &io),
                                               Supla::Io::IoPin(9, &io));
  lights[5] =
      std::make_unique<LightingPwmLedsForTest>(lights[4].get(),
                                               Supla::Io::IoPin(6, &io),
                                               Supla::Io::IoPin(7, &io),
                                               Supla::Io::IoPin(8, &io),
                                               Supla::Io::IoPin(9, &io),
                                               Supla::Io::IoPin(10, &io));
  lights[6] =
      std::make_unique<LightingPwmLedsForTest>(lights[5].get(),
                                               Supla::Io::IoPin(7, &io),
                                               Supla::Io::IoPin(8, &io),
                                               Supla::Io::IoPin(9, &io),
                                               Supla::Io::IoPin(10, &io));
  lights[7] =
      std::make_unique<LightingPwmLedsForTest>(lights[6].get(),
                                               Supla::Io::IoPin(8, &io),
                                               Supla::Io::IoPin(9, &io),
                                               Supla::Io::IoPin(10, &io));
  lights[8] = std::make_unique<LightingPwmLedsForTest>(
      lights[7].get(), Supla::Io::IoPin(9, &io), Supla::Io::IoPin(10, &io));
  lights[9] = std::make_unique<LightingPwmLedsForTest>(
      lights[8].get(), Supla::Io::IoPin(10, &io));

  const uint32_t expectedFuncLists[10] = {
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB,
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB,
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB,
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB,
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB,
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB,
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT,
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
          SUPLA_RGBW_BIT_FUNC_DIMMER_CCT,
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_DIMMER_CCT,
      SUPLA_RGBW_BIT_FUNC_DIMMER,
  };

  const uint32_t expectedDefaultFunctions[10] = {
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING,
      SUPLA_CHANNELFNC_RGBLIGHTING,
      SUPLA_CHANNELFNC_DIMMER_CCT,
      SUPLA_CHANNELFNC_DIMMER,
  };

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(lights[i]->getChannel()->getFuncList(), expectedFuncLists[i])
        << "index " << i;
    EXPECT_EQ(lights[i]->getChannel()->getDefaultFunction(),
              expectedDefaultFunctions[i])
        << "index " << i;
  }

  const uint32_t functions[10] = {
      SUPLA_CHANNELFNC_DIMMER_CCT,
      SUPLA_CHANNELFNC_DIMMER,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER,
      SUPLA_CHANNELFNC_RGBLIGHTING,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
      SUPLA_CHANNELFNC_DIMMER,
  };

  for (int i = 0; i < 10; ++i) {
    lights[i]->getChannel()->setDefaultFunction(functions[i]);
  }

  time.advance(1000);
  for (auto &light : lights) {
    light->onInit();
  }

  EXPECT_TRUE(lights[0]->isEnabledForTest());
  EXPECT_FALSE(lights[1]->isEnabledForTest());
  EXPECT_TRUE(lights[2]->isEnabledForTest());
  EXPECT_FALSE(lights[3]->isEnabledForTest());
  EXPECT_FALSE(lights[4]->isEnabledForTest());
  EXPECT_FALSE(lights[5]->isEnabledForTest());
  EXPECT_FALSE(lights[6]->isEnabledForTest());
  EXPECT_TRUE(lights[7]->isEnabledForTest());
  EXPECT_FALSE(lights[8]->isEnabledForTest());
  EXPECT_FALSE(lights[9]->isEnabledForTest());
}

TEST(RgbwPwmBaseTests, LoadConfigForwardsPwmResolutionBitsAndFrequencyToIo) {
  SimpleTime time;
  SuplaIoMock io;

  Supla::Control::LightingPwmLeds pwm(
      nullptr, Supla::Io::IoPin(21, &io), Supla::Io::IoPin(22, &io));
  pwm.setPwmFrequency(2345);

  time.advance(1000);
  EXPECT_CALL(io, customSetPwmResolutionBits(21, 10)).Times(1);
  EXPECT_CALL(io, customSetPwmResolutionBits(22, 10)).Times(1);
  EXPECT_CALL(io, customSetPwmFrequency(2345)).Times(1);
  pwm.onLoadConfig(nullptr);
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
      nullptr, Supla::Io::IoPin(21, &warmIo), Supla::Io::IoPin(22, &coldIo));
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
