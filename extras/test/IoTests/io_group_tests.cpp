// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <arduino_mock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/control/lighting_pwm_leds.h>
#include <supla/control/relay.h>
#include <supla/control/roller_shutter.h>
#include <supla/device/register_device.h>
#include <supla/io/io_group.h>
#include <supla_io_mock.h>

#include <array>

using Supla::Io::IoGroup;
using Supla::Io::IoPin;
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
class GroupBackendMock : public SuplaIoMock {
 public:
  MOCK_METHOD(bool, isReady, (), (const, override));
  MOCK_METHOD(uint8_t, customDefaultPwmResolutionBits, (uint8_t),
              (const, override));
  MOCK_METHOD(bool, customCanSetPwmResolutionBits, (uint8_t),
              (const, override));
  MOCK_METHOD(uint16_t, customPwmFrequency, (), (const, override));
};

class GroupRollerShutter : public Supla::Control::RollerShutter {
 public:
  using RollerShutter::RollerShutter;
  using RollerShutter::startClosing;
  using RollerShutter::startOpening;
  using RollerShutter::stopMovement;
};

void InterruptCallback() {
}

class RecordingPwmIo : public Supla::Io::Base {
 public:
  void customAnalogWrite(int, uint8_t pin, int value) override {
    values.at(pin) = value;
    ++writes;
  }

  bool customCanSetPwmResolutionBits(uint8_t) const override {
    return false;
  }

  uint8_t customDefaultPwmResolutionBits(uint8_t) const override {
    return 8;
  }

  uint8_t customPwmResolutionBits(uint8_t) const override {
    return 8;
  }

  std::array<int, 16> values{};
  int writes = 0;
};
}  // namespace

TEST(IoGroupTests, FanOutPreservesValuesChannelsAndDifferentBackends) {
  StrictMock<SuplaIoMock> first;
  StrictMock<SuplaIoMock> second;
  const IoPin pins[] = {
      IoPin(25, &first), IoPin(26, &second), IoPin(27, &first)};
  IoGroup group(pins, 3);
  auto output = group.getPin();
  output.setMode(OUTPUT);

  ::testing::InSequence sequence;
  EXPECT_CALL(first, customPinMode(7, 25, OUTPUT));
  EXPECT_CALL(second, customPinMode(7, 26, OUTPUT));
  EXPECT_CALL(first, customPinMode(7, 27, OUTPUT));
  EXPECT_CALL(first, customDigitalWrite(7, 25, HIGH));
  EXPECT_CALL(second, customDigitalWrite(7, 26, HIGH));
  EXPECT_CALL(first, customDigitalWrite(7, 27, HIGH));
  EXPECT_CALL(first, customAnalogWrite(7, 25, 321));
  EXPECT_CALL(second, customAnalogWrite(7, 26, 321));
  EXPECT_CALL(first, customAnalogWrite(7, 27, 321));

  output.pinMode(7);
  output.writeActive(7);
  output.analogWrite(321, 7);
}

TEST(IoGroupTests, DefaultGpioBackendAlsoReceivesConfigurationAndWrites) {
  StrictMock<DigitalInterfaceMock> io;
  const IoPin pins[] = {IoPin(25), IoPin(26), IoPin(27)};
  IoGroup group(pins, 3);
  auto output = group.getPin();
  output.setMode(OUTPUT);
  EXPECT_TRUE(group.isReady());
  for (uint8_t pin : {25, 26, 27}) {
    EXPECT_CALL(io, pinMode(pin, OUTPUT));
    EXPECT_CALL(io, digitalWrite(pin, LOW));
    EXPECT_CALL(io, analogWrite(pin, 512));
    EXPECT_CALL(io, analogWriteResolution(pin, 10));
    EXPECT_CALL(io, analogWriteFrequency(pin, 500));
  }
  EXPECT_CALL(io, digitalRead(25)).WillOnce(Return(HIGH));
  output.pinMode();
  output.writeInactive();
  output.setPwmResolutionBits(10);
  output.setPwmFrequency(500);
  output.configureAnalogOutput();
  output.analogWrite(512);
  EXPECT_TRUE(output.readActive());
}

