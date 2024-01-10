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

#include <SuplaDevice.h>
#include <arduino_mock.h>
#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mqtt_mock.h>
#include <network_client_mock.h>
#include <srpc_mock.h>
#include <supla/channel.h>
#include <supla/channel_element.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <supla/control/virtual_relay.h>
#include <supla/protocol/supla_srpc.h>
#include <storage_mock.h>
#include "supla/actions.h"
#include "supla/events.h"

using testing::_;
using ::testing::DoAll;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;

class SuplaSrpcStub : public Supla::Protocol::SuplaSrpc {
 public:
  explicit SuplaSrpcStub(SuplaDeviceClass *sdc)
      : Supla::Protocol::SuplaSrpc(sdc) {
  }

  void setRegisteredAndReady() {
    registered = 1;
  }
};

class ActionTriggerTests : public ::testing::Test {
 protected:
  SuplaDeviceClass sd;
  SuplaSrpcStub *suplaSrpc = nullptr;

  virtual void SetUp() {
    new NetworkClientMock;  // it will be destroyed in
                            // Supla::Protocol::SuplaSrpc
    suplaSrpc = new SuplaSrpcStub(&sd);
    suplaSrpc->setRegisteredAndReady();
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }
  virtual void TearDown() {
    delete suplaSrpc;
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
  uint32_t millis() override {
    static uint32_t value = 0;
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
  at.iterateConnected();

  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);

  b1.addAction(Supla::TURN_ON, ah, Supla::ON_CLICK_1);
  b1.addAction(Supla::TURN_ON, ah, Supla::ON_CLICK_3);
  b1.addAction(Supla::TURN_ON, ah, Supla::ON_CLICK_5);
  b1.addAction(Supla::TURN_ON, ah, Supla::ON_HOLD);

  EXPECT_EQ(b1.getMaxMulticlickValue(), 5);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(_, 0)).Times(4);

  EXPECT_TRUE(b1.isMonostable());
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  at.onInit();

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x2 | SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x4 | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result);
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x5);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
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
  at.iterateConnected();
  at.setRelatedChannel(ch1);

  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);

  b1.addAction(Supla::TURN_ON, relay1, Supla::ON_CLICK_1);

  EXPECT_EQ(b1.getMaxMulticlickValue(), 1);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TURN_ON));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x5));

  EXPECT_EQ(b1.getMaxMulticlickValue(), 1);

  EXPECT_TRUE(b1.isBistable());
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  at.onInit();

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_TURN_ON | SUPLA_ACTION_CAP_TURN_OFF |
      SUPLA_ACTION_CAP_TOGGLE_x1 | SUPLA_ACTION_CAP_TOGGLE_x2 |
      SUPLA_ACTION_CAP_TOGGLE_x3 | SUPLA_ACTION_CAP_TOGGLE_x4 |
      SUPLA_ACTION_CAP_TOGGLE_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  EXPECT_EQ(b1.getMaxMulticlickValue(), 1);
  at.handleChannelConfig(&result);
  EXPECT_EQ(b1.getMaxMulticlickValue(), 5);
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 2);
  EXPECT_EQ(propInRegister->disablesLocalOperation, SUPLA_ACTION_CAP_TOGGLE_x1);
}

TEST_F(ActionTriggerTests, AttachToMotionSensorButton) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  // enabling motion sensor button
  b1.setButtonType(Supla::Control::Button::ButtonType::MOTION_SENSOR);
  b1.setMulticlickTime(500);
  Supla::Control::ActionTrigger at;
  Supla::Channel ch1;
  Supla::Control::VirtualRelay relay1(1);

  at.attach(b1);
  at.iterateConnected();
  at.setRelatedChannel(ch1);

  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);

  b1.addAction(Supla::TURN_ON, relay1, Supla::ON_PRESS);
  b1.addAction(Supla::TURN_OFF, relay1, Supla::ON_RELEASE);

  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TURN_ON));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TURN_OFF));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x1)).Times(0);
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x5)).Times(0);

  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);

  EXPECT_TRUE(b1.isMotionSensor());
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  at.onInit();

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_TURN_ON | SUPLA_ACTION_CAP_TURN_OFF |
      SUPLA_ACTION_CAP_TOGGLE_x1 | SUPLA_ACTION_CAP_TOGGLE_x2 |
      SUPLA_ACTION_CAP_TOGGLE_x3 | SUPLA_ACTION_CAP_TOGGLE_x4 |
      SUPLA_ACTION_CAP_TOGGLE_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);
  at.handleChannelConfig(&result);
  // actions toggle x1, x2, x3, x4, x5 are not supported for Motion sensor
  // button type, so they won't be enabled even if such data was send by server
  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 2);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_TURN_ON | SUPLA_ACTION_CAP_TURN_OFF);
}

