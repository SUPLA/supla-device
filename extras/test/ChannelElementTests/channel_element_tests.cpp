// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/action_handler.h>
#include <supla/actions.h>
#include <supla/channel_element.h>
#include <supla/condition.h>
#include <supla/events.h>
#include <supla_srpc_layer_mock.h>

#include "supla/element_with_channel_actions.h"

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

namespace {

class ExternalChannelElement : public Supla::ChannelElement {
 public:
  ExternalChannelElement(Supla::Channel &channel, Supla::ElementMode mode)
      : Supla::ChannelElement(channel, mode) {
  }
};

}  // namespace

TEST(ChannelElementTests, OwnsChannelByDefault) {
  Supla::Channel::resetToDefaults();

  Supla::ChannelElement element;

  ASSERT_NE(element.getChannel(), nullptr);
  EXPECT_EQ(element.getChannelNumber(), 0);
  EXPECT_EQ(Supla::Channel::Begin(), element.getChannel());
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), &element);

  Supla::Channel::resetToDefaults();
}

TEST(ChannelElementTests, UsesExternalChannelWithoutDuplicateRegistration) {
  Supla::Channel::resetToDefaults();

  Supla::Channel externalChannel;
  ExternalChannelElement element(externalChannel,
                                 Supla::ElementMode::Registered);

  EXPECT_EQ(element.getChannel(), &externalChannel);
  EXPECT_EQ(element.getChannelNumber(), 0);
  EXPECT_EQ(Supla::Channel::Begin(), &externalChannel);
  EXPECT_EQ(Supla::Channel::Last(), &externalChannel);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), &element);

  Supla::Channel::resetToDefaults();
}

TEST(ChannelElementTests, DetachedExternalChannelElementIsNotRegistered) {
  Supla::Channel::resetToDefaults();

  auto elementBefore = Supla::Element::begin();
  auto lastElementBefore = Supla::Element::last();
  auto lookupBefore = Supla::Element::getElementByChannelNumber(0);
  Supla::Channel externalChannel;
  ExternalChannelElement element(externalChannel, Supla::ElementMode::Detached);

  EXPECT_EQ(element.getChannel(), &externalChannel);
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), lookupBefore);
  EXPECT_EQ(Supla::Element::begin(), elementBefore);
  EXPECT_EQ(Supla::Element::last(), lastElementBefore);

  Supla::Channel::resetToDefaults();
}

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

  Supla::ChannelConfigState getChannelConfigState() const {
    return channelConfigState;
  }

  uint8_t getLocallyChangedConfigTypes() const {
    return locallyChangedConfigTypes.getAll();
  }

  bool isLocalConfigChangePending(int configType) const {
    return isLocalChannelConfigChangePending(configType);
  }

  bool loadConfigChangeFlagForTesting() {
    return loadConfigChangeFlag();
  }

  bool saveConfigChangeFlagForTesting() const {
    return saveConfigChangeFlag();
  }

  void clearLocalConfigChangesForTesting(int configType) {
    clearLocalConfigChanges(configType);
  }

  void markChannelConfigReceivedForTesting(int configType) {
    markChannelConfigReceived(configType);
  }

  int getAppliedConfigCount() const {
    return appliedConfigCount;
  }

  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local) override {
    (void)(local);
    EXPECT_NE(result, nullptr);
    if (!usedConfigTypes.isSet(result->ConfigType)) {
      return Supla::ApplyConfigResult::NotSupported;
    }
    appliedConfigCount++;
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

 private:
  int appliedConfigCount = 0;
};

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