TEST(IoGroupTests, OuterPinControlsModeAndPullUpWithoutChangingMembers) {
  StrictMock<SuplaIoMock> io;
  IoPin pins[] = {IoPin(5, &io), IoPin(6, &io)};
  pins[0].setActiveHigh(false);
  pins[0].setMode(OUTPUT);
  IoGroup group(pins, 2);
  auto output = group.getPin();
  output.setMode(INPUT);
  output.setPullUp(true);
  for (uint8_t pin : {5, 6}) {
    EXPECT_CALL(io, customPinMode(4, pin, INPUT_PULLUP));
  }
  output.pinMode(4);
  EXPECT_FALSE(pins[0].isActiveHigh());
  EXPECT_TRUE(pins[1].isActiveHigh());
  EXPECT_EQ(pins[0].getMode(), OUTPUT);
}

TEST(IoGroupTests, DigitalAndPwmPolarityComposeForEveryMemberAndOuterPin) {
  for (bool outerActiveHigh : {false, true}) {
    for (bool firstActiveHigh : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "outer=" << outerActiveHigh
                   << " first=" << firstActiveHigh);
      StrictMock<SuplaIoMock> io;
      IoPin pins[] = {IoPin(5, &io), IoPin(6, &io)};
      pins[0].setActiveHigh(firstActiveHigh);
      pins[1].setActiveHigh(!firstActiveHigh);
      IoGroup group(pins, 2);
      auto output = group.getPin();
      output.setActiveHigh(outerActiveHigh);
      ::testing::InSequence sequence;
      for (bool active : {true, false}) {
        for (const auto &pin : pins) {
          const bool effectiveActiveHigh =
              pin.isActiveHigh() == outerActiveHigh;
          EXPECT_CALL(io, customDigitalWrite(
                              4, pin.pin, active == effectiveActiveHigh));
        }
        EXPECT_CALL(io, customDigitalRead(4, 5))
            .WillOnce(Return(active == (firstActiveHigh == outerActiveHigh)));
        if (active) {
          output.writeActive(4);
        } else {
          output.writeInactive(4);
        }
        EXPECT_EQ(output.readActive(4), active);
      }
      for (const auto &pin : pins) {
        EXPECT_CALL(io, customConfigureAnalogOutput(
                            4, pin.pin, pin.isActiveHigh() != outerActiveHigh));
      }
      output.configureAnalogOutput(4);
      for (const auto &pin : pins) {
        EXPECT_CALL(io, customAnalogWrite(4, pin.pin, 123));
      }
      output.analogWrite(123, 4);
      EXPECT_EQ(pins[0].isActiveHigh(), firstActiveHigh);
      EXPECT_EQ(pins[1].isActiveHigh(), !firstActiveHigh);
    }
  }
}

TEST(IoGroupTests, DigitalReadNormalizesFirstMemberAndUnsetPinReturnsZero) {
  StrictMock<SuplaIoMock> io;
  IoPin first(5, &io);
  first.setActiveHigh(false);
  const IoPin pins[] = {first, IoPin(6, &io)};
  IoGroup group(pins, 2);
  EXPECT_CALL(io, customDigitalRead(4, 5))
      .WillOnce(Return(LOW)).WillOnce(Return(HIGH));
  EXPECT_EQ(group.getPin().digitalRead(4), HIGH);
  EXPECT_EQ(group.getPin().digitalRead(4), LOW);

  IoPin unset;
  unset.setActiveHigh(false);
  IoGroup unsetGroup(&unset, 1);
  EXPECT_EQ(unsetGroup.getPin().digitalRead(4), LOW);
}

