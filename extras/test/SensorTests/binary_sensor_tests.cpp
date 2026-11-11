// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla-common/proto.h>
#include <supla/events.h>
#include <supla/device/register_device.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/binary.h>
#include <simple_time.h>
#include <arduino_mock.h>

#include "../doubles/supla_io_mock.h"

class BinaryConfigStub : public Supla::Sensor::BinaryBase {
 public:
  bool getValue() override {
    return false;
  }
};

class BinaryActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

class AdvancingTime : public TimeInterface {
 public:
  uint32_t millis() override {
    const uint32_t result = value;
    value += incrementOnRead;
    return result;
  }

  uint32_t value = 0;
  uint32_t incrementOnRead = 0;
};

TEST(BinarySensorTests, VirtualBinaryValuesTest) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::VirtualBinary sensor;

  EXPECT_EQ(sensor.getValue(), false);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), false);

  sensor.setServerInvertLogic(true);

  EXPECT_EQ(sensor.getValue(), false);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), true);

  sensor.set();

  for (int i = 0; i < 50; ++i) {
    sensor.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(sensor.getValue(), true);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), false);

  sensor.setServerInvertLogic(false);

  EXPECT_EQ(sensor.getValue(), true);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), true);
}

TEST(BinarySensorTests, VirtualBinaryTimeoutClearsLogicalState) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::VirtualBinary sensor;

  sensor.setTimeoutDs(25);
  sensor.onInit();
  sensor.set();

  for (int i = 0; i < 25; ++i) {
    time.advance(100);
    sensor.iterateAlways();
  }

  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  time.advance(100);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->getValueBool());
  EXPECT_FALSE(sensor.getValue());
}

TEST(BinarySensorTests, VirtualBinaryTimeoutRespectsServerInvertLogic) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::VirtualBinary sensor;

  sensor.setTimeoutDs(25);
  sensor.setServerInvertLogic(true);
  sensor.onInit();
  sensor.clear();

  for (int i = 0; i < 25; ++i) {
    time.advance(100);
    sensor.iterateAlways();
  }

  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  time.advance(100);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->getValueBool());
  EXPECT_TRUE(sensor.getValue());
}

TEST(BinarySensorTests, VirtualBinaryCanDisableConfiguredTimeout) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::VirtualBinary sensor;

  sensor.setTimeoutDs(25);
  sensor.setUseConfiguredTimeout(false);
  sensor.onInit();
  sensor.set();

  for (int i = 0; i < 26; ++i) {
    time.advance(100);
    sensor.iterateAlways();
  }

  EXPECT_TRUE(sensor.getChannel()->getValueBool());
  EXPECT_TRUE(sensor.getValue());
}

TEST(BinarySensorTests, VirtualBinaryInitDoesNotRunActions) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  Supla::Sensor::VirtualBinary sensor;

  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_TURN_OFF);
  sensor.addAction(3, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(4, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  sensor.set();

  sensor.onInit();

  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  testing::InSequence sequence;
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_OFF, 2));
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 3));
  EXPECT_CALL(actionHandler,
              handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 4));

  sensor.clear();
  time.advance(101);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->getValueBool());
}

TEST(BinarySensorTests, VirtualBinaryRawTransitionRunsAllActions) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  Supla::Sensor::VirtualBinary sensor;

  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(3, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  sensor.onInit();
  sensor.set();

  testing::InSequence sequence;
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_ON, 1));
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 2));
  EXPECT_CALL(actionHandler,
              handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 3));

  time.advance(101);
  sensor.iterateAlways();
  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(
                sensor.getChannelNumber())[0],
            1);
}

TEST(BinarySensorTests, VirtualBinaryRawTransitionWithInvertRunsTurnOff) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  Supla::Sensor::VirtualBinary sensor;

  sensor.addAction(1, actionHandler, Supla::ON_TURN_OFF);
  sensor.addAction(2, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(3, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  sensor.setServerInvertLogic(true);
  sensor.onInit();
  sensor.set();

  testing::InSequence sequence;
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_OFF, 1));
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 2));
  EXPECT_CALL(actionHandler,
              handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 3));

  time.advance(101);
  sensor.iterateAlways();
  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(
                sensor.getChannelNumber())[0],
            1);
}

