/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <arduino_mock.h>
#include <config_mock.h>
#include <protocol_layer_mock.h>
#include <simple_time.h>
#include <storage_mock.h>
#include <supla/channel.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <supla/control/relay_roller_shutter_pair.h>
#include <supla/device/register_device.h>
#include <supla/io.h>

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

class RelayRollerShutterPairFixture : public testing::Test {
 public:
  DigitalInterfaceMock ioMock;
  StorageMock storage;
  SimpleTime time;
  int gpio0 = 1;
  int gpio1 = 2;
  int button0Gpio = 10;
  int button1Gpio = 11;

  void SetUp() override {
    Supla::Channel::resetToDefaults();
    EXPECT_CALL(storage, scheduleSave(_, 2000)).WillRepeatedly(Return());
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }

  void expectRelayInitOff() {
    EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(2);
    EXPECT_CALL(ioMock, pinMode(gpio0, OUTPUT));
    EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(2);
    EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  }

  void expectButtonInit() {
    EXPECT_CALL(ioMock, pinMode(button0Gpio, INPUT)).Times(testing::AtLeast(1));
    EXPECT_CALL(ioMock, pinMode(button1Gpio, INPUT)).Times(testing::AtLeast(1));
    EXPECT_CALL(ioMock, digitalRead(button0Gpio)).WillRepeatedly(Return(0));
    EXPECT_CALL(ioMock, digitalRead(button1Gpio)).WillRepeatedly(Return(0));
    EXPECT_CALL(ioMock, digitalRead(gpio0)).WillRepeatedly(Return(0));
    EXPECT_CALL(ioMock, digitalRead(gpio1)).WillRepeatedly(Return(0));
  }

  const int8_t *channelValue(int channelNumber) {
    return Supla::RegisterDevice::getChannelValuePtr(channelNumber);
  }

  TActionTriggerProperties *actionTriggerProperties(
      Supla::Control::ActionTrigger *actionTrigger) {
    return reinterpret_cast<TActionTriggerProperties *>(
        Supla::RegisterDevice::getChannelValuePtr(
            actionTrigger->getChannelNumber()));
  }
};

TEST_F(RelayRollerShutterPairFixture,
       RegistersOneElementAndFindsItByBothChannels) {
  Supla::Channel::setStartingChannelNumber(100);
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);

  EXPECT_EQ(pair.getChannelNumber(), 100);
  EXPECT_EQ(pair.getSecondaryChannelNumber(), 101);
  EXPECT_EQ(Supla::Element::last(), &pair);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(100), &pair);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(101), &pair);
  EXPECT_EQ(pair.getChannelByChannelNumber(100), pair.getChannel());
  EXPECT_EQ(pair.getChannelByChannelNumber(101), pair.getSecondaryChannel());
}

TEST_F(RelayRollerShutterPairFixture,
       ServerValuesInRelayModeControlSeparateOutputs) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  expectRelayInitOff();
  pair.onInit();

  TSD_SuplaChannelNewValue value = {};
  value.ChannelNumber = 0;
  value.value[0] = 1;
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1));
  EXPECT_EQ(pair.handleNewValueFromServer(&value), 1);
  EXPECT_TRUE(pair.getChannel()->getValueBool());

  value.ChannelNumber = 1;
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  EXPECT_EQ(pair.handleNewValueFromServer(&value), 1);
  EXPECT_TRUE(pair.getSecondaryChannel()->getValueBool());
}

TEST_F(RelayRollerShutterPairFixture,
       AttachedButtonsControlRelaysInRelayMode) {
  Supla::Control::Button primaryButton(button0Gpio);
  Supla::Control::Button secondaryButton(button1Gpio);
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  pair.attach(&primaryButton, &secondaryButton);

  expectButtonInit();
  expectRelayInitOff();
  pair.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1));
  primaryButton.runAction(Supla::CONDITIONAL_ON_PRESS);
  EXPECT_TRUE(pair.getChannel()->getValueBool());
  EXPECT_FALSE(pair.getSecondaryChannel()->getValueBool());

  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  secondaryButton.runAction(Supla::CONDITIONAL_ON_PRESS);
  EXPECT_TRUE(pair.getSecondaryChannel()->getValueBool());
}

