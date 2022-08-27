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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <srpc_mock.h>
#include <arduino_mock.h>
#include <supla/channel.h>
#include <supla/channel_element.h>
#include <supla/control/virtual_relay.h>

using testing::ElementsAreArray;
using testing::_;

class ActionTriggerTests : public ::testing::Test {
  protected:
    virtual void SetUp() {
      Supla::Channel::lastCommunicationTimeMs = 0;
      memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
    }
    virtual void TearDown() {
      Supla::Channel::lastCommunicationTimeMs = 0;
      memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
    }

};

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

class TimeInterfaceStub : public TimeInterface {
  public:
    virtual uint64_t millis() override {
      static uint64_t value = 0;
      value += 1000;
      return value;
    }
};

TEST_F(ActionTriggerTests, AttachToMonostableButton) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  at.attach(b1);
  at.iterateConnected(0);

  b1.addAction(Supla::TURN_ON, ah, Supla::ON_CLICK_1);
  b1.addAction(Supla::TURN_ON, ah, Supla::ON_CLICK_3);
  b1.addAction(Supla::TURN_ON, ah, Supla::ON_CLICK_5);
  b1.addAction(Supla::TURN_ON, ah, Supla::ON_HOLD);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(_, 0)).Times(4);

  EXPECT_FALSE(b1.isBistable());
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  at.onInit();

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TSD_ChannelConfig_ActionTrigger);
  TSD_ChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x1
    | SUPLA_ACTION_CAP_SHORT_PRESS_x2
    | SUPLA_ACTION_CAP_SHORT_PRESS_x3
    | SUPLA_ACTION_CAP_SHORT_PRESS_x4
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result);
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TActionTriggerProperties *propInRegister =
    reinterpret_cast<TActionTriggerProperties *>
    (Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
      SUPLA_ACTION_CAP_HOLD
      | SUPLA_ACTION_CAP_SHORT_PRESS_x1
      | SUPLA_ACTION_CAP_SHORT_PRESS_x3
      | SUPLA_ACTION_CAP_SHORT_PRESS_x5
      );

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x2
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  // it should be executed on ah mock
  b1.runAction(Supla::ON_CLICK_1);
}

TEST_F(ActionTriggerTests, AttachToBistableButton) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  // enabling bistable button
  b1.setMulticlickTime(500, true);
  Supla::Control::ActionTrigger at;
  Supla::Channel ch1;
  Supla::Control::VirtualRelay relay1(1);

  at.attach(b1);
  at.iterateConnected(0);
  at.setRelatedChannel(ch1);

  b1.addAction(Supla::TURN_ON, relay1, Supla::ON_CLICK_1);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TURN_ON));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x5));

  EXPECT_TRUE(b1.isBistable());
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  at.onInit();

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TSD_ChannelConfig_ActionTrigger);
  TSD_ChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_TURN_ON
    | SUPLA_ACTION_CAP_TURN_OFF
    | SUPLA_ACTION_CAP_TOGGLE_x1
    | SUPLA_ACTION_CAP_TOGGLE_x2
    | SUPLA_ACTION_CAP_TOGGLE_x3
    | SUPLA_ACTION_CAP_TOGGLE_x4
    | SUPLA_ACTION_CAP_TOGGLE_x5;

  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result);
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TActionTriggerProperties *propInRegister =
    reinterpret_cast<TActionTriggerProperties *>
    (Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 2);
  EXPECT_EQ(propInRegister->disablesLocalOperation, SUPLA_ACTION_CAP_TOGGLE_x1);

}

TEST_F(ActionTriggerTests, SendActionOnce) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;
  Supla::Control::ActionTrigger at2;

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TSD_ChannelConfig_ActionTrigger);
  TSD_ChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_TURN_ON;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result);

  config.ActiveActions = SUPLA_ACTION_CAP_SHORT_PRESS_x1;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));

  at2.handleChannelConfig(&result);

  at.iterateConnected(0);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TURN_ON));
  EXPECT_CALL(srpc, actionTrigger(1, SUPLA_ACTION_CAP_SHORT_PRESS_x1));

  at.handleAction(0, Supla::SEND_AT_TURN_ON);

  at.iterateConnected(0);
  at.iterateConnected(0);
  at.iterateConnected(0);

  at2.iterateConnected(0);
  at2.handleAction(0, Supla::SEND_AT_SHORT_PRESS_x1);

  at2.iterateConnected(0);
  at2.iterateConnected(0);
}

TEST_F(ActionTriggerTests, SendFewActions) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TSD_ChannelConfig_ActionTrigger);
  TSD_ChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_TURN_ON;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));

  at.iterateConnected(0);
  at.handleChannelConfig(&result);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TURN_ON));

  // activated action
  at.handleAction(0, Supla::SEND_AT_TURN_ON);

  // not activated action - should be ignored
  at.handleAction(0, Supla::SEND_AT_SHORT_PRESS_x2);

  at.iterateConnected(0);
  at.iterateConnected(0);
  at.iterateConnected(0);
}

TEST_F(ActionTriggerTests, ActionsShouldAddCaps) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;
  Supla::Control::Button button(10, false, false);

  EXPECT_EQ(at.getChannel()->getActionTriggerCaps(), 0);

  button.addAction(Supla::SEND_AT_HOLD, at, Supla::ON_PRESS);
  EXPECT_EQ(at.getChannel()->getActionTriggerCaps(), SUPLA_ACTION_CAP_HOLD);

  button.addAction(Supla::SEND_AT_TOGGLE_x2, at, Supla::ON_PRESS);
  EXPECT_EQ(at.getChannel()->getActionTriggerCaps(),
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_TOGGLE_x2);

  button.addAction(Supla::SEND_AT_SHORT_PRESS_x5, at, Supla::ON_PRESS);
  EXPECT_EQ(at.getChannel()->getActionTriggerCaps(),
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_TOGGLE_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5);
}