TEST(BinarySensorTests, VirtualBinaryInvertChangeRunsActionsWithoutRawChange) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  Supla::Sensor::VirtualBinary sensor;

  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_TURN_OFF);
  sensor.addAction(3, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(4, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  sensor.onInit();

  auto rawValue = Supla::RegisterDevice::getChannelValuePtr(
      sensor.getChannelNumber());
  ASSERT_NE(rawValue, nullptr);
  EXPECT_EQ(rawValue[0], 0);

  {
    testing::InSequence sequence;
    EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_ON, 1));
    EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 3));
    EXPECT_CALL(actionHandler,
                handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 4));
    EXPECT_TRUE(sensor.setServerInvertLogic(true));
  }
  EXPECT_EQ(rawValue[0], 0);
  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  {
    testing::InSequence sequence;
    EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_OFF, 2));
    EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 3));
    EXPECT_CALL(actionHandler,
                handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 4));
    EXPECT_TRUE(sensor.setServerInvertLogic(false));
  }
  EXPECT_EQ(rawValue[0], 0);
  EXPECT_FALSE(sensor.getChannel()->getValueBool());
}

TEST(BinarySensorTests, VirtualBinaryInvertChangeBeforeInitDoesNotRunActions) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  Supla::Sensor::VirtualBinary sensor;

  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_TURN_OFF);
  sensor.addAction(3, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(4, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);

  sensor.onLoadConfig(nullptr);
  EXPECT_TRUE(sensor.setServerInvertLogic(true));
  EXPECT_TRUE(sensor.setServerInvertLogic(false));
  sensor.onInit();
}

TEST(BinarySensorTests, VirtualBinaryStartupSyncRunsOnceWithoutChangeActions) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  Supla::Sensor::VirtualBinary sensor;

  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(3, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  sensor.set();
  sensor.setTurnActionSyncOnStartup();
  sensor.onInit();

  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_ON, 1));
  time.advance(101);
  sensor.iterateAlways();

  time.advance(101);
  sensor.iterateAlways();
}

TEST(BinarySensorTests, VirtualBinaryStartupSyncRunsTurnOff) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  Supla::Sensor::VirtualBinary sensor;

  sensor.addAction(1, actionHandler, Supla::ON_TURN_OFF);
  sensor.setTurnActionSyncOnStartup();
  sensor.onInit();

  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_OFF, 1));
  time.advance(101);
  sensor.iterateAlways();
}

TEST(BinarySensorTests, VirtualBinaryStartupSyncIsCancelledByRawTransition) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  Supla::Sensor::VirtualBinary sensor;

  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(3, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  sensor.setTurnActionSyncOnStartup();
  sensor.onInit();
  sensor.set();

  testing::InSequence sequence;
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_ON, 1));
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 2));
  EXPECT_CALL(actionHandler,
              handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 3));

  time.advance(101);
  sensor.iterateAlways();
  time.advance(101);
  sensor.iterateAlways();
}

TEST(BinarySensorTests, BinaryStartupSyncWaitsForFilteringTime) {
  Supla::Channel::resetToDefaults();
  DigitalInterfaceMock ioMock;
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;

  int gpioValue = 0;
  EXPECT_CALL(ioMock, pinMode(1, INPUT)).WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, digitalRead(1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));

  Supla::Sensor::Binary sensor(1);
  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(3, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  sensor.addAction(4, actionHandler, Supla::ON_TURN_OFF);
  sensor.setFilteringTimeMs(1000);
  sensor.setTurnActionSyncOnStartup();
  sensor.onInit();

  time.advance(101);
  sensor.iterateAlways();

  gpioValue = 1;
  time.advance(500);
  sensor.iterateAlways();

  testing::InSequence sequence;
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_ON, 1));
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 2));
  EXPECT_CALL(actionHandler,
              handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 3));

  time.advance(1000);
  sensor.iterateAlways();
  EXPECT_FALSE(sensor.getChannel()->getValueBool());

  time.advance(101);
  sensor.iterateAlways();
}

TEST(BinarySensorTests, BinaryStartupSyncQuietlyAcceptsInitialFilteredState) {
  Supla::Channel::resetToDefaults();
  DigitalInterfaceMock ioMock;
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;

  int gpioValue = 1;
  EXPECT_CALL(ioMock, pinMode(1, INPUT)).WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, digitalRead(1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));

  Supla::Sensor::Binary sensor(1);
  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(3, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  sensor.setFilteringTimeMs(1000);
  sensor.setTurnActionSyncOnStartup();
  sensor.onInit();

  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  time.advance(500);
  sensor.iterateAlways();

  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_ON, 1));
  time.advance(501);
  sensor.iterateAlways();

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(
                sensor.getChannelNumber())[0],
            1);
  time.advance(101);
  sensor.iterateAlways();
}