TEST_F(RelayRollerShutterPairFixture,
       AttachedButtonsControlRollerShutterInRollerMode) {
  Supla::Control::Button primaryButton(button0Gpio);
  Supla::Control::Button secondaryButton(button1Gpio);
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  pair.attach(&primaryButton, &secondaryButton);

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);

  expectButtonInit();
  EXPECT_CALL(ioMock, pinMode(gpio0, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  pair.onInit();

  primaryButton.runAction(Supla::CONDITIONAL_ON_PRESS);

  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1)).Times(testing::AtLeast(1));
  pair.onTimer();
  EXPECT_FALSE(pair.getSecondaryChannel()->getValueBool());
}

TEST_F(RelayRollerShutterPairFixture,
       AttachedButtonsFollowRuntimeFunctionSwitch) {
  Supla::Control::Button primaryButton(button0Gpio);
  Supla::Control::Button secondaryButton(button1Gpio);
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  pair.attach(&primaryButton, &secondaryButton);

  expectButtonInit();
  expectRelayInitOff();
  pair.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1));
  primaryButton.runAction(Supla::CONDITIONAL_ON_PRESS);
  EXPECT_TRUE(pair.getChannel()->getValueBool());

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);

  primaryButton.runAction(Supla::CONDITIONAL_ON_PRESS);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1)).Times(testing::AtLeast(1));
  pair.onTimer();

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  secondaryButton.runAction(Supla::CONDITIONAL_ON_PRESS);
  EXPECT_TRUE(pair.getSecondaryChannel()->getValueBool());
}

TEST_F(RelayRollerShutterPairFixture,
       AttachedActionTriggersFollowRelayModeButtonProfile) {
  Supla::Control::Button primaryButton(button0Gpio);
  Supla::Control::Button secondaryButton(button1Gpio);
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1, true, true);
  Supla::Control::ActionTrigger primaryAt;
  Supla::Control::ActionTrigger secondaryAt;
  primaryAt.setAlwaysUseOnClick1();
  secondaryAt.setAlwaysUseOnClick1();
  pair.attach(&primaryButton, &secondaryButton, &primaryAt, &secondaryAt);

  expectButtonInit();
  expectRelayInitOff();
  pair.onInit();
  primaryAt.onInit();
  secondaryAt.onInit();

  EXPECT_EQ(primaryAt.getChannel()->getActionTriggerCaps(),
            SUPLA_ACTION_CAP_TURN_ON | SUPLA_ACTION_CAP_TURN_OFF |
                SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x5);
  EXPECT_EQ(actionTriggerProperties(&primaryAt)->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);
  EXPECT_EQ(actionTriggerProperties(&secondaryAt)->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);
  EXPECT_EQ(actionTriggerProperties(&primaryAt)->relatedChannelNumber,
            pair.getChannelNumber() + 1);
  EXPECT_EQ(actionTriggerProperties(&secondaryAt)->relatedChannelNumber,
            pair.getSecondaryChannelNumber() + 1);
}