TEST_F(ActionTriggerTests, RelatedChannel) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Channel ch0;
  Supla::ChannelElement che1;
  Supla::Channel ch2;
  Supla::Channel ch3;
  Supla::ChannelElement che4;
  Supla::Control::ActionTrigger at;

  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0], 0);

  at.setRelatedChannel(&che4);
  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0], 5);

  at.setRelatedChannel(&ch0);
  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0], 1);

  at.setRelatedChannel(ch3);
  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0], 4);

  at.setRelatedChannel(che1);
  EXPECT_EQ(che1.getChannelNumber(), 1);
  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0], 2);

}

TEST_F(ActionTriggerTests, ManageLocalActionsForMonostableButton) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD);
  at.attach(b1);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->enabled);
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  at.iterateConnected(0);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(Supla::ON_PRESS, Supla::TOGGLE)).Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF));
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(2);

  EXPECT_FALSE(b1.isBistable());
  // button actions run before we received channel config from server, so
  // only ON_PRESS and ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TSD_ChannelConfig_ActionTrigger);
  TSD_ChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x2
    | SUPLA_ACTION_CAP_SHORT_PRESS_x3
    | SUPLA_ACTION_CAP_SHORT_PRESS_x4
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_PRESS);   // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1); // local execution
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TActionTriggerProperties *propInRegister =
    reinterpret_cast<TActionTriggerProperties *>
    (Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
      SUPLA_ACTION_CAP_HOLD
      | SUPLA_ACTION_CAP_SHORT_PRESS_x1
      );

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x1
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->enabled);
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->enabled);
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }
}

TEST_F(ActionTriggerTests, ManageLocalActionsForMonostableButtonOnRelease) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_RELEASE);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD);
  at.attach(b1);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_RELEASE));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_PRESS));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  at.iterateConnected(0);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(Supla::ON_RELEASE, Supla::TOGGLE)).Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF));
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(2);

  EXPECT_FALSE(b1.isBistable());
  // button actions run before we received channel config from server, so
  // only ON_PRESS and ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TSD_ChannelConfig_ActionTrigger);
  TSD_ChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x2
    | SUPLA_ACTION_CAP_SHORT_PRESS_x3
    | SUPLA_ACTION_CAP_SHORT_PRESS_x4
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_RELEASE);   // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1); // local execution
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TActionTriggerProperties *propInRegister =
    reinterpret_cast<TActionTriggerProperties *>
    (Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
      SUPLA_ACTION_CAP_HOLD
      | SUPLA_ACTION_CAP_SHORT_PRESS_x1
      );

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x1
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }
}

TEST_F(ActionTriggerTests,
    ManageLocalActionsForMonostableButtonOnReleaseAndOnPress) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_RELEASE);
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD);
  at.attach(b1);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_RELEASE));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->enabled);

  at.iterateConnected(0);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(Supla::ON_PRESS, Supla::TOGGLE)).Times(5);
  EXPECT_CALL(ah, handleAction(Supla::ON_RELEASE, Supla::TOGGLE)).Times(5);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF));
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(0);

  EXPECT_FALSE(b1.isBistable());
  // button actions run before we received channel config from server, so
  // only ON_PRESS, ON_RELEASE, ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TSD_ChannelConfig_ActionTrigger);
  TSD_ChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x2
    | SUPLA_ACTION_CAP_SHORT_PRESS_x3
    | SUPLA_ACTION_CAP_SHORT_PRESS_x4
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TActionTriggerProperties *propInRegister =
    reinterpret_cast<TActionTriggerProperties *>
    (Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
      SUPLA_ACTION_CAP_HOLD
      );

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x1
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->enabled);

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD
    | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->enabled);

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->enabled);

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }
}

TEST_F(ActionTriggerTests, ManageLocalActionsForBistableButton) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  b1.setMulticlickTime(500, true); // enable bistable button
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_CHANGE);
  at.attach(b1);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CHANGE));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_PRESS));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->enabled);
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  at.iterateConnected(0);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x5));

  EXPECT_CALL(ah, handleAction(Supla::ON_CHANGE, Supla::TOGGLE)).Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(2);

  EXPECT_TRUE(b1.isBistable());
  // button actions run before we received channel config from server, so
  // only ON_PRESS and ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TSD_ChannelConfig_ActionTrigger);
  TSD_ChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_TOGGLE_x2
    | SUPLA_ACTION_CAP_TOGGLE_x3
    | SUPLA_ACTION_CAP_TOGGLE_x4
    | SUPLA_ACTION_CAP_TOGGLE_x5;

  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_CHANGE);   // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1); // local execution
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  TActionTriggerProperties *propInRegister =
    reinterpret_cast<TActionTriggerProperties *>
    (Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
      SUPLA_ACTION_CAP_TOGGLE_x1
      );

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_TOGGLE_x1
    | SUPLA_ACTION_CAP_TOGGLE_x5;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->enabled);
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_TOGGLE_x5;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->enabled);
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TSD_ChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->enabled);
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->enabled);

  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(0);
  }
}