TEST_F(ActionTriggerTests, SendActionOnce) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;
  Supla::Control::ActionTrigger at2;

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_TURN_ON;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result);

  config.ActiveActions = SUPLA_ACTION_CAP_SHORT_PRESS_x1;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at2.handleChannelConfig(&result);

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TURN_ON));
  EXPECT_CALL(srpc, actionTrigger(1, SUPLA_ACTION_CAP_SHORT_PRESS_x1));

  at.handleAction(0, Supla::SEND_AT_TURN_ON);

  at.iterateConnected();
  at.iterateConnected();
  at.iterateConnected();

  at2.iterateConnected();
  at2.handleAction(0, Supla::SEND_AT_SHORT_PRESS_x1);

  at2.iterateConnected();
  at2.iterateConnected();
}

TEST_F(ActionTriggerTests, SendFewActions) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_TURN_ON;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at.iterateConnected();
  at.handleChannelConfig(&result);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TURN_ON));

  // activated action
  at.handleAction(0, Supla::SEND_AT_TURN_ON);

  // not activated action - should be ignored
  at.handleAction(0, Supla::SEND_AT_SHORT_PRESS_x2);

  at.iterateConnected();
  at.iterateConnected();
  at.iterateConnected();
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

  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0],
            0);

  at.setRelatedChannel(&che4);
  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0],
            5);

  at.setRelatedChannel(&ch0);
  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0],
            1);

  at.setRelatedChannel(ch3);
  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0],
            4);

  at.setRelatedChannel(che1);
  EXPECT_EQ(che1.getChannelNumber(), 1);
  EXPECT_EQ((Supla::Channel::reg_dev.channels)[at.getChannelNumber()].value[0],
            2);
}

TEST_F(ActionTriggerTests, ManageLocalActionsForMonostableButtonOnPress) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD);
  at.attach(b1);

  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();
  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

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
  EXPECT_EQ(b1.getMaxMulticlickValue(), 0);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);
  EXPECT_EQ(b1.getMaxMulticlickValue(), 5);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // local execution
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
}

TEST_F(ActionTriggerTests,
       ManageLocalActionsForMonostableButtonConditionalOnPress) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::CONDITIONAL_ON_PRESS);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD);
  at.attach(b1);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::CONDITIONAL_ON_PRESS, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(Supla::CONDITIONAL_ON_PRESS, Supla::TOGGLE))
      .Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF));
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(2);

  EXPECT_FALSE(b1.isBistable());
  // button actions run before we received channel config from server, so
  // only CONDITIONAL_ON_PRESS and ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::CONDITIONAL_ON_PRESS);
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);  // local execution
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
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

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

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
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_RELEASE);  // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // local execution
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
}

TEST_F(ActionTriggerTests,
       ManageLocalActionsForMonostableButtonConditionalOnRelease) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::CONDITIONAL_ON_RELEASE);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD);
  at.attach(b1);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::CONDITIONAL_ON_RELEASE, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_RELEASE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(Supla::CONDITIONAL_ON_RELEASE, Supla::TOGGLE))
      .Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF));
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(2);

  EXPECT_FALSE(b1.isBistable());
  // button actions run before we received channel config from server, so
  // only ON_PRESS and ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::CONDITIONAL_ON_RELEASE);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_RELEASE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_RELEASE);  // this one should be disabled
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);  // local execution
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_RELEASE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_RELEASE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_RELEASE);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_RELEASE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_RELEASE);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
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

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(Supla::ON_PRESS, Supla::TOGGLE)).Times(5);
  EXPECT_CALL(ah, handleAction(Supla::ON_RELEASE, Supla::TOGGLE)).Times(5);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF));
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(0);

  EXPECT_TRUE(b1.isMonostable());
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
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation, SUPLA_ACTION_CAP_HOLD);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_RELEASE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_RELEASE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
}