TEST_F(RelayRollerShutterPairFixture,
       AttachedActionTriggersFollowRollerModeButtonProfile) {
  Supla::Control::Button primaryButton(button0Gpio);
  Supla::Control::Button secondaryButton(button1Gpio);
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1, true, true);
  Supla::Control::ActionTrigger primaryAt;
  Supla::Control::ActionTrigger secondaryAt;
  primaryAt.setAlwaysUseOnClick1();
  secondaryAt.setAlwaysUseOnClick1();
  pair.attach(&primaryButton, &secondaryButton, &primaryAt, &secondaryAt);

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);

  expectButtonInit();
  EXPECT_CALL(ioMock, pinMode(gpio0, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  pair.onInit();
  primaryAt.onInit();
  secondaryAt.onInit();

  EXPECT_EQ(actionTriggerProperties(&primaryAt)->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);
  EXPECT_EQ(actionTriggerProperties(&secondaryAt)->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);
  EXPECT_EQ(actionTriggerProperties(&primaryAt)->relatedChannelNumber,
            pair.getChannelNumber() + 1);
  EXPECT_EQ(actionTriggerProperties(&secondaryAt)->relatedChannelNumber,
            pair.getChannelNumber() + 1);
}

TEST_F(RelayRollerShutterPairFixture,
       AttachedActionTriggersRebuildOnRuntimeFunctionSwitch) {
  Supla::Control::Button primaryButton(button0Gpio);
  Supla::Control::Button secondaryButton(button1Gpio);
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1, true, true);
  Supla::Control::ActionTrigger primaryAt;
  Supla::Control::ActionTrigger secondaryAt;
  primaryAt.setAlwaysUseOnClick1();
  secondaryAt.setAlwaysUseOnClick1();
  pair.attach(&primaryButton, &secondaryButton, &primaryAt, &secondaryAt);

  expectButtonInit();
  expectRelayInitOff();
  pair.onInit();
  primaryAt.onInit();
  secondaryAt.onInit();
  ASSERT_EQ(actionTriggerProperties(&secondaryAt)->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);
  ASSERT_EQ(actionTriggerProperties(&secondaryAt)->relatedChannelNumber,
            pair.getSecondaryChannelNumber() + 1);

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  EXPECT_EQ(actionTriggerProperties(&secondaryAt)->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);
  EXPECT_EQ(actionTriggerProperties(&secondaryAt)->relatedChannelNumber,
            pair.getChannelNumber() + 1);

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  EXPECT_EQ(actionTriggerProperties(&secondaryAt)->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);
  EXPECT_EQ(actionTriggerProperties(&secondaryAt)->relatedChannelNumber,
            pair.getSecondaryChannelNumber() + 1);
}

TEST_F(RelayRollerShutterPairFixture,
       AttachedActionTriggersSendChannelValueOnRuntimeFunctionSwitch) {
  ProtocolLayerMock protoMock;
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(protoMock, getChannelConfig(_, _)).Times(AnyNumber());

  Supla::Control::Button primaryButton(button0Gpio);
  Supla::Control::Button secondaryButton(button1Gpio);
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1, true, true);
  Supla::Control::ActionTrigger primaryAt;
  Supla::Control::ActionTrigger secondaryAt;
  primaryAt.setAlwaysUseOnClick1();
  secondaryAt.setAlwaysUseOnClick1();
  pair.attach(&primaryButton, &secondaryButton, &primaryAt, &secondaryAt);

  expectButtonInit();
  expectRelayInitOff();
  pair.onInit();
  primaryAt.onInit();
  secondaryAt.onInit();
  primaryAt.onRegistered(nullptr);
  secondaryAt.onRegistered(nullptr);
  primaryAt.iterateConnected();
  secondaryAt.iterateConnected();
  testing::Mock::VerifyAndClearExpectations(&protoMock);

  TActionTriggerProperties expected = {};
  expected.relatedChannelNumber = pair.getChannelNumber() + 1;
  expected.disablesLocalOperation =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1;
  EXPECT_CALL(protoMock,
              sendChannelValueChanged(secondaryAt.getChannelNumber(),
                                      _,
                                      0,
                                      0))
      .WillOnce([&expected](uint8_t,
                            int8_t *value,
                            unsigned char,
                            uint32_t) {
        EXPECT_EQ(0, memcmp(value, &expected, sizeof(expected)));
      });

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  secondaryAt.iterateConnected();

  testing::Mock::VerifyAndClearExpectations(&protoMock);
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _))
      .Times(AnyNumber());
  primaryAt.iterateConnected();
  secondaryAt.iterateConnected();
  testing::Mock::VerifyAndClearExpectations(&protoMock);

  expected.relatedChannelNumber = pair.getSecondaryChannelNumber() + 1;
  expected.disablesLocalOperation = SUPLA_ACTION_CAP_SHORT_PRESS_x1;
  EXPECT_CALL(protoMock,
              sendChannelValueChanged(secondaryAt.getChannelNumber(),
                                      _,
                                      0,
                                      0))
      .WillOnce([&expected](uint8_t,
                            int8_t *value,
                            unsigned char,
                            uint32_t) {
        EXPECT_EQ(0, memcmp(value, &expected, sizeof(expected)));
      });

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  secondaryAt.iterateConnected();
}