TEST(IoGroupTests, ReadsAndInterruptsUseOnlyFirstMember) {
  StrictMock<SuplaIoMock> io;
  const IoPin pins[] = {IoPin(5, &io), IoPin(6, &io)};
  IoGroup group(pins, 2);
  EXPECT_CALL(io, customDigitalRead(7, 5)).WillOnce(Return(HIGH));
  EXPECT_CALL(io, customAnalogRead(7, 5)).WillOnce(Return(123));
  EXPECT_CALL(io, customPulseIn(7, 5, HIGH, 5000)).WillOnce(Return(42));
  EXPECT_CALL(io, customAttachInterrupt(0, InterruptCallback, RISING));
  EXPECT_CALL(io, customDetachInterrupt(0));
  EXPECT_CALL(io, customPinToInterrupt(5)).WillOnce(Return(9));

  EXPECT_EQ(group.getPin().digitalRead(7), HIGH);
  EXPECT_EQ(Supla::Io::analogRead(7, 0, &group), 123);
  EXPECT_EQ(Supla::Io::pulseIn(7, 0, HIGH, 5000, &group), 42);
  Supla::Io::attachInterrupt(0, InterruptCallback, RISING, &group);
  Supla::Io::detachInterrupt(0, &group);
  EXPECT_EQ(Supla::Io::pinToInterrupt(0, &group), 9);
}

TEST(IoGroupTests, PinToInterruptPreservesBackendMapping3To3And4To5) {
  StrictMock<SuplaIoMock> io;
  const IoPin pin3[] = {IoPin(3, &io)};
  const IoPin pin4[] = {IoPin(4, &io)};
  IoGroup group3(pin3, 1);
  IoGroup group4(pin4, 1);

  EXPECT_CALL(io, customPinToInterrupt(3)).WillOnce(Return(3));
  EXPECT_CALL(io, customPinToInterrupt(4)).WillOnce(Return(5));

  EXPECT_EQ(Supla::Io::pinToInterrupt(0, &group3), 3);
  EXPECT_EQ(Supla::Io::pinToInterrupt(0, &group4), 5);
}

TEST(IoGroupTests, AttachAndDetachUseMappedInterruptInsteadOfPhysicalPin) {
  StrictMock<SuplaIoMock> io;
  const IoPin pins[] = {IoPin(2, &io), IoPin(4, &io)};
  IoGroup group(pins, 2);

  EXPECT_CALL(io, customPinToInterrupt(2)).WillOnce(Return(0));
  EXPECT_CALL(io, customAttachInterrupt(0, InterruptCallback, RISING));
  EXPECT_CALL(io, customDetachInterrupt(0));

  const uint8_t interrupt = Supla::Io::pinToInterrupt(0, &group);
  Supla::Io::attachInterrupt(interrupt, InterruptCallback, RISING, &group);
  Supla::Io::detachInterrupt(interrupt, &group);
}

TEST(IoGroupTests, PwmConfigurationFansOutAndCapabilitiesComeFromFirst) {
  StrictMock<GroupBackendMock> first;
  StrictMock<GroupBackendMock> second;
  const IoPin pins[] = {IoPin(5, &first), IoPin(6, &second)};
  IoGroup group(pins, 2);
  auto output = group.getPin();
  EXPECT_CALL(first, customSetPwmResolutionBits(5, 8));
  EXPECT_CALL(second, customSetPwmResolutionBits(6, 8));
  EXPECT_CALL(first, customSetPwmFrequency(2000));
  EXPECT_CALL(second, customSetPwmFrequency(2000));
  EXPECT_CALL(first, customDefaultPwmResolutionBits(5)).WillOnce(Return(8));
  EXPECT_CALL(first, customCanSetPwmResolutionBits(5)).WillOnce(Return(false));
  EXPECT_CALL(first, customPwmResolutionBits(5))
      .Times(3).WillRepeatedly(Return(8));
  EXPECT_CALL(first, customPwmFrequency()).WillOnce(Return(2000));
  output.setPwmResolutionBits(8);
  output.setPwmFrequency(2000);
  EXPECT_EQ(Supla::Io::defaultPwmResolutionBits(0, &group), 8);
  EXPECT_FALSE(Supla::Io::canSetPwmResolutionBits(0, &group));
  EXPECT_EQ(output.pwmResolutionBits(), 8);
  EXPECT_EQ(output.pwmMaxValue(), 255);
  EXPECT_EQ(group.customPwmMaxValue(0), 255);
  EXPECT_EQ(Supla::Io::pwmFrequency(&group), 2000);
}