TEST_F(ActionTriggerTests, ManageLocalActionsForBistableButton) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  b1.setMulticlickTime(500, true);  // enable bistable button
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_CHANGE);
  b1.addAction(Supla::TURN_ON, ah, Supla::CONDITIONAL_ON_PRESS);
  at.attach(b1);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CHANGE, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::CONDITIONAL_ON_PRESS, false));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

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
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_TOGGLE_x2 | SUPLA_ACTION_CAP_TOGGLE_x3 |
      SUPLA_ACTION_CAP_TOGGLE_x4 | SUPLA_ACTION_CAP_TOGGLE_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_CHANGE);   // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // local execution
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation, SUPLA_ACTION_CAP_TOGGLE_x1 |
                                                    SUPLA_ACTION_CAP_TURN_ON);

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_TOGGLE_x1 | SUPLA_ACTION_CAP_TOGGLE_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_TOGGLE_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
}
TEST_F(ActionTriggerTests,
       ManageLocalActionsForBistableButtonConditionalOnChange) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  b1.setMulticlickTime(500, true);  // enable bistable button
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::CONDITIONAL_ON_CHANGE);
  at.attach(b1);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::CONDITIONAL_ON_CHANGE, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_CHANGE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_TOGGLE_x5));

  EXPECT_CALL(ah, handleAction(Supla::CONDITIONAL_ON_CHANGE, Supla::TOGGLE))
      .Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(2);

  EXPECT_TRUE(b1.isBistable());
  // button actions run before we received channel config from server, so
  // only ON_PRESS and ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::CONDITIONAL_ON_CHANGE);
  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_TOGGLE_x2 | SUPLA_ACTION_CAP_TOGGLE_x3 |
      SUPLA_ACTION_CAP_TOGGLE_x4 | SUPLA_ACTION_CAP_TOGGLE_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_CHANGE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_CHANGE);   // this one should be disabled
  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);  // local execution
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation, SUPLA_ACTION_CAP_TOGGLE_x1);

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_TOGGLE_x1 | SUPLA_ACTION_CAP_TOGGLE_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_CHANGE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_CHANGE);
  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_TOGGLE_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_CHANGE)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(
      b1.getHandlerForFirstClient(Supla::CONDITIONAL_ON_CHANGE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::CONDITIONAL_ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
}

TEST_F(ActionTriggerTests, AlwaysEnabledLocalAction) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD, true);  // always enabled
  at.attach(b1);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(Supla::ON_PRESS, Supla::TOGGLE)).Times(1);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF)).Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(1);

  EXPECT_TRUE(b1.isMonostable());
  // button actions run before we received channel config from server, so
  // only ON_PRESS and ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // local execution
  b1.runAction(Supla::ON_HOLD);  // should be executed anyway, because it can't
                                 // be disabled
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);
}

TEST_F(ActionTriggerTests, RemoveSomeActionsFromATAttachWithStorage) {
  SrpcMock srpc;
  StorageMock storage;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  storage.defaultInitialization(4);
  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD, true);  // always enabled
  at.attach(b1);
  at.enableStateStorage();
  at.disableATCapability(SUPLA_ACTION_CAP_TURN_ON);
  at.disableATCapability(SUPLA_ACTION_CAP_TURN_OFF);
  at.disableATCapability(SUPLA_ACTION_CAP_HOLD);
  at.disableATCapability(SUPLA_ACTION_CAP_SHORT_PRESS_x2);
  at.disableATCapability(SUPLA_ACTION_CAP_SHORT_PRESS_x4);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));

  EXPECT_CALL(storage, scheduleSave(2000));

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  // onLoadState expectations
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .Times(2)
      .WillRepeatedly(
          [](uint32_t address, unsigned char *data, int size, bool) {
          uint32_t storageData = 0;
          memcpy(data, &storageData, sizeof(storageData));
          return sizeof(storageData);
          });

  // on init call is executed in SuplaDevice.setup()
  at.onLoadConfig(nullptr);
  Supla::Storage::LoadStateStorage();
  at.onInit();
  Supla::Storage::WriteStateStorage();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(Supla::ON_PRESS, Supla::TOGGLE)).Times(1);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF)).Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(1);

  EXPECT_TRUE(b1.isMonostable());
  // button actions run before we received channel config from server, so
  // only ON_PRESS and ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // local execution
  b1.runAction(Supla::ON_HOLD);  // should be executed anyway, because it can't
                                 // be disabled
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  EXPECT_EQ(Supla::Channel::reg_dev.channels[at.getChannelNumber()].FuncList,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1 | SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x5);
}