TEST(ChannelElementTests, ConfigTypesBitmapPreservesStorageBitAssignments) {
  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  configTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  configTypes.set(SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE);
  configTypes.set(SUPLA_CONFIG_TYPE_OCR);
  configTypes.set(SUPLA_CONFIG_TYPE_EXTENDED);

  // The raw value is stored as uint32_t and must remain compatible with
  // existing devices: bits 0, 2, 3, 4 and 5 represent these five types.
  EXPECT_EQ(configTypes.getAll(), 0x3D);
  EXPECT_FALSE(configTypes.isSet(1));

  configTypes.clear(SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(configTypes.getAll(), 0x3C);
}

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

TEST(ChannelElementTests, SuccessfulSingleConfigExchangeResetsAttempts) {
  Supla::Channel::resetToDefaults();
  ASSERT_EQ(Supla::LocalAction::getClientListPtr(), nullptr);

  SuplaSrpcLayerMock srpc;
  TestingChannelElement element;
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);
  channel->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  element.setUsedConfigTypes(configTypes);
  element.onRegistered(&srpc);

  TSD_ChannelConfig config = {};
  config.ChannelNumber = channel->getChannelNumber();
  config.Func = SUPLA_CHANNELFNC_POWERSWITCH;
  config.ConfigSize = 4;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  EXPECT_EQ(element.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  element.handleChannelConfigFinished();

  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_DEFAULT))
      .Times(4)
      .WillRepeatedly(Return(true));

  TSDS_SetChannelConfigResult result = {};
  result.ChannelNumber = channel->getChannelNumber();
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.Result = SUPLA_CONFIG_RESULT_TRUE;

  for (int i = 0; i < 4; i++) {
    config.ConfigSize = 0;
    EXPECT_EQ(element.handleChannelConfig(&config, true),
              SUPLA_CONFIG_RESULT_TRUE);
    EXPECT_FALSE(element.iterateConnected());
    element.handleSetChannelConfigResult(&result);
  }
}

TEST(ChannelElementTests, ExhaustedSendAttemptsAllowFreshLocalConfigExchange) {
  Supla::Channel::resetToDefaults();

  SuplaSrpcLayerMock srpc;
  TestingChannelElement element;
  EXPECT_EQ(element.getChannelConfigState(), Supla::ChannelConfigState::None);
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);
  channel->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  element.setUsedConfigTypes(configTypes);
  element.onRegistered(&srpc);
  element.handleChannelConfigFinished();

  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_DEFAULT))
      .Times(3)
      .WillRepeatedly(Return(false));
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(element.iterateConnected());
    EXPECT_EQ(element.getChannelConfigState(),
              Supla::ChannelConfigState::LocalChangePending);
    EXPECT_TRUE(element.isLocalConfigChangePending(SUPLA_CONFIG_TYPE_DEFAULT));
  }

  // Exhaustion must stop sending, without wrapping the counter or state.
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(element.iterateConnected());
  }
  EXPECT_FALSE(element.isLocalConfigChangePending(SUPLA_CONFIG_TYPE_DEFAULT));
  EXPECT_EQ(element.getChannelConfigState(), Supla::ChannelConfigState::None);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&srpc));

  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_DEFAULT))
      .WillOnce(Return(true));
  EXPECT_FALSE(element.iterateConnected());
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::LocalChangeSent);

  TSDS_SetChannelConfigResult result = {};
  result.ChannelNumber = channel->getChannelNumber();
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.Result = SUPLA_CONFIG_RESULT_TRUE;
  element.handleSetChannelConfigResult(&result);
  EXPECT_EQ(element.getChannelConfigState(), Supla::ChannelConfigState::None);
  EXPECT_FALSE(element.isLocalConfigChangePending(SUPLA_CONFIG_TYPE_DEFAULT));
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

TEST(ChannelElementTests, LocalConfigChangeKeepsProvenanceUntilAcknowledged) {
  Supla::Channel::resetToDefaults();

  SuplaSrpcLayerMock srpc;
  TestingChannelElement element;
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);
  channel->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  element.setUsedConfigTypes(configTypes);
  element.onRegistered(&srpc);

  TSD_ChannelConfig config = {};
  config.ChannelNumber = channel->getChannelNumber();
  config.Func = SUPLA_CHANNELFNC_POWERSWITCH;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = 4;
  EXPECT_EQ(element.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  element.handleChannelConfigFinished();
  EXPECT_EQ(element.getChannelConfigState(), Supla::ChannelConfigState::None);

  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::LocalChangePending);

  EXPECT_EQ(element.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(element.getAppliedConfigCount(), 1);
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::LocalChangePending);
  element.handleChannelConfigFinished();
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::LocalChangePending);

  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_DEFAULT))
      .WillOnce(Return(true));
  EXPECT_FALSE(element.iterateConnected());
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::LocalChangeSent);

  TSDS_SetChannelConfigResult result = {};
  result.ChannelNumber = channel->getChannelNumber();
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.Result = SUPLA_CONFIG_RESULT_TRUE;
  element.handleSetChannelConfigResult(&result);
  EXPECT_EQ(element.getChannelConfigState(), Supla::ChannelConfigState::None);
}