TEST(IoGroupTests, ReadinessChecksAllMembersWithoutBlockingWrites) {
  StrictMock<GroupBackendMock> first;
  StrictMock<GroupBackendMock> second;
  const IoPin pins[] = {IoPin(5, &first), IoPin(6, &second)};
  IoGroup group(pins, 2);
  EXPECT_CALL(first, isReady()).Times(2).WillRepeatedly(Return(true));
  EXPECT_CALL(second, isReady()).WillOnce(Return(false)).WillOnce(Return(true));
  EXPECT_FALSE(group.isReady());
  EXPECT_TRUE(group.isReady());
  EXPECT_CALL(first, customDigitalWrite(-1, 5, HIGH));
  EXPECT_CALL(second, customDigitalWrite(-1, 6, HIGH));
  group.getPin().writeActive();
}

TEST(IoGroupTests, EmptyGroupDoesNotAccessAnyPhysicalPin) {
  StrictMock<DigitalInterfaceMock> io;
  const IoPin unused(25);
  IoGroup nullGroup(nullptr, 3);
  IoGroup zeroGroup(&unused, 0);
  for (auto *group : {&nullGroup, &zeroGroup}) {
    EXPECT_FALSE(group->getPin().isSet());
    EXPECT_FALSE(group->isReady());
    group->customPinMode(7, 0, OUTPUT);
    group->customDigitalWrite(7, 0, HIGH);
    group->customAnalogWrite(7, 0, 123);
    group->customConfigureAnalogOutput(7, 0, true);
    group->customSetPwmResolutionBits(0, 10);
    group->customSetPwmFrequency(500);
    group->customAttachInterrupt(0, InterruptCallback, RISING);
    group->customDetachInterrupt(0);
    EXPECT_EQ(group->customDigitalRead(7, 0), 0);
    EXPECT_EQ(group->customAnalogRead(7, 0), 0);
    EXPECT_EQ(group->customPulseIn(7, 0, HIGH, 100), 0);
    EXPECT_EQ(group->customDefaultPwmResolutionBits(0), 0);
    EXPECT_FALSE(group->customCanSetPwmResolutionBits(0));
    EXPECT_EQ(group->customPwmResolutionBits(0), 0);
    EXPECT_EQ(group->customPwmMaxValue(0), 0);
    EXPECT_EQ(group->customPwmFrequency(), 0);
    EXPECT_EQ(group->customPinToInterrupt(0), 0);
  }
}

TEST(IoGroupTests, SingleMemberAndUnsetMemberDoNotRedirectToGpioZero) {
  StrictMock<SuplaIoMock> io;
  StrictMock<DigitalInterfaceMock> gpio;
  const IoPin pins[] = {IoPin(), IoPin(5, &io)};
  IoGroup group(pins, 2);
  EXPECT_FALSE(group.isReady());
  EXPECT_CALL(io, customDigitalWrite(-1, 5, HIGH)).Times(2);
  group.getPin().writeActive();
  EXPECT_EQ(group.getPin().digitalRead(), LOW);
  EXPECT_EQ(group.getPin().pwmMaxValue(), 0);
  IoGroup single(&pins[1], 1);
  EXPECT_CALL(io, customDigitalRead(-1, 5)).WillOnce(Return(HIGH));
  single.getPin().writeActive();
  EXPECT_TRUE(single.getPin().readActive());
}