TEST(BinarySensorTests, BinaryStartupSyncTimerDoesNotUnderflow) {
  Supla::Channel::resetToDefaults();
  DigitalInterfaceMock ioMock;
  AdvancingTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;

  int gpioValue = 0;
  EXPECT_CALL(ioMock, pinMode(1, INPUT)).WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, digitalRead(1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));

  Supla::Sensor::Binary sensor(1);
  sensor.addAction(1, actionHandler, Supla::ON_TURN_OFF);
  sensor.setFilteringTimeMs(1000);
  sensor.setTurnActionSyncOnStartup();
  sensor.onInit();

  gpioValue = 1;
  time.value = 101;
  time.incrementOnRead = 1;
  sensor.iterateAlways();
}

TEST(BinarySensorTests,
     BinaryStartupSyncWaitsForAcceptedCandidateAcrossClockTick) {
  Supla::Channel::resetToDefaults();
  DigitalInterfaceMock ioMock;
  AdvancingTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  int gpioValue = 1;
  EXPECT_CALL(ioMock, pinMode(1, INPUT)).WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, digitalRead(1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  Supla::Sensor::Binary sensor(1);
  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_TURN_OFF);
  sensor.addAction(3, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(4, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  sensor.setFilteringTimeMs(1000);
  sensor.setTurnActionSyncOnStartup();
  sensor.onInit();
  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  gpioValue = 0;
  time.value = 101;
  sensor.iterateAlways();
  // The filter checks at 1101 (1000 ms elapsed), then startup sync checks
  // at 1102 (1001 ms elapsed). The candidate has not been accepted yet.
  time.value = 1100;
  time.incrementOnRead = 1;
  sensor.iterateAlways();
  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  testing::InSequence sequence;
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_OFF, 2));
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 3));
  EXPECT_CALL(actionHandler,
              handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 4));
  time.value = 1201;
  time.incrementOnRead = 0;
  sensor.iterateAlways();
  EXPECT_FALSE(sensor.getChannel()->getValueBool());
  time.value = 1402;
  sensor.iterateAlways();
}

TEST(BinarySensorTests, BinaryInitDoesNotRunActions) {
  Supla::Channel::resetToDefaults();
  DigitalInterfaceMock ioMock;
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;

  int gpioValue = 1;
  EXPECT_CALL(ioMock, pinMode(1, INPUT)).WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, digitalRead(1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));

  Supla::Sensor::Binary sensor(1);
  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_TURN_OFF);
  sensor.addAction(3, actionHandler, Supla::ON_CHANGE);
  sensor.addAction(4, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);

  sensor.onInit();

  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  testing::InSequence sequence;
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_TURN_OFF, 2));
  EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 3));
  EXPECT_CALL(actionHandler,
              handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 4));

  gpioValue = 0;
  time.advance(101);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->getValueBool());
}

TEST(BinarySensorTests, BinaryConfigStoresLocalAlarmIndicationInChannelConfig) {
  Supla::Channel::resetToDefaults();
  BinaryConfigStub sensor;

  ASSERT_TRUE(sensor.setLocalAlarmIndication(2, false));

  TChannelConfig_BinarySensor channelConfig = {};
  int configSize = 0;

  sensor.fillChannelConfig(&channelConfig,
                           &configSize,
                           SUPLA_CONFIG_TYPE_DEFAULT);

  EXPECT_EQ(configSize, sizeof(TChannelConfig_BinarySensor));
  EXPECT_EQ(channelConfig.LocalAlarmIndication, 2);
}

TEST(BinarySensorTests, BinaryConfigAppliesLocalAlarmIndicationFromServer) {
  Supla::Channel::resetToDefaults();
  BinaryConfigStub sensor;

  ASSERT_TRUE(sensor.setLocalAlarmIndication(2, false));

  TSD_ChannelConfig newConfig = {};
  newConfig.ConfigSize = sizeof(TChannelConfig_BinarySensor);
  newConfig.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;

  auto serverConfig =
      reinterpret_cast<TChannelConfig_BinarySensor *>(newConfig.Config);

  serverConfig->LocalAlarmIndication = 1;
  EXPECT_EQ(sensor.applyChannelConfig(&newConfig, false),
            Supla::ApplyConfigResult::Success);
  EXPECT_EQ(sensor.getLocalAlarmIndication(), 1);

  serverConfig->LocalAlarmIndication = 0;
  EXPECT_EQ(sensor.applyChannelConfig(&newConfig, false),
            Supla::ApplyConfigResult::SetChannelConfigNeeded);
  EXPECT_EQ(sensor.getLocalAlarmIndication(), 1);
}