TEST(ChannelElementTests, LocalConfigChangeFailureDoesNotRetryIndefinitely) {
  Supla::Channel::resetToDefaults();

  SuplaSrpcLayerMock srpc;
  TestingChannelElement element;
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);
  channel->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  element.setUsedConfigTypes(configTypes);
  element.onRegistered(&srpc);

  TSD_ChannelConfig config = {};
  config.ChannelNumber = channel->getChannelNumber();
  config.Func = SUPLA_CHANNELFNC_POWERSWITCH;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = 4;
  EXPECT_EQ(element.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  element.handleChannelConfigFinished();

  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_DEFAULT))
      .WillOnce(Return(true));
  EXPECT_FALSE(element.iterateConnected());

  TSDS_SetChannelConfigResult result = {};
  result.ChannelNumber = channel->getChannelNumber();
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.Result = SUPLA_CONFIG_RESULT_FALSE;
  element.handleSetChannelConfigResult(&result);
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::SetChannelConfigFailed);

  EXPECT_TRUE(element.iterateConnected());
  EXPECT_TRUE(element.iterateConnected());
}

TEST(ChannelElementTests, GenericConfigResendKeepsGenericState) {
  Supla::Channel::resetToDefaults();

  SuplaSrpcLayerMock srpc;
  TestingChannelElement element;
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);
  channel->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  element.setUsedConfigTypes(configTypes);
  element.onRegistered(&srpc);

  TSD_ChannelConfig config = {};
  config.ChannelNumber = channel->getChannelNumber();
  config.Func = SUPLA_CHANNELFNC_POWERSWITCH;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = 4;
  EXPECT_EQ(element.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  element.handleChannelConfigFinished();

  element.triggerSetChannelConfig();
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::ResendConfig);

  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_DEFAULT))
      .WillOnce(Return(true));
  EXPECT_FALSE(element.iterateConnected());
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::SetChannelConfigSend);
}

TEST(ChannelElementTests, LocalChangeBlocksOnlyMatchingConfigType) {
  Supla::Channel::resetToDefaults();

  SuplaSrpcLayerMock srpc;
  TestingChannelElement element;
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);
  channel->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  configTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  element.setUsedConfigTypes(configTypes);
  element.onRegistered(&srpc);
  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE, true);

  TSD_ChannelConfig config = {};
  config.ChannelNumber = channel->getChannelNumber();
  config.Func = SUPLA_CHANNELFNC_POWERSWITCH;
  config.ConfigSize = 4;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  EXPECT_EQ(element.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(element.getAppliedConfigCount(), 1);

  config.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  EXPECT_EQ(element.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(element.getAppliedConfigCount(), 1);
  element.handleChannelConfigFinished();

  EXPECT_CALL(srpc,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_POWERSWITCH,
                               _,
                               4,
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
      .WillOnce(Return(true));
  EXPECT_FALSE(element.iterateConnected());

  TSDS_SetChannelConfigResult result = {};
  result.ChannelNumber = channel->getChannelNumber();
  result.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  result.Result = SUPLA_CONFIG_RESULT_TRUE;
  element.handleSetChannelConfigResult(&result);
  EXPECT_EQ(element.getChannelConfigState(), Supla::ChannelConfigState::None);
}

TEST(ChannelElementTests, FailedLocalTypeDoesNotBlockNextLocalType) {
  Supla::Channel::resetToDefaults();

  SuplaSrpcLayerMock srpc;
  TestingChannelElement element;
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);
  channel->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  configTypes.set(SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE);
  element.setUsedConfigTypes(configTypes);
  element.onRegistered(&srpc);
  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE, true);
  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE, true);
  element.handleChannelConfigFinished();

  EXPECT_CALL(srpc,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_POWERSWITCH,
                               _,
                               4,
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
      .WillOnce(Return(true));
  EXPECT_FALSE(element.iterateConnected());

  TSDS_SetChannelConfigResult result = {};
  result.ChannelNumber = channel->getChannelNumber();
  result.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  result.Result = SUPLA_CONFIG_RESULT_FALSE;
  element.handleSetChannelConfigResult(&result);
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::LocalChangePending);

  EXPECT_CALL(srpc,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_POWERSWITCH,
                               _,
                               4,
                               SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE))
      .WillOnce(Return(true));
  EXPECT_FALSE(element.iterateConnected());
}