TEST(IoGroupTests, DimmerWithThreePhysicalOutputsRemainsOneChannel) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ::testing::NiceMock<SuplaIoMock> io;
  const IoPin pins[] = {IoPin(25, &io), IoPin(26, &io), IoPin(27, &io)};
  IoGroup group(pins, 3);
  EXPECT_EQ(Supla::RegisterDevice::getChannelCount(), 0);
  Supla::Control::LightingPwmLeds dimmer(nullptr, group.getPin());
  EXPECT_EQ(Supla::RegisterDevice::getChannelCount(), 1);
  EXPECT_EQ(dimmer.getChannelNumber(), 0);
  EXPECT_EQ(dimmer.getChannel()->getDefaultFunction(), SUPLA_CHANNELFNC_DIMMER);
  EXPECT_EQ(dimmer.getChannel()->getFuncList(), SUPLA_RGBW_BIT_FUNC_DIMMER);
  for (uint8_t pin : {25, 26, 27}) {
    EXPECT_CALL(io, customPinMode(-1, pin, OUTPUT));
    EXPECT_CALL(io, customConfigureAnalogOutput(-1, pin, false));
    EXPECT_CALL(io, customSetPwmResolutionBits(pin, 10));
    EXPECT_CALL(io, customAnalogWrite(-1, pin, 512)).Times(1);
    EXPECT_CALL(io, customAnalogWrite(-1, pin, 0)).Times(1);
  }
  dimmer.onInit();
  uint32_t values[5] = {512, 0, 0, 0, 0};
  dimmer.setRGBCCTValueOnDevice(values, 1);
  dimmer.setRGBCCTValueOnDevice(values, 1);
  values[0] = 0;
  dimmer.setRGBCCTValueOnDevice(values, 1);
}

TEST(IoGroupTests, RgbGroupsOfDifferentSizesAndFunctionReduction) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ::testing::NiceMock<SuplaIoMock> io;
  const IoPin red[] = {IoPin(25, &io), IoPin(26, &io)};
  const IoPin green[] = {IoPin(27, &io)};
  const IoPin blue[] = {IoPin(32, &io), IoPin(33, &io)};
  IoGroup r(red, 2), g(green, 1), b(blue, 2);
  Supla::Control::LightingPwmLeds rgb(
      nullptr, r.getPin(), g.getPin(), b.getPin());
  EXPECT_EQ(Supla::RegisterDevice::getChannelCount(), 1);
  EXPECT_EQ(rgb.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_RGBLIGHTING);
  rgb.onInit();
  EXPECT_CALL(io, customAnalogWrite(-1, 25, 100));
  EXPECT_CALL(io, customAnalogWrite(-1, 26, 100));
  EXPECT_CALL(io, customAnalogWrite(-1, 27, 200));
  EXPECT_CALL(io, customAnalogWrite(-1, 32, 300));
  EXPECT_CALL(io, customAnalogWrite(-1, 33, 300));
  uint32_t values[5] = {100, 200, 300, 0, 0};
  rgb.setRGBCCTValueOnDevice(values, 3);
  EXPECT_CALL(io, customAnalogWrite(-1, 25, 400));
  EXPECT_CALL(io, customAnalogWrite(-1, 26, 400));
  EXPECT_CALL(io, customAnalogWrite(-1, 27, 0));
  EXPECT_CALL(io, customAnalogWrite(-1, 32, 0));
  EXPECT_CALL(io, customAnalogWrite(-1, 33, 0));
  values[0] = 400;
  rgb.setRGBCCTValueOnDevice(values, 1);
}

TEST(IoGroupTests, RelayReadsFirstPinAndFansOutActiveLowState) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ::testing::NiceMock<SuplaIoMock> io;
  const IoPin pins[] = {IoPin(5, &io), IoPin(6, &io)};
  IoGroup group(pins, 2);
  auto output = group.getPin();
  output.setActiveHigh(false);
  Supla::Control::Relay relay(output);
  relay.onInit();
  EXPECT_EQ(Supla::RegisterDevice::getChannelCount(), 1);
  EXPECT_CALL(io, customDigitalWrite(0, 5, LOW));
  EXPECT_CALL(io, customDigitalWrite(0, 6, LOW));
  EXPECT_CALL(io, customDigitalRead(0, 5)).WillRepeatedly(Return(LOW));
  EXPECT_CALL(io, customDigitalRead(0, 6)).Times(0);
  relay.turnOn();
  EXPECT_TRUE(relay.isOn());
  EXPECT_CALL(io, customDigitalWrite(0, 5, HIGH));
  EXPECT_CALL(io, customDigitalWrite(0, 6, HIGH));
  relay.turnOff();
}