TEST_F(RelayRollerShutterPairFixture,
       SeparatelyAttachedActionTriggerRebuildsOnServerConfigAfterModeSwitch) {
  Supla::Control::Button primaryButton(button0Gpio);
  Supla::Control::Button secondaryButton(button1Gpio);
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1, true, true);
  Supla::Control::ActionTrigger primaryAt;
  primaryAt.setAlwaysUseOnClick1();
  pair.attach(&primaryButton, &secondaryButton);
  primaryAt.attach(primaryButton);
  primaryAt.setRelatedChannel(pair);

  expectButtonInit();
  expectRelayInitOff();
  pair.onInit();
  primaryAt.onInit();
  ASSERT_EQ(actionTriggerProperties(&primaryAt)->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);

  TSD_ChannelConfig config = {};
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger actionTriggerConfig = {};
  actionTriggerConfig.ActiveActions = 0;
  memcpy(config.Config,
         &actionTriggerConfig,
         sizeof(TChannelConfig_ActionTrigger));
  primaryAt.handleChannelConfig(&config);

  EXPECT_EQ(actionTriggerProperties(&primaryAt)->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);
}

TEST_F(RelayRollerShutterPairFixture,
       IterateAlwaysDoesNotResetRelayModeChannelValues) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  expectRelayInitOff();
  pair.onInit();

  TSD_SuplaChannelNewValue value = {};
  value.ChannelNumber = 0;
  value.value[0] = 1;
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1));
  ASSERT_EQ(pair.handleNewValueFromServer(&value), 1);

  value.ChannelNumber = 1;
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  ASSERT_EQ(pair.handleNewValueFromServer(&value), 1);

  pair.iterateAlways();

  EXPECT_TRUE(pair.getChannel()->getValueBool());
  EXPECT_TRUE(pair.getSecondaryChannel()->getValueBool());
}

TEST_F(RelayRollerShutterPairFixture,
       ReportsPrimaryRelayRemainingCountdownTimer) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  expectRelayInitOff();
  pair.onInit();
  time.advance(1);

  TSD_SuplaChannelNewValue value = {};
  value.ChannelNumber = 0;
  value.value[0] = 1;
  value.DurationMS = 5000;
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1));
  ASSERT_EQ(pair.handleNewValueFromServer(&value), 1);

  uint32_t remainingSec = 0;
  ASSERT_TRUE(pair.getRemainingCountdownTimerSec(&remainingSec));
  EXPECT_EQ(5u, remainingSec);

  time.advance(1100);
  ASSERT_TRUE(pair.getRemainingCountdownTimerSec(&remainingSec));
  EXPECT_EQ(4u, remainingSec);

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);

  EXPECT_FALSE(pair.getRemainingCountdownTimerSec(&remainingSec));
  EXPECT_EQ(0u, remainingSec);
}

TEST_F(RelayRollerShutterPairFixture,
       PurgeConfigClearsPrimaryRelaySpecificConfig) {
  ConfigMock config;
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);

  EXPECT_CALL(config, init()).WillRepeatedly(Return(true));
  EXPECT_CALL(config, eraseKey(StrEq("0_fnc"))).WillOnce(Return(true));
  EXPECT_CALL(config, eraseKey(StrEq("0_cfg_chng")))
      .WillOnce(Return(true));
  EXPECT_CALL(config, eraseKey(StrEq("0_oc_thr"))).WillOnce(Return(true));
  EXPECT_CALL(config, eraseKey(StrEq("1_fnc"))).WillOnce(Return(true));
  EXPECT_CALL(config, eraseKey(StrEq("1_cfg_chng"))).WillOnce(Return(true));
  EXPECT_CALL(config, eraseKey(StrEq("1_oc_thr"))).WillOnce(Return(true));
  EXPECT_CALL(config, eraseKey(StrEq("0_rs_cfg"))).WillOnce(Return(true));
  EXPECT_CALL(config, eraseKey(StrEq("0_tilt_cfg"))).WillOnce(Return(true));

  pair.purgeConfig();
}