TEST(BinarySensorTests, BinaryValuesTest) {
  Supla::Channel::resetToDefaults();
  DigitalInterfaceMock ioMock;
  SimpleTime time;

  int gpio1Value = 0;
  EXPECT_CALL(ioMock, pinMode(1, INPUT)).WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, digitalRead(1))
      .WillRepeatedly(::testing::ReturnPointee(&gpio1Value));

  Supla::Sensor::Binary sensor(1);

  sensor.onInit();

  EXPECT_EQ(sensor.getValue(), false);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), false);

  sensor.setServerInvertLogic(true);

  EXPECT_EQ(sensor.getValue(), false);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), true);

  gpio1Value = 1;

  for (int i = 0; i < 50; ++i) {
    sensor.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(sensor.getValue(), true);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), false);

  sensor.setServerInvertLogic(false);

  EXPECT_EQ(sensor.getValue(), true);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), true);
}

TEST(BinarySensorTests, BinaryValuesWithLocalInvertTest) {
  Supla::Channel::resetToDefaults();
  DigitalInterfaceMock ioMock;
  SimpleTime time;

  int gpio1Value = 0;
  EXPECT_CALL(ioMock, pinMode(1, INPUT)).WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, digitalRead(1))
      .WillRepeatedly(::testing::ReturnPointee(&gpio1Value));

  Supla::Sensor::Binary sensor(1, false, true);

  sensor.onInit();

  EXPECT_EQ(sensor.getValue(), true);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), true);

  sensor.setServerInvertLogic(true);

  EXPECT_EQ(sensor.getValue(), true);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), false);

  gpio1Value = 1;

  for (int i = 0; i < 50; ++i) {
    sensor.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(sensor.getValue(), false);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), true);

  sensor.setServerInvertLogic(false);

  EXPECT_EQ(sensor.getValue(), false);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), false);
}

TEST(BinarySensorTests, BinaryIoPinConstructorUsesSeparateIoAndPolarity) {
  Supla::Channel::resetToDefaults();
  SuplaIoMock ioMock;
  SimpleTime time;

  int gpio1Value = 0;
  EXPECT_CALL(ioMock, customPinMode(0, 1, INPUT_PULLUP))
      .WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, customDigitalRead(0, 1))
      .WillRepeatedly(::testing::ReturnPointee(&gpio1Value));

  Supla::Io::IoPin inputPin(1, &ioMock);
  inputPin.setPullUp(true);
  inputPin.setActiveHigh(false);

  Supla::Sensor::Binary sensor(inputPin);

  sensor.onInit();

  EXPECT_EQ(sensor.getValue(), true);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), true);

  gpio1Value = 1;

  for (int i = 0; i < 50; ++i) {
    sensor.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(sensor.getValue(), false);
  EXPECT_EQ(sensor.getChannel()->getValueBool(), false);
}

class BinaryFilteredTests : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    gpioValue = GetParam() ? 1 : 0;
    EXPECT_CALL(ioMock, pinMode(1, INPUT)).WillOnce(::testing::Return());
    EXPECT_CALL(ioMock, digitalRead(1))
        .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  }

  void addActions(Supla::Sensor::Binary& sensor) {
    sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
    sensor.addAction(2, actionHandler, Supla::ON_TURN_OFF);
    sensor.addAction(3, actionHandler, Supla::ON_CHANGE);
    sensor.addAction(4, actionHandler, Supla::ON_SECONDARY_CHANNEL_CHANGE);
  }

  void expectTransition(bool active) {
    testing::InSequence sequence;
    EXPECT_CALL(actionHandler, handleAction(active ? Supla::ON_TURN_ON
                                                   : Supla::ON_TURN_OFF,
                                            active ? 1 : 2));
    EXPECT_CALL(actionHandler, handleAction(Supla::ON_CHANGE, 3));
    EXPECT_CALL(actionHandler,
                handleAction(Supla::ON_SECONDARY_CHANNEL_CHANGE, 4));
  }

  DigitalInterfaceMock ioMock;
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  int gpioValue = 0;
};

TEST_P(BinaryFilteredTests, InitializesChannelFromInputWithoutActions) {
  Supla::Sensor::Binary sensor(1);
  addActions(sensor);
  sensor.setFilteringTimeMs(1000);
  // The first read must also work before onInit().
  EXPECT_EQ(sensor.getValue(), GetParam());
  sensor.onInit();
  EXPECT_EQ(sensor.getValue(), GetParam());
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());
  time.advance(500);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());
  time.advance(501);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());
}