TEST(ChannelElementTests, LocalConfigTypesAreStoredAsUInt32) {
  Supla::Channel::resetToDefaults();

  ConfigMock cfg;
  TestingChannelElement element;
  EXPECT_CALL(cfg, init());
  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  configTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  element.setUsedConfigTypes(configTypes);
  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE, true);

  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 5))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000));
  EXPECT_TRUE(element.saveConfigChangeFlagForTesting());
}

TEST(ChannelElementTests, ClearingLastLocalChangeResumesMissingConfigUpload) {
  Supla::Channel::resetToDefaults();

  ConfigMock cfg;
  SuplaSrpcLayerMock srpc;
  TestingChannelElement element;
  EXPECT_CALL(cfg, init());
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);
  channel->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  configTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  element.setUsedConfigTypes(configTypes);
  element.onRegistered(&srpc);
  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
  element.markChannelConfigReceivedForTesting(SUPLA_CONFIG_TYPE_DEFAULT);
  element.handleChannelConfigFinished();

  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 0))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000));
  element.clearLocalConfigChangesForTesting(SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::ResendConfig);

  EXPECT_CALL(
      srpc,
      setChannelConfig(0,
                       SUPLA_CHANNELFNC_POWERSWITCH,
                       _,
                       4,
                       SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
      .WillOnce(Return(true));
  EXPECT_FALSE(element.iterateConnected());
}

TEST(ChannelElementTests, ClearingSentLocalChangeWaitsForAcknowledgement) {
  Supla::Channel::resetToDefaults();

  ConfigMock cfg;
  SuplaSrpcLayerMock srpc;
  TestingChannelElement element;
  EXPECT_CALL(cfg, init());
  auto channel = element.getChannel();
  channel->setType(SUPLA_CHANNELTYPE_RELAY);
  channel->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  element.setUsedConfigTypes(configTypes);
  element.onRegistered(&srpc);
  element.markChannelConfigReceivedForTesting(SUPLA_CONFIG_TYPE_DEFAULT);
  element.handleChannelConfigFinished();
  element.triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);

  EXPECT_CALL(
      srpc,
      setChannelConfig(
          0, SUPLA_CHANNELFNC_POWERSWITCH, _, 4, SUPLA_CONFIG_TYPE_DEFAULT))
      .WillOnce(Return(true));
  EXPECT_FALSE(element.iterateConnected());
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::LocalChangeSent);

  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 0))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000));
  element.clearLocalConfigChangesForTesting(SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(element.getChannelConfigState(),
            Supla::ChannelConfigState::LocalChangeSent);

  TSDS_SetChannelConfigResult result = {};
  result.ChannelNumber = channel->getChannelNumber();
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.Result = SUPLA_CONFIG_RESULT_TRUE;
  element.handleSetChannelConfigResult(&result);
  EXPECT_EQ(element.getChannelConfigState(), Supla::ChannelConfigState::None);
}