TEST_F(RelayRollerShutterPairFixture,
       LoadConfigKeepsPrimaryChannelCommonFieldsOwnedByWrapper) {
  ConfigMock config;
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);

  EXPECT_CALL(config, init()).WillRepeatedly(Return(true));

  EXPECT_CALL(config, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config, getInt32(StrEq("1_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config, getUInt8(StrEq("1_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config,
              getBlob(StrEq("0_rs_cfg"),
                      _,
                      sizeof(Supla::Control::RollerShutterConfig)))
      .Times(1)
      .WillOnce(Return(false));

  pair.onLoadConfig(nullptr);
}

TEST_F(RelayRollerShutterPairFixture,
       LoadConfigFacadeBlindFunctionMakesRollerEngineReadTiltConfig) {
  ConfigMock config;
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1, true);
  int32_t primaryFunction = SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND;

  EXPECT_CALL(config, init()).WillRepeatedly(Return(true));
  EXPECT_CALL(config, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(primaryFunction), Return(true)));
  EXPECT_CALL(config, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config, getInt32(StrEq("1_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config, getUInt8(StrEq("1_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config,
              getBlob(StrEq("0_rs_cfg"),
                      _,
                      sizeof(Supla::Control::RollerShutterConfig)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config,
              getBlob(StrEq("0_tilt_cfg"),
                      _,
                      sizeof(Supla::Control::TiltConfig)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));

  pair.onLoadConfig(nullptr);
}

TEST_F(RelayRollerShutterPairFixture,
       LoadConfigRollerFunctionDoesNotMakeRollerEngineReadTiltConfig) {
  ConfigMock config;
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1, true);
  int32_t primaryFunction = SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER;

  EXPECT_CALL(config, init()).WillRepeatedly(Return(true));
  EXPECT_CALL(config, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(primaryFunction), Return(true)));
  EXPECT_CALL(config, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config, getInt32(StrEq("1_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config, getUInt8(StrEq("1_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config,
              getBlob(StrEq("0_rs_cfg"),
                      _,
                      sizeof(Supla::Control::RollerShutterConfig)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config,
              getBlob(StrEq("0_tilt_cfg"),
                      _,
                      sizeof(Supla::Control::TiltConfig)))
      .Times(0);
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));

  pair.onLoadConfig(nullptr);
}

TEST_F(RelayRollerShutterPairFixture,
       LoadConfigStaircaseFunctionMakesRelayEngineFillStaircaseConfig) {
  ConfigMock config;
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  int32_t primaryFunction = SUPLA_CHANNELFNC_STAIRCASETIMER;

  EXPECT_CALL(config, init()).WillRepeatedly(Return(true));
  EXPECT_CALL(config, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(primaryFunction), Return(true)));
  EXPECT_CALL(config, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config, getInt32(StrEq("1_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config, getUInt8(StrEq("1_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(config,
              getBlob(StrEq("0_rs_cfg"),
                      _,
                      sizeof(Supla::Control::RollerShutterConfig)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));

  pair.onLoadConfig(nullptr);

  TChannelConfig_StaircaseTimer channelConfig = {};
  int size = 0;
  pair.fillChannelConfig(&channelConfig, &size, SUPLA_CONFIG_TYPE_DEFAULT);

  EXPECT_EQ(sizeof(TChannelConfig_StaircaseTimer), size);
  EXPECT_EQ(10000u, channelConfig.TimeMS);
}

TEST_F(RelayRollerShutterPairFixture,
       RemotePrimaryFunctionChangeToImpulseSetsDefaultTurnOnDuration) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  expectRelayInitOff();
  pair.onInit();

  TSD_ChannelConfig config = {};
  config.ChannelNumber = 0;
  config.Func = SUPLA_CHANNELFNC_CONTROLLINGTHEGATE;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.handleChannelConfig(&config, false);

  TSD_SuplaChannelNewValue value = {};
  value.ChannelNumber = 0;
  pair.fillSuplaChannelNewValue(&value);

  EXPECT_EQ(500u, value.DurationMS);
}

TEST_F(RelayRollerShutterPairFixture,
       RemotePrimaryFunctionChangeToImpulseDoesNotLatchOutputOn) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  expectRelayInitOff();
  pair.onInit();

  TSD_ChannelConfig config = {};
  config.ChannelNumber = 0;
  config.Func = SUPLA_CHANNELFNC_CONTROLLINGTHEGATE;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.handleChannelConfig(&config, false);

  TSD_SuplaChannelNewValue value = {};
  value.ChannelNumber = 0;
  value.value[0] = 1;
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1));
  ASSERT_EQ(pair.handleNewValueFromServer(&value), 1);

  time.advance(501);
  EXPECT_CALL(ioMock, digitalRead(gpio0)).WillRepeatedly(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  pair.iterateAlways();
  EXPECT_FALSE(pair.getChannel()->getValueBool());
}

