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
#include <supla/channel_element.h>
#include <supla/events.h>
#include <supla/actions.h>
#include <supla/action_handler.h>
#include <supla/condition.h>
#include <supla_srpc_layer_mock.h>
#include <simple_time.h>
#include "supla/element_with_channel_actions.h"

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};


TEST(ChannelElementTests, ActionDelegationToChannel) {
  Supla::Channel::resetToDefaults();
  ASSERT_EQ(Supla::LocalAction::getClientListPtr(), nullptr);
  Supla::ChannelElement element;

  ActionHandlerMock mock1;
  ActionHandlerMock mock2;

  int action1 = 11;

  EXPECT_CALL(mock1, handleAction(Supla::ON_TURN_ON, action1)).Times(2);
  EXPECT_CALL(mock2, handleAction(Supla::ON_TURN_OFF, action1)).Times(2);

  EXPECT_FALSE(element.isEventAlreadyUsed(Supla::ON_TURN_ON, false));
  EXPECT_FALSE(element.isEventAlreadyUsed(Supla::ON_TURN_OFF, false));

  element.addAction(action1, mock1, Supla::ON_TURN_ON);
  element.addAction(action1, &mock2, Supla::ON_TURN_OFF);

  EXPECT_TRUE(element.isEventAlreadyUsed(Supla::ON_TURN_ON, false));
  EXPECT_TRUE(element.isEventAlreadyUsed(Supla::ON_TURN_OFF, false));

  element.getChannel()->setNewValue(false);
  element.getChannel()->setNewValue(true);
  element.getChannel()->setNewValue(true);

  element.getChannel()->setNewValue(false);
  element.getChannel()->setNewValue(true);
  element.getChannel()->setNewValue(false);
  element.getChannel()->setNewValue(false);
}

TEST(ChannelElementTests, ActionDelegationToConditions) {
  Supla::Channel::resetToDefaults();
  ASSERT_EQ(Supla::LocalAction::getClientListPtr(), nullptr);

  ActionHandlerMock mock1;
  ActionHandlerMock mock2;

  int action1 = 11;

  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, action1)).Times(1);
  EXPECT_CALL(mock2, handleAction(Supla::ON_CHANGE, action1)).Times(1);

  Supla::ChannelElement element;
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_DISTANCESENSOR);

  element.addAction(action1, mock1, OnLess(50));
  element.addAction(action1, &mock2, OnLess(30));

  channel->setNewValue(60);
  channel->setNewValue(50);
  channel->setNewValue(45);
  channel->setNewValue(25);
}

class TestingChannelElement : public Supla::ChannelElement {
 public:
  void setUsedConfigTypes(Supla::ConfigTypesBitmap ct) {
    usedConfigTypes = ct;
  }

  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local) override {
    (void)(local);
    EXPECT_NE(result, nullptr);
    if (!usedConfigTypes.isSet(result->ConfigType)) {
      return Supla::ApplyConfigResult::NotSupported;
    }
    if (result->ConfigSize == 0) {
      return Supla::ApplyConfigResult::SetChannelConfigNeeded;
    }
    if (result->ConfigSize == 4) {
      return Supla::ApplyConfigResult::Success;
    }

    return Supla::ApplyConfigResult::DataError;
  }


  void fillChannelConfig(void *buf, int *size, uint8_t index) override {
    *size = 0;

    if (!usedConfigTypes.isSet(index)) {
      return;
    }

    auto cfg = reinterpret_cast<uint8_t *>(buf);
    cfg[0] = index;
    cfg[1] = 1;
    cfg[2] = 2;
    cfg[3] = 3;
    *size = 4;
  }

};

using ::testing::_;
using ::testing::Return;

TEST(ChannelElementTests, ConfigExchangeNoConfigOnServer) {
  Supla::Channel::resetToDefaults();
  ASSERT_EQ(Supla::LocalAction::getClientListPtr(), nullptr);

  SuplaSrpcLayerMock srpc;
  SimpleTime time;

  TestingChannelElement element;
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);

  Supla::ConfigTypesBitmap ct;
  ct.set(SUPLA_CONFIG_TYPE_DEFAULT);

  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_DEFAULT))
          .WillOnce(Return(true));

  element.setUsedConfigTypes(ct);

  element.onInit();
  element.onRegistered(&srpc);

  for (int i = 0; i < 10; i++) {
    time.advance(500);
    element.iterateAlways();
    element.iterateConnected();
  }

  TSD_ChannelConfig config = {};
  config.ChannelNumber = channel->getChannelNumber();
  config.Func = SUPLA_CHANNELFNC_POWERSWITCH;
  config.ConfigSize = 0;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;

  EXPECT_EQ(element.handleChannelConfig(&config, false),
      SUPLA_CONFIG_RESULT_TRUE);

  for (int i = 0; i < 10; i++) {
    time.advance(500);
    element.iterateAlways();
    element.iterateConnected();
  }

  element.handleChannelConfigFinished();

  for (int i = 0; i < 10; i++) {
    time.advance(500);
    element.iterateAlways();
    element.iterateConnected();
  }

  config.ConfigSize = 4;
  EXPECT_EQ(element.handleChannelConfig(&config, false),
      SUPLA_CONFIG_RESULT_TRUE);
}

TEST(ChannelElementTests, ConfigExchange2xNoConfigOnServer) {
  Supla::Channel::resetToDefaults();
  ASSERT_EQ(Supla::LocalAction::getClientListPtr(), nullptr);

  SuplaSrpcLayerMock srpc;
  SimpleTime time;

  TestingChannelElement element;
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);

  Supla::ConfigTypesBitmap ct;
  ct.set(SUPLA_CONFIG_TYPE_DEFAULT);
  ct.set(SUPLA_CONFIG_TYPE_EXTENDED);

  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_DEFAULT))
          .WillOnce(Return(true));


  element.setUsedConfigTypes(ct);

  element.onInit();
  element.onRegistered(&srpc);

  for (int i = 0; i < 10; i++) {
    time.advance(500);
    element.iterateAlways();
    element.iterateConnected();
  }

  TSD_ChannelConfig config = {};
  config.ChannelNumber = channel->getChannelNumber();
  config.Func = SUPLA_CHANNELFNC_POWERSWITCH;
  config.ConfigSize = 0;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;

  EXPECT_EQ(element.handleChannelConfig(&config, false),
      SUPLA_CONFIG_RESULT_TRUE);

  for (int i = 0; i < 10; i++) {
    time.advance(500);
    element.iterateAlways();
    element.iterateConnected();
  }

  element.handleChannelConfigFinished();

  for (int i = 0; i < 10; i++) {
    time.advance(500);
    element.iterateAlways();
    element.iterateConnected();
  }

  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&srpc));

  TSDS_SetChannelConfigResult result = {};
  result.ChannelNumber = element.getChannel()->getChannelNumber();
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.Result = SUPLA_CONFIG_RESULT_TRUE;

  element.handleSetChannelConfigResult(&result);

  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_EXTENDED))
          .WillOnce(Return(true));

  for (int i = 0; i < 10; i++) {
    time.advance(500);
    element.iterateAlways();
    element.iterateConnected();
  }
}