TEST_P(BinaryFilteredTests,
       FiltersShortAndPersistentTransitionsInBothDirections) {
  Supla::Sensor::Binary sensor(1);
  addActions(sensor);
  sensor.setFilteringTimeMs(1000);
  sensor.onInit();

  gpioValue = !GetParam();
  time.advance(101);
  sensor.iterateAlways();
  time.advance(500);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());
  gpioValue = GetParam();
  time.advance(101);
  sensor.iterateAlways();
  time.advance(1001);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());

  gpioValue = !GetParam();
  time.advance(101);
  sensor.iterateAlways();
  time.advance(1000);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());
  expectTransition(!GetParam());
  time.advance(101);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), !GetParam());
}

TEST_P(BinaryFilteredTests, EnablingFilterPreservesLastUnfilteredRead) {
  Supla::Sensor::Binary sensor(1);
  addActions(sensor);
  sensor.onInit();
  gpioValue = !GetParam();
  expectTransition(!GetParam());
  time.advance(101);
  sensor.iterateAlways();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&actionHandler));
  EXPECT_EQ(sensor.getChannel()->getValueBool(), !GetParam());

  sensor.setFilteringTimeMs(1000);
  EXPECT_EQ(sensor.getValue(), !GetParam());
  time.advance(101);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), !GetParam());

  gpioValue = GetParam();
  time.advance(101);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), !GetParam());
  time.advance(500);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), !GetParam());
  expectTransition(GetParam());
  time.advance(501);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());
}

TEST_P(BinaryFilteredTests, StartupSyncWaitsAndRunsOnlyOneTurnAction) {
  Supla::Sensor::Binary sensor(1);
  addActions(sensor);
  sensor.setFilteringTimeMs(1000);
  sensor.setTurnActionSyncOnStartup();
  sensor.onInit();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());
  time.advance(800);
  sensor.iterateAlways();

  EXPECT_CALL(actionHandler, handleAction(GetParam() ? Supla::ON_TURN_ON
                                                    : Supla::ON_TURN_OFF,
                                         GetParam() ? 1 : 2));
  time.advance(200);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());
  time.advance(1001);
  sensor.iterateAlways();
}

TEST_P(BinaryFilteredTests, StartupSyncAcceptsReturnedInitialState) {
  Supla::Sensor::Binary sensor(1);
  addActions(sensor);
  sensor.setFilteringTimeMs(1000);
  sensor.setTurnActionSyncOnStartup();
  sensor.onInit();
  gpioValue = !GetParam();
  time.advance(101);
  sensor.iterateAlways();
  gpioValue = GetParam();
  time.advance(501);
  sensor.iterateAlways();
  time.advance(1000);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());

  EXPECT_CALL(actionHandler, handleAction(GetParam() ? Supla::ON_TURN_ON
                                                    : Supla::ON_TURN_OFF,
                                         GetParam() ? 1 : 2));
  time.advance(101);
  sensor.iterateAlways();
  EXPECT_EQ(sensor.getChannel()->getValueBool(), GetParam());
  time.advance(1001);
  sensor.iterateAlways();
}

INSTANTIATE_TEST_SUITE_P(InputStates, BinaryFilteredTests,
                         ::testing::Bool());

TEST(BinarySensorTests, FilteredStartupRespectsLocalAndServerInversion) {
  Supla::Channel::resetToDefaults();
  DigitalInterfaceMock ioMock;
  SimpleTime time;
  testing::StrictMock<BinaryActionHandlerMock> actionHandler;
  EXPECT_CALL(ioMock, pinMode(1, INPUT)).Times(2);
  EXPECT_CALL(ioMock, digitalRead(1)).WillRepeatedly(::testing::Return(0));
  Supla::Sensor::Binary sensor(1, false, true);
  sensor.addAction(1, actionHandler, Supla::ON_TURN_ON);
  sensor.addAction(2, actionHandler, Supla::ON_TURN_OFF);
  sensor.setFilteringTimeMs(1000);
  sensor.onInit();
  EXPECT_TRUE(sensor.getValue());
  EXPECT_TRUE(sensor.getChannel()->getValueBool());
  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(
                sensor.getChannelNumber())[0], 1);
  sensor.onLoadConfig(nullptr);
  sensor.setServerInvertLogic(true);
  sensor.onInit();
  EXPECT_TRUE(sensor.getValue());
  EXPECT_FALSE(sensor.getChannel()->getValueBool());
  time.advance(1001);
  sensor.iterateAlways();
  EXPECT_FALSE(sensor.getChannel()->getValueBool());
}