TEST(IoGroupTests, RollerShutterTurnsOffWholeOppositeGroupBeforeStarting) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ::testing::NiceMock<SuplaIoMock> io;
  const IoPin up[] = {IoPin(5, &io), IoPin(6, &io)};
  const IoPin down[] = {IoPin(7, &io), IoPin(8, &io)};
  IoGroup upGroup(up, 2), downGroup(down, 2);
  GroupRollerShutter shutter(upGroup.getPin(), downGroup.getPin());
  shutter.onInit();
  EXPECT_EQ(Supla::RegisterDevice::getChannelCount(), 1);
  ::testing::InSequence sequence;
  EXPECT_CALL(io, customDigitalWrite(0, 7, LOW));
  EXPECT_CALL(io, customDigitalWrite(0, 8, LOW));
  EXPECT_CALL(io, customDigitalWrite(0, 5, HIGH));
  EXPECT_CALL(io, customDigitalWrite(0, 6, HIGH));
  EXPECT_CALL(io, customDigitalWrite(0, 5, LOW));
  EXPECT_CALL(io, customDigitalWrite(0, 6, LOW));
  EXPECT_CALL(io, customDigitalWrite(0, 7, HIGH));
  EXPECT_CALL(io, customDigitalWrite(0, 8, HIGH));
  shutter.startOpening();
  shutter.startClosing();
}

TEST(IoGroupTests, RgbCctFadeAndBrightnessLimitsMatchSinglePinOutputs) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  RecordingPwmIo io;
  const IoPin red[] = {IoPin(0, &io), IoPin(1, &io)};
  const IoPin green[] = {IoPin(2, &io)};
  const IoPin blue[] = {IoPin(3, &io), IoPin(4, &io)};
  const IoPin warm[] = {IoPin(5, &io), IoPin(6, &io), IoPin(7, &io)};
  const IoPin cold[] = {IoPin(8, &io), IoPin(9, &io)};
  IoGroup r(red, 2), g(green, 1), b(blue, 2), w(warm, 3), c(cold, 2);
  Supla::Control::LightingPwmLeds grouped(
      nullptr, r.getPin(), g.getPin(), b.getPin(), w.getPin(), c.getPin());
  EXPECT_EQ(Supla::RegisterDevice::getChannelCount(), 1);
  EXPECT_EQ(grouped.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB);
  Supla::Control::LightingPwmLeds reference(
      nullptr, IoPin(10, &io), IoPin(11, &io), IoPin(12, &io),
      IoPin(13, &io), IoPin(14, &io));
  for (auto *light : {&grouped, &reference}) {
    light->setFadeEffectTime(1000);
    light->setBrightnessRatioLimits(0.1f, 0.8f);
    light->setColorBrightnessRatioLimits(0.1f, 0.8f);
    light->onInit();
    light->setRGBCCT(255, 128, 64, 80, 70, 35);
  }
  const int referencePin[] = {10, 10, 11, 12, 12, 13, 13, 13, 14, 14};
  for (int step = 0; step < 150; ++step) {
    time.advance(10);
    grouped.iterateAlways();
    reference.iterateAlways();
    grouped.onFastTimer();
    reference.onFastTimer();
    for (int pin = 0; pin < 10; ++pin) {
      EXPECT_EQ(io.values[pin], io.values[referencePin[pin]])
          << "step=" << step << " pin=" << pin;
    }
  }
  EXPECT_GT(io.writes, 30);
  EXPECT_GT(io.values[0], 0);
  EXPECT_GT(io.values[5], 0);
  EXPECT_GT(io.values[8], 0);
  for (int pin = 0; pin < 10; ++pin) {
    EXPECT_LE(io.values[pin], 204);  // 80% of the fixed 8-bit range.
  }
}