TEST_F(ActionTriggerTests, ManageLocalActionsForMonostableButtonWithCfg) {
  SrpcMock srpc;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;
  SuplaDeviceClass sd;

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD);
  at.attach(b1);
  b1.configureAsConfigButton(&sd);

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_NE(b1.getHandlerForClient(&sd, Supla::ON_CLICK_1), nullptr);
  EXPECT_EQ(b1.getHandlerForClient(&ah, Supla::ON_CLICK_1), nullptr);
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));

  // on init call is executed in SuplaDevice.setup()
  at.onInit();

  EXPECT_NE(b1.getHandlerForClient(&sd, Supla::ON_CLICK_1), nullptr);
  EXPECT_NE(b1.getHandlerForClient(&ah, Supla::ON_CLICK_1), nullptr);
  EXPECT_TRUE(b1.getHandlerForClient(&ah, Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForClient(&ah, Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(ah, handleAction(Supla::ON_PRESS, Supla::TOGGLE)).Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF));
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(2);

  EXPECT_TRUE(b1.isMonostable());
  // button actions run before we received channel config from server, so
  // only ON_PRESS and ON_HOLD should be executed locally.
  // Other actions will be ignored
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // local execution
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForClient(&ah, Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForClient(&ah, Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForClient(&ah, Supla::ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForClient(&ah, Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  // another config from server which disables all actions
  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  EXPECT_TRUE(b1.getHandlerForClient(&ah, Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForClient(&ah, Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
}

TEST_F(ActionTriggerTests, ActionHandlingType_PublishAllDisableAllTest) {
  SrpcMock srpc;
  StorageMock storage;
  ConfigMock cfg;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  EXPECT_CALL(cfg, getInt32(_, _)).WillOnce([](const char *key, int32_t *buf) {
    if (strcmp(key, "0_mqtt_at") == 0) {
      *buf = 2;
      return true;
    }
    EXPECT_TRUE(false);
    return false;
  });

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD, true);  // always enabled
  at.attach(b1);
  at.enableStateStorage();
  at.disableATCapability(SUPLA_ACTION_CAP_TURN_ON);
  at.disableATCapability(SUPLA_ACTION_CAP_TURN_OFF);
  at.disableATCapability(SUPLA_ACTION_CAP_HOLD);
  at.disableATCapability(SUPLA_ACTION_CAP_SHORT_PRESS_x2);
  at.disableATCapability(SUPLA_ACTION_CAP_SHORT_PRESS_x4);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));

  EXPECT_CALL(storage, scheduleSave(2000));

  storage.defaultInitialization();

  // onLoadState expectations
  uint32_t storedActionsFromServer = 0;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillOnce(DoAll(SetArgPointee<1>(storedActionsFromServer), Return(4)));

  // onSaveState expectations
  uint32_t actionsFromServerToBeSaved = 0xFFFFFFFF;
  EXPECT_CALL(storage, writeStorage(_, Pointee(actionsFromServerToBeSaved), 4));

  // on init call is executed in SuplaDevice.setup()
  at.onLoadConfig(nullptr);
  at.onLoadState();
  at.onInit();
  at.onSaveState();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5)).Times(2);
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1)).Times(2);

  EXPECT_CALL(ah, handleAction(Supla::ON_PRESS, Supla::TOGGLE)).Times(0);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF)).Times(2);
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(0);

  EXPECT_TRUE(b1.isMonostable());
  // button actions run before we received channel config from server, so
  // only ON_HOLD should be executed locally, because all actions are disabled
  // expect for those which can't be disabled.
  b1.runAction(Supla::ON_PRESS);    // not published
  b1.runAction(Supla::ON_CLICK_1);  // published
  b1.runAction(Supla::ON_HOLD);     // local handler
  b1.runAction(Supla::ON_CLICK_6);  // ON_CLICK_6 is not published because
                                    // we only have AT defined up to 5x
  b1.runAction(Supla::ON_CLICK_5);  // published

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  // however actionHandlerType is set to disable all local actions
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // published
  b1.runAction(Supla::ON_HOLD);  // should be executed anyway, because it can't
                                 // be disabled
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);  // published

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  EXPECT_EQ(Supla::Channel::reg_dev.channels[at.getChannelNumber()].FuncList,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1 | SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x5);
}