TEST_F(RelayRollerShutterPairFixture,
       RemoteRollerFunctionChangeToImpulseSetsDefaultTurnOnDuration) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  EXPECT_CALL(ioMock, pinMode(gpio0, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  pair.onInit();

  TSD_ChannelConfig config = {};
  config.ChannelNumber = 0;
  config.Func = SUPLA_CHANNELFNC_CONTROLLINGTHEGATE;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.handleChannelConfig(&config, false);

  TSD_SuplaChannelNewValue value = {};
  value.ChannelNumber = 0;
  pair.fillSuplaChannelNewValue(&value);

  EXPECT_EQ(500u, value.DurationMS);
  EXPECT_FALSE(pair.getChannel()->getValueBool());
  EXPECT_FALSE(pair.getSecondaryChannel()->getValueBool());
}

TEST_F(RelayRollerShutterPairFixture,
       SwitchingRelayToRollerForcesBothOutputsOffAndDisablesSecondary) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  expectRelayInitOff();
  pair.onInit();

  TSD_SuplaChannelNewValue value = {};
  value.value[0] = 1;
  value.ChannelNumber = 0;
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1));
  pair.handleNewValueFromServer(&value);
  value.ChannelNumber = 1;
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  pair.handleNewValueFromServer(&value);

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);

  EXPECT_TRUE(pair.getSecondaryChannel()->isStateOnlineAndNotAvailable());

  value.ChannelNumber = 1;
  value.value[0] = 1;
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(0);
  EXPECT_EQ(pair.handleNewValueFromServer(&value), 0);
}

TEST_F(RelayRollerShutterPairFixture,
       SwitchingRollerToRelayForcesBothOutputsOffAndEnablesSecondary) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  pair.setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  EXPECT_FALSE(pair.getSecondaryChannel()->isStateOnlineAndNotAvailable());
  EXPECT_FALSE(pair.getChannel()->getValueBool());
  EXPECT_FALSE(pair.getSecondaryChannel()->getValueBool());
}

TEST_F(RelayRollerShutterPairFixture,
       SwitchingRollerToRelayClearsFullChannelPayloads) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  int8_t stalePrimary[SUPLA_CHANNELVALUE_SIZE] = {-1, 2, 3, 4, 5, 6, 7, 8};
  int8_t staleSecondary[SUPLA_CHANNELVALUE_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8};
  int8_t expected[SUPLA_CHANNELVALUE_SIZE] = {};

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  pair.getChannel()->setNewValue(reinterpret_cast<const char *>(stalePrimary));
  pair.getSecondaryChannel()->setNewValue(
      reinterpret_cast<const char *>(staleSecondary));

  pair.setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  EXPECT_EQ(0, memcmp(channelValue(0), expected, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_EQ(0, memcmp(channelValue(1), expected, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(pair.getChannel()->isUpdateReady());
  EXPECT_TRUE(pair.getSecondaryChannel()->isUpdateReady());
}

TEST_F(RelayRollerShutterPairFixture,
       SwitchingRelayToRollerClearsOldPayloadBeforePublishingRsValue) {
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  int8_t stalePrimary[SUPLA_CHANNELVALUE_SIZE] = {0, 2, 3, 4, 5, 6, 7, 8};
  int8_t expected[SUPLA_CHANNELVALUE_SIZE] = {};
  TDSC_RollerShutterValue expectedRs = {};
  expectedRs.position = UNKNOWN_POSITION;
  memcpy(expected, &expectedRs, sizeof(expectedRs));

  pair.getChannel()->setNewValue(reinterpret_cast<const char *>(stalePrimary));

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(testing::AtLeast(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(testing::AtLeast(1));
  pair.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);

  EXPECT_EQ(0, memcmp(channelValue(0), expected, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(pair.getChannel()->isUpdateReady());
}