TEST(ChannelElementTests, LegacyConfigFlagsAreMigratedToTypedBitmap) {
  Supla::Channel::resetToDefaults();

  ConfigMock cfg;
  TestingChannelElement element;
  EXPECT_CALL(cfg, init());
  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  configTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  configTypes.set(SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE);
  configTypes.set(SUPLA_CONFIG_TYPE_EXTENDED);
  element.setUsedConfigTypes(configTypes);

  EXPECT_CALL(cfg, getUInt32(StrEq("0_cfg_chng_t"), _))
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .WillOnce(::testing::DoAll(SetArgPointee<1>(1), Return(true)));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .WillOnce(::testing::DoAll(SetArgPointee<1>(1), Return(true)));
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 45))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), 0))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_weekly_chng"), 0))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000));

  EXPECT_TRUE(element.loadConfigChangeFlagForTesting());
  EXPECT_EQ(element.getLocallyChangedConfigTypes(), 45);
  EXPECT_TRUE(
      element.isLocalConfigChangePending(SUPLA_CONFIG_TYPE_DEFAULT));
  EXPECT_TRUE(element.isLocalConfigChangePending(
      SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE));
  EXPECT_TRUE(element.isLocalConfigChangePending(
      SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE));
  EXPECT_TRUE(
      element.isLocalConfigChangePending(SUPLA_CONFIG_TYPE_EXTENDED));
}

TEST(ChannelElementTests, StoredConfigTypesAreSanitizedToUsedRuntimeBitmap) {
  Supla::Channel::resetToDefaults();

  ConfigMock cfg;
  TestingChannelElement element;
  EXPECT_CALL(cfg, init());
  Supla::ConfigTypesBitmap configTypes;
  configTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  element.setUsedConfigTypes(configTypes);

  EXPECT_CALL(cfg, getUInt32(StrEq("0_cfg_chng_t"), _))
      .WillOnce(::testing::DoAll(
          SetArgPointee<1>(0x80000005UL), Return(true)));
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 4))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000));

  EXPECT_TRUE(element.loadConfigChangeFlagForTesting());
  EXPECT_EQ(element.getLocallyChangedConfigTypes(), 4);
}

TEST(ChannelElementTests, LocalConfigTypeOutsideRuntimeBitmapIsRejected) {
  Supla::Channel::resetToDefaults();

  TestingChannelElement element;
  element.triggerSetChannelConfig(8, true);

  EXPECT_EQ(element.getLocallyChangedConfigTypes(), 0);
  EXPECT_EQ(element.getChannelConfigState(), Supla::ChannelConfigState::None);
}

TEST(ChannelElementTests, NewLocalConfigAfterRejectionStartsFreshAttempts) {
  for (int configType : {SUPLA_CONFIG_TYPE_DEFAULT,
                         SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE,
                         SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE,
                         SUPLA_CONFIG_TYPE_EXTENDED}) {
    SCOPED_TRACE(configType);
    Supla::Channel::resetToDefaults();
    SuplaSrpcLayerMock srpc;
    TestingChannelElement element;
    element.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
    Supla::ConfigTypesBitmap configTypes;
    configTypes.set(configType);
    element.setUsedConfigTypes(configTypes);
    element.onRegistered(&srpc);
    element.markChannelConfigReceivedForTesting(configType);
    element.handleChannelConfigFinished();

    // More edits than the retry budget: each new edit must have its own budget.
    for (int edit = 0; edit < 4; ++edit) {
      SCOPED_TRACE(edit);
      element.triggerSetChannelConfig(configType, true);
      ASSERT_TRUE(element.isLocalConfigChangePending(configType));
      ASSERT_EQ(element.getChannelConfigState(),
                Supla::ChannelConfigState::LocalChangePending);
      EXPECT_CALL(srpc, setChannelConfig(0, SUPLA_CHANNELFNC_POWERSWITCH,
                                         _, 4, configType))
          .WillOnce(Return(true));
      EXPECT_FALSE(element.iterateConnected());
      ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&srpc));

      TSDS_SetChannelConfigResult result = {};
      result.ConfigType = configType;
      result.Result = SUPLA_CONFIG_RESULT_FALSE;
      element.handleSetChannelConfigResult(&result);
      EXPECT_FALSE(element.isLocalConfigChangePending(configType));
      EXPECT_EQ(element.getChannelConfigState(),
                Supla::ChannelConfigState::SetChannelConfigFailed);

      EXPECT_CALL(srpc, setChannelConfig(_, _, _, _, _)).Times(0);
      element.triggerSetChannelConfig(configType, false);
      EXPECT_TRUE(element.iterateConnected());
      EXPECT_TRUE(element.iterateConnected());
      ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&srpc));
    }
  }
}