TEST_F(ActionTriggerTests, ActionHandlingType_PublishAllDisableNoneTest) {
  SrpcMock srpc;
  StorageMock storage;
  ConfigMock cfg;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  EXPECT_CALL(cfg, getInt32(_, _)).WillOnce([](const char *key, int32_t *buf) {
    if (strcmp(key, "0_mqtt_at") == 0) {
      *buf = 1;
      return true;
    }
    EXPECT_TRUE(false);
    return false;
  });

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  b1.addAction(Supla::TURN_OFF, ah, Supla::ON_HOLD, true);  // always enabled
  at.attach(b1);
  at.enableStateStorage();
  at.disableATCapability(SUPLA_ACTION_CAP_TURN_ON);
  at.disableATCapability(SUPLA_ACTION_CAP_TURN_OFF);
  at.disableATCapability(SUPLA_ACTION_CAP_HOLD);
  at.disableATCapability(SUPLA_ACTION_CAP_SHORT_PRESS_x2);

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));

  storage.defaultInitialization(4);

  EXPECT_CALL(storage, scheduleSave(2000)).Times(2);
  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  // onLoadState expectations
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .Times(2)
      .WillRepeatedly(
          [](uint32_t address, unsigned char *data, int size, bool) {
          uint32_t storageData = 0;
          memcpy(data, &storageData, sizeof(storageData));
          return sizeof(storageData);
          });

  // on init call is executed in SuplaDevice.setup()
  at.onLoadConfig(nullptr);
  Supla::Storage::LoadStateStorage();
  at.onInit();
  Supla::Storage::WriteStateStorage();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5)).Times(3);
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x3)).Times(1);
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x4)).Times(2);
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1)).Times(3);

  //  EXPECT_CALL(ah, handleAction(Supla::ON_PRESS, Supla::TOGGLE)).Times(1);
  EXPECT_CALL(ah, handleAction(Supla::ON_HOLD, Supla::TURN_OFF)).Times(3);
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(2);

  EXPECT_TRUE(b1.isMonostable());
  // button actions run before we received channel config from server, so
  // only ON_HOLD should be executed locally, because all actions are disabled
  // expect for those which can't be disabled.
  b1.runAction(Supla::ON_PRESS);    // not published, local action disabled
  b1.runAction(Supla::ON_CLICK_1);  // published, local action run
  b1.runAction(Supla::ON_HOLD);     // local handler
  b1.runAction(Supla::ON_CLICK_6);  // ON_CLICK_6 is not published because
                                    // we only have AT defined up to 5x
  b1.runAction(Supla::ON_CLICK_5);  // published

  for (int i = 0; i < 10; i++) {
    at.iterateConnected(nullptr);
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  // however actionHandlerType is set to disable all local actions
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // published, local action run
  b1.runAction(Supla::ON_HOLD);  // should be executed anyway, because it can't
                                 // be disabled
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);  // published
  b1.runAction(Supla::ON_CLICK_4);  // published

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  memset(&result, 0, sizeof(result));
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  memset(&config, 0, sizeof(config));
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x2 | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with SHORT_PRESS_x1 used, so
  // ON_CLICK_1 is disabled locally and published to servers
  at.handleChannelConfig(&result);

  b1.runAction(Supla::ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // published, local action disabled
  b1.runAction(Supla::ON_HOLD);     // local action run
  b1.runAction(Supla::ON_CLICK_3);  // published
  b1.runAction(Supla::ON_CLICK_4);  // published
  b1.runAction(Supla::ON_CLICK_5);  // published
  b1.runAction(Supla::ON_CLICK_6);  // not published

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
  ////

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  EXPECT_EQ(Supla::Channel::reg_dev.channels[at.getChannelNumber()].FuncList,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1 | SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x5);
}

TEST_F(ActionTriggerTests, ActionHandlingType_RelayOnSuplaServerTest) {
  SrpcMock srpc;
  StorageMock storage;
  ConfigMock cfg;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  EXPECT_CALL(cfg, getInt32(_, _)).WillOnce([](const char *key, int32_t *buf) {
    if (strcmp(key, "0_mqtt_at") == 0) {
      *buf = 0;
      return true;
    }
    EXPECT_TRUE(false);
    return false;
  });

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  at.attach(b1);
  at.enableStateStorage();

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));

  EXPECT_CALL(storage, scheduleSave(2000)).Times(2);
  storage.defaultInitialization(4);

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  // onLoadState expectations
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .Times(2)
      .WillRepeatedly(
          [](uint32_t address, unsigned char *data, int size, bool) {
          uint32_t storageData = 0;
          memcpy(data, &storageData, sizeof(storageData));
          return sizeof(storageData);
          });

  // on init call is executed in SuplaDevice.setup()
  at.onLoadConfig(nullptr);
  Supla::Storage::LoadStateStorage();
  at.onInit();
  Supla::Storage::WriteStateStorage();

  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  at.iterateConnected();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x4)).Times(1);
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5)).Times(2);
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1)).Times(1);
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD)).Times(2);

  EXPECT_CALL(ah, handleAction(Supla::ON_PRESS, Supla::TOGGLE)).Times(1);
  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TOGGLE)).Times(1);

  EXPECT_TRUE(b1.isMonostable());
  // button actions run before we received channel config from server, so
  // only ON_HOLD should be executed locally, because all actions are disabled
  // expect for those which can't be disabled.
  b1.runAction(Supla::ON_PRESS);    // not published, local action run
  b1.runAction(Supla::ON_CLICK_1);  // not published
  b1.runAction(Supla::ON_HOLD);     // not published
  b1.runAction(Supla::ON_CLICK_6);  // ON_CLICK_6 is not published because
                                    // we only have AT defined up to 5x
  b1.runAction(Supla::ON_CLICK_5);  // not published

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x4 | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with no SHORT_PRESS_x1 used, so
  // ON_CLICK_1 should be executed on local ah element
  at.handleChannelConfig(&result);

  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_PRESS)->isEnabled());
  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // not published, local action run
  b1.runAction(Supla::ON_HOLD);     // published
  b1.runAction(Supla::ON_CLICK_3);  // not published
  b1.runAction(Supla::ON_CLICK_4);  // published
  b1.runAction(Supla::ON_CLICK_5);  // published
  b1.runAction(Supla::ON_CLICK_6);  // not published

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  memset(&result, 0, sizeof(result));
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  memset(&config, 0, sizeof(config));
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x2 | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  // we received channel config with SHORT_PRESS_x1 used, so
  // ON_CLICK_1 is disabled locally and published to servers
  at.handleChannelConfig(&result);

  b1.runAction(Supla::ON_PRESS);    // this one should be disabled
  b1.runAction(Supla::ON_CLICK_1);  // published, local action disabled
  b1.runAction(Supla::ON_HOLD);     // published
  b1.runAction(Supla::ON_CLICK_3);  // not published
  b1.runAction(Supla::ON_CLICK_4);  // not published
  b1.runAction(Supla::ON_CLICK_5);  // published
  b1.runAction(Supla::ON_CLICK_6);  // not published

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
}

TEST_F(ActionTriggerTests, MqttSendAtTest) {
  SrpcMock srpc;
  MqttMock mqtt(&sd);
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;
  mqtt.onInit();
  mqtt.setRegisteredAndReady();

  at.attach(b1);
  at.iterateConnected();

  b1.addAction(Supla::TURN_ON, ah, Supla::ON_CLICK_1);
  b1.addAction(Supla::TURN_ON, ah, Supla::ON_CLICK_3);
  b1.addAction(Supla::TURN_ON, ah, Supla::ON_CLICK_5);
  b1.addAction(Supla::TURN_ON, ah, Supla::ON_HOLD);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x5));

  EXPECT_CALL(
      mqtt,
      publishTest("supla/devices/supla-device/channels/0/button_short_press",
                  "button_short_press",
                  0,
                  false));

  EXPECT_CALL(
      mqtt,
      publishTest("supla/devices/supla-device/channels/0/button_long_press",
                  "button_long_press",
                  0,
                  false));

  EXPECT_CALL(
      mqtt,
      publishTest(
          "supla/devices/supla-device/channels/0/button_quintuple_press",
          "button_quintuple_press",
          0,
          false));

  EXPECT_CALL(ah, handleAction(_, 0)).Times(4);

  EXPECT_TRUE(b1.isMonostable());
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  at.onInit();

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x2 | SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x4 | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result);
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::Channel::reg_dev.channels[at.getChannelNumber()].value);

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
                SUPLA_ACTION_CAP_SHORT_PRESS_x5);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result);

  // it should be executed on ah mock
  b1.runAction(Supla::ON_CLICK_1);
}

