// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
#include <supla/control/weekly_schedule_buffer.h>
#include <supla/control/virtual_relay.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/device/register_device.h>
#include <storage_mock.h>
#include <vector>
#include "clock_stub.h"
#include "supla/actions.h"
#include "supla/events.h"

using testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAreArray;
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
    suplaSrpc = new SuplaSrpcStub(&sd);
    suplaSrpc->setRegisteredAndReady();
    Supla::Channel::resetToDefaults();
  }
  virtual void TearDown() {
    delete suplaSrpc;
    Supla::Channel::resetToDefaults();
  }
};

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

class InspectableButton : public Supla::Control::Button {
 public:
  using Supla::Control::Button::Button;

  bool isActionTriggerModeLocked() const {
    return actionTriggerModeLocked;
  }

  bool isLocalUnlockAllowed() const {
    return actionTriggerLocalUnlockAllowed;
  }

  bool keepsConfigButtonTriggerAlwaysAvailable() const {
    return keepConfigButtonTriggerAlwaysAvailable;
  }
};

class TimeInterfaceStub : public TimeInterface {
 public:
  uint32_t millis() override {
    static uint32_t value = 0;
    value += 1000;
    return value;
  }
};

class ActionTriggerWeeklyConfigHandlerForTests
    : public Supla::Control::WeeklyScheduleConfigHandler {
 public:
  void onLoadConfig() override {
    loadCount++;
  }

  bool supportsConfigType(uint8_t configType) const override {
    return configType == SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  }

  Supla::ApplyConfigResult applyChannelConfig(
      TSD_ChannelConfig *config, bool local) override {
    appliedConfig = config;
    appliedLocally = local;
    applyCount++;
    return Supla::ApplyConfigResult::Success;
  }

  void fillChannelConfig(void *config,
                         int *size,
                         uint8_t configType) override {
    if (size == nullptr) {
      return;
    }
    *size = 0;
    if (config == nullptr ||
        configType != SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) {
      return;
    }
    auto *schedule = reinterpret_cast<TChannelConfig_WeeklySchedule *>(config);
    memset(schedule, 0, sizeof(*schedule));
    schedule->Program[0].Mode = SUPLA_BUTTON_MODE_LOCKED;
    *size = sizeof(*schedule);
    fillCount++;
  }

  void purgeConfig() override {
    purgeCount++;
  }

  TSD_ChannelConfig *appliedConfig = nullptr;
  int loadCount = 0;
  int applyCount = 0;
  int fillCount = 0;
  int purgeCount = 0;
  bool appliedLocally = false;
};

void ignoreAtValueUpdates(SrpcMock *srpc) {
  EXPECT_CALL(*srpc, valueChanged(_, _, _, _, _)).Times(AnyNumber());
}

void expectActionTriggerConfigStorage(ConfigMock *cfg,
                                      int weeklyScheduleBlobSize = 0) {
  EXPECT_CALL(*cfg, getBlobSize(testing::StrEq("0_at_weekly")))
      .WillOnce(Return(weeklyScheduleBlobSize));
  EXPECT_CALL(*cfg, getUInt32(testing::StrEq("0_cfg_chng_t"), _))
      .WillOnce(Return(false));
  EXPECT_CALL(*cfg, getUInt8(testing::StrEq("0_cfg_chng"), _))
      .WillOnce(Return(false));
  EXPECT_CALL(*cfg, getUInt8(testing::StrEq("0_weekly_chng"), _))
      .WillOnce(Return(false));
}

void loadMqttActionTriggerMode(Supla::Control::ActionTrigger *at,
                               int32_t mode) {
  ConfigMock cfg;
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg, getInt32(_, _)).WillRepeatedly([mode](const char *key,
                                                         int32_t *value) {
    if (strcmp(key, "0_mqtt_at") == 0) {
      *value = mode;
      return true;
    }
    EXPECT_STREQ(key, "0_at_unlock");
    return false;
  });
  expectActionTriggerConfigStorage(&cfg);
  at->onLoadConfig(nullptr);
}

void applyActionTriggerServerConfig(Supla::Control::ActionTrigger *at,
                                    uint32_t activeActions) {
  TSD_ChannelConfig result = {};
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions = activeActions;
  memcpy(result.Config, &config, sizeof(config));
  at->handleChannelConfig(&result);
}

TSD_ChannelConfig makeActionTriggerWeeklySchedule(uint8_t mode) {
  TSD_ChannelConfig result = {};
  result.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  result.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(result.Config);
  schedule->Program[0].Mode = mode;
  Supla::Control::WeeklyScheduleBuffer buffer;
  for (int index = 0; index < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; index++) {
    buffer.setWeeklySchedule(schedule, index, 1);
  }
  return result;
}

const TActionTriggerProperties *actionTriggerValue(
    const Supla::Control::ActionTrigger &at) {
  return reinterpret_cast<const TActionTriggerProperties *>(
      Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));
}

TEST_F(ActionTriggerTests, DefaultConfigIgnoresFunctionField) {
  Supla::Control::ActionTrigger at;
  TSD_ChannelConfig result = {};
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  auto *config =
      reinterpret_cast<TChannelConfig_ActionTrigger *>(result.Config);
  config->ActiveActions = SUPLA_ACTION_CAP_HOLD;

  EXPECT_EQ(at.handleChannelConfig(&result), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_TRUE(at.isAnyActionEnabledOnServer());
  EXPECT_EQ(at.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_ACTIONTRIGGER);
}

TEST_F(ActionTriggerTests, InvalidDefaultConfigDoesNotChangeActiveActions) {
  Supla::Control::ActionTrigger at;
  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_HOLD);
  TSD_ChannelConfig result = {};
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger) - 1;

  EXPECT_EQ(at.handleChannelConfig(&result), SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_TRUE(at.isAnyActionEnabledOnServer());
}

TEST_F(ActionTriggerTests, EmptyDefaultConfigPreservesActiveActions) {
  Supla::Control::ActionTrigger at;
  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_HOLD);
  TSD_ChannelConfig result = {};
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;

  EXPECT_EQ(at.handleChannelConfig(&result), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_TRUE(at.isAnyActionEnabledOnServer());
}

TEST_F(ActionTriggerTests, WeeklyScheduleIsAvailableByDefaultAndLoadedLazily) {
  Supla::Control::ActionTrigger at;

  EXPECT_TRUE(at.getChannel()->getFlags() &
              SUPLA_CHANNEL_FLAG_BUTTON_MODE_SUPPORTED);
  EXPECT_TRUE(at.getChannel()->getFlags() &
              SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);

  TSD_ChannelConfig emptyConfig = {};
  emptyConfig.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  EXPECT_EQ(at.handleWeeklySchedule(&emptyConfig, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  TChannelConfig_WeeklySchedule schedule;
  memset(&schedule, 0xFF, sizeof(schedule));
  int size = 0;
  at.fillChannelConfig(
      &schedule, &size, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  EXPECT_EQ(size, sizeof(TChannelConfig_WeeklySchedule));
  const TChannelConfig_WeeklySchedule defaultSchedule = {};
  EXPECT_EQ(memcmp(&schedule, &defaultSchedule, sizeof(schedule)), 0);
  EXPECT_FALSE(actionTriggerValue(at)->Flags &
               SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
}

TEST_F(ActionTriggerTests, WeeklyScheduleCanBeDisabledWithoutDisablingLock) {
  Supla::Control::ActionTrigger at;
  at.setWeeklyScheduleAvailable(false);

  EXPECT_FALSE(at.getChannel()->getFlags() &
               SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
  EXPECT_TRUE(at.getChannel()->getFlags() &
              SUPLA_CHANNEL_FLAG_BUTTON_MODE_SUPPORTED);
  auto config = makeActionTriggerWeeklySchedule(SUPLA_BUTTON_MODE_LOCKED);
  EXPECT_EQ(at.handleWeeklySchedule(&config, false, false),
            SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED);

  TSD_SuplaChannelNewValue command = {};
  reinterpret_cast<TActionTriggerProperties *>(command.value)->ButtonMode =
      SUPLA_BUTTON_MODE_LOCKED;
  EXPECT_EQ(at.handleNewValueFromServer(&command), 1);
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_LOCKED);
}

TEST_F(ActionTriggerTests,
       ExternalWeeklyScheduleWithoutConfigControlsOnlyOperatingMode) {
  InspectableButton button(10);
  Supla::Control::ActionTrigger at;
  at.attach(button);
  ASSERT_TRUE(at.setWeeklyScheduleController(
      new Supla::Control::ExternalManagedWeeklySchedule()));
  at.onLoadConfig(nullptr);

  EXPECT_FALSE(at.getChannel()->getFlags() &
               SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
  EXPECT_TRUE(at.getChannel()->getFlags() &
              SUPLA_CHANNEL_FLAG_BUTTON_MODE_SUPPORTED);

  TSD_ChannelConfig weeklyConfig = {};
  weeklyConfig.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  EXPECT_EQ(at.handleWeeklySchedule(&weeklyConfig, false, false),
            SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED);

  TSD_SuplaChannelNewValue command = {};
  auto *properties =
      reinterpret_cast<TActionTriggerProperties *>(command.value);
  properties->ButtonMode = SUPLA_BUTTON_MODE_CMD_WEEKLY_SCHEDULE;
  ASSERT_EQ(at.handleNewValueFromServer(&command), 1);
  EXPECT_TRUE(actionTriggerValue(at)->Flags &
              SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_NOT_SET);
  EXPECT_FALSE(button.isActionTriggerModeLocked());

  properties->ButtonMode = SUPLA_BUTTON_MODE_CMD_SWITCH_TO_MANUAL;
  ASSERT_EQ(at.handleNewValueFromServer(&command), 1);
  EXPECT_FALSE(actionTriggerValue(at)->Flags &
               SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_NOT_SET);
}

TEST_F(ActionTriggerTests,
       ExternallyExecutedWeeklyScheduleUsesIndependentConfigHandler) {
  ActionTriggerWeeklyConfigHandlerForTests configHandler;
  InspectableButton button(10);
  Supla::Control::ActionTrigger at;
  at.attach(button);
  ASSERT_TRUE(at.setWeeklyScheduleController(
      new Supla::Control::ExternalManagedWeeklySchedule(), &configHandler));

  at.onLoadConfig(nullptr);
  EXPECT_EQ(configHandler.loadCount, 1);
  EXPECT_TRUE(at.getChannel()->getFlags() &
              SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);

  TSD_ChannelConfig weeklyConfig = {};
  weeklyConfig.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  weeklyConfig.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  EXPECT_EQ(at.handleWeeklySchedule(&weeklyConfig, false, true),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(configHandler.applyCount, 1);
  EXPECT_EQ(configHandler.appliedConfig, &weeklyConfig);
  EXPECT_TRUE(configHandler.appliedLocally);

  TChannelConfig_WeeklySchedule filledConfig = {};
  int size = 0;
  at.fillChannelConfig(
      &filledConfig, &size, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  EXPECT_EQ(configHandler.fillCount, 1);
  EXPECT_EQ(size, sizeof(filledConfig));
  EXPECT_EQ(filledConfig.Program[0].Mode, SUPLA_BUTTON_MODE_LOCKED);

  TSD_SuplaChannelNewValue command = {};
  reinterpret_cast<TActionTriggerProperties *>(command.value)->ButtonMode =
      SUPLA_BUTTON_MODE_CMD_WEEKLY_SCHEDULE;
  ASSERT_EQ(at.handleNewValueFromServer(&command), 1);
  EXPECT_TRUE(actionTriggerValue(at)->Flags &
              SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_NOT_SET);
  EXPECT_FALSE(button.isActionTriggerModeLocked());

  at.purgeConfig();
  EXPECT_EQ(configHandler.purgeCount, 1);
}

TEST_F(ActionTriggerTests,
       ExternalWeeklyScheduleModeIsRestoredFromStateStorage) {
  StorageMock storage;
  storage.defaultInitialization(sizeof(uint32_t));
  InspectableButton button(10);
  Supla::Control::ActionTrigger at;
  at.attach(button);
  at.enableStateStorage();
  ASSERT_TRUE(at.setWeeklyScheduleController(
      new Supla::Control::ExternalManagedWeeklySchedule()));
  at.onLoadConfig(nullptr);

  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce([](uint32_t, unsigned char *data, uint32_t, bool) {
        const uint32_t state = static_cast<uint32_t>(1) << 30;
        memcpy(data, &state, sizeof(state));
        return sizeof(state);
      });
  at.onLoadState();

  EXPECT_TRUE(actionTriggerValue(at)->Flags &
              SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_NOT_SET);
  EXPECT_FALSE(button.isActionTriggerModeLocked());
}

TEST_F(ActionTriggerTests,
       WeeklyScheduleControllerCannotChangeAfterActionTriggerConfigLoad) {
  Supla::Control::ActionTrigger at;
  ASSERT_TRUE(at.setWeeklyScheduleController(
      new Supla::Control::ExternalManagedWeeklySchedule()));
  at.onLoadConfig(nullptr);

  auto *replacement = new Supla::Control::ExternalManagedWeeklySchedule();
  EXPECT_FALSE(at.setWeeklyScheduleController(replacement));
  delete replacement;

  TSD_SuplaChannelNewValue command = {};
  reinterpret_cast<TActionTriggerProperties *>(command.value)->ButtonMode =
      SUPLA_BUTTON_MODE_CMD_WEEKLY_SCHEDULE;
  EXPECT_EQ(at.handleNewValueFromServer(&command), 1);
  EXPECT_TRUE(actionTriggerValue(at)->Flags &
              SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
}

TEST_F(ActionTriggerTests, WeeklyScheduleConfigDoesNotEnableSchedule) {
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;
  auto config = makeActionTriggerWeeklySchedule(SUPLA_BUTTON_MODE_LOCKED);

  EXPECT_EQ(at.handleWeeklySchedule(&config, false, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_FALSE(actionTriggerValue(at)->Flags &
               SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_NOT_SET);
}

TEST_F(ActionTriggerTests, WeeklyScheduleAndManualCommandsControlButtonMode) {
  TimeInterfaceStub time;
  ClockStub clock;
  InspectableButton button(10);
  Supla::Control::ActionTrigger at;
  at.attach(button);
  at.activateAction(SUPLA_ACTION_CAP_TOGGLE_x1);
  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_TOGGLE_x1);
  auto config = makeActionTriggerWeeklySchedule(SUPLA_BUTTON_MODE_LOCKED);
  ASSERT_EQ(at.handleWeeklySchedule(&config, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  TSD_SuplaChannelNewValue command = {};
  auto *properties =
      reinterpret_cast<TActionTriggerProperties *>(command.value);
  properties->ButtonMode = SUPLA_BUTTON_MODE_CMD_WEEKLY_SCHEDULE;
  EXPECT_EQ(at.handleNewValueFromServer(&command), 1);
  EXPECT_TRUE(actionTriggerValue(at)->Flags &
              SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_LOCKED);
  EXPECT_TRUE(button.isActionTriggerModeLocked());

  at.handleAction(0, Supla::SEND_AT_TOGGLE_x1);
  EXPECT_EQ(static_cast<Supla::AtChannel *>(at.getChannel())->popAction(), 0);

  properties->ButtonMode = SUPLA_BUTTON_MODE_NOT_SET;
  EXPECT_EQ(at.handleNewValueFromServer(&command), 1);
  EXPECT_FALSE(actionTriggerValue(at)->Flags &
               SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_NOT_SET);
  EXPECT_FALSE(button.isActionTriggerModeLocked());

  at.handleAction(0, Supla::SEND_AT_TOGGLE_x1);
  EXPECT_EQ(static_cast<Supla::AtChannel *>(at.getChannel())->popAction(),
            SUPLA_ACTION_CAP_TOGGLE_x1);
}

TEST_F(ActionTriggerTests, ManualLockIsReportedAsChannelValue) {
  SrpcMock srpc;
  EXPECT_CALL(srpc, getChannelConfig(_, _)).Times(AnyNumber());
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;
  at.onRegistered(suplaSrpc);
  at.iterateConnected();
  testing::Mock::VerifyAndClearExpectations(&srpc);

  TSD_SuplaChannelNewValue command = {};
  reinterpret_cast<TActionTriggerProperties *>(command.value)->ButtonMode =
      SUPLA_BUTTON_MODE_LOCKED;
  ASSERT_EQ(at.handleNewValueFromServer(&command), 1);

  TActionTriggerProperties expected = {};
  expected.ButtonMode = SUPLA_BUTTON_MODE_LOCKED;
  std::vector<char> expectedValue(SUPLA_CHANNELVALUE_SIZE, 0);
  memcpy(expectedValue.data(), &expected, sizeof(expected));
  EXPECT_CALL(srpc,
              valueChanged(nullptr,
                           at.getChannelNumber(),
                           ElementsAreArray(expectedValue),
                           0,
                           0));
  at.iterateConnected();
}

TEST_F(ActionTriggerTests, LockActionsShareButtonModeAndAttachLifecycle) {
  TimeInterfaceStub time;
  InspectableButton button(10);
  Supla::Control::ActionTrigger at;

  at.setLocalUnlockAllowed(true)
      .setKeepConfigButtonTriggerAlwaysAvailable(true);
  at.attach(button);
  EXPECT_FALSE(button.isActionTriggerModeLocked());
  EXPECT_TRUE(button.isLocalUnlockAllowed());
  EXPECT_TRUE(button.keepsConfigButtonTriggerAlwaysAvailable());

  at.handleAction(Supla::ON_HOLD, Supla::LOCK);
  EXPECT_TRUE(button.isActionTriggerModeLocked());
  EXPECT_EQ(static_cast<Supla::AtChannel *>(at.getChannel())->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);

  at.handleAction(Supla::ON_HOLD, Supla::UNLOCK);
  EXPECT_FALSE(button.isActionTriggerModeLocked());
  EXPECT_EQ(static_cast<Supla::AtChannel *>(at.getChannel())->getButtonMode(),
            SUPLA_BUTTON_MODE_NOT_SET);

  at.setLocalUnlockAllowed(false);
  at.handleAction(Supla::ON_HOLD, Supla::LOCK);
  at.handleAction(Supla::ON_HOLD, Supla::TOGGLE_LOCK);
  EXPECT_EQ(static_cast<Supla::AtChannel *>(at.getChannel())->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);

  at.attach(nullptr);
  EXPECT_FALSE(button.isActionTriggerModeLocked());
  EXPECT_FALSE(button.isLocalUnlockAllowed());
  EXPECT_FALSE(button.keepsConfigButtonTriggerAlwaysAvailable());
}

TEST_F(ActionTriggerTests, RebuildPreservesUserLockActionBindings) {
  InspectableButton button(10);
  Supla::Control::ActionTrigger at;
  at.attach(button);

  button.addAction(Supla::TOGGLE_LOCK, at, Supla::ON_HOLD);
  ASSERT_TRUE(button.hasEnabledAction(Supla::ON_HOLD, Supla::TOGGLE_LOCK));

  at.onInit();
  EXPECT_TRUE(button.hasEnabledAction(Supla::ON_HOLD, Supla::TOGGLE_LOCK));

  TSD_ChannelConfig config = {};
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  auto *actionTriggerConfig =
      reinterpret_cast<TChannelConfig_ActionTrigger *>(config.Config);
  actionTriggerConfig->ActiveActions = SUPLA_ACTION_CAP_HOLD;
  EXPECT_EQ(at.handleChannelConfig(&config), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_TRUE(button.hasEnabledAction(Supla::ON_HOLD, Supla::TOGGLE_LOCK));
}

TEST_F(ActionTriggerTests, LocalUnlockPolicyIsLoadedAndPropagatedToButton) {
  InspectableButton button(10);
  Supla::Control::ActionTrigger at;
  at.attach(button);
  ConfigMock cfg;

  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg, getInt32(_, _)).WillRepeatedly(
      [](const char *key, int32_t *value) {
        if (strcmp(key, "0_at_unlock") == 0) {
          *value = 1;
          return true;
        }
        EXPECT_STREQ(key, "0_mqtt_at");
        *value = 0;
        return true;
      });
  expectActionTriggerConfigStorage(&cfg);
  at.onLoadConfig(nullptr);

  EXPECT_TRUE(at.isLocalUnlockAllowed());
  EXPECT_TRUE(button.isLocalUnlockAllowed());
}

TEST_F(ActionTriggerTests, LocalLockActionLeavesWeeklyScheduleForManualMode) {
  InspectableButton button(10);
  Supla::Control::ActionTrigger at;
  at.attach(button);
  auto *schedule = new Supla::Control::ExternalManagedWeeklySchedule();
  ASSERT_TRUE(at.setWeeklyScheduleController(schedule));

  TSD_SuplaChannelNewValue command = {};
  auto *properties =
      reinterpret_cast<TActionTriggerProperties *>(command.value);
  properties->ButtonMode = SUPLA_BUTTON_MODE_CMD_WEEKLY_SCHEDULE;
  ASSERT_EQ(at.handleNewValueFromServer(&command), 1);
  ASSERT_TRUE(schedule->isActive());

  at.handleAction(Supla::ON_HOLD, Supla::UNLOCK);
  EXPECT_FALSE(schedule->isActive());
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_NOT_SET);
}

TEST_F(ActionTriggerTests, RestoredLockedStateIsPropagatedToAttachedButton) {
  TimeInterfaceStub time;
  StorageMock storage;
  storage.defaultInitialization(4);
  InspectableButton button(10);
  Supla::Control::ActionTrigger at;
  at.attach(button);
  at.enableStateStorage();

  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce([](uint32_t, unsigned char *data, uint32_t, bool) {
        uint32_t state = static_cast<uint32_t>(1) << 31;
        memcpy(data, &state, sizeof(state));
        return sizeof(uint32_t);
      });
  at.onLoadState();

  EXPECT_TRUE(button.isActionTriggerModeLocked());
}

TEST_F(ActionTriggerTests, InvalidWeeklyScheduleModeIsRejected) {
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;
  auto config = makeActionTriggerWeeklySchedule(0xFF);

  EXPECT_EQ(at.handleWeeklySchedule(&config, false, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_FALSE(actionTriggerValue(at)->Flags &
               SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
}

TEST_F(ActionTriggerTests, WeeklyScheduleModeIsRestoredFromStateStorage) {
  TimeInterfaceStub time;
  ConfigMock cfg;
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg, getInt32(_, _)).WillRepeatedly([](const char *key,
                                                       int32_t *) {
    EXPECT_TRUE(strcmp(key, "0_mqtt_at") == 0 ||
                strcmp(key, "0_at_unlock") == 0);
    return false;
  });
  expectActionTriggerConfigStorage(&cfg,
                                   sizeof(TChannelConfig_WeeklySchedule));
  StorageMock storage;
  storage.defaultInitialization(4);
  Supla::Control::ActionTrigger at;
  at.enableStateStorage();
  at.onLoadConfig(nullptr);

  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce([](uint32_t, unsigned char *data, uint32_t, bool) {
        uint32_t state = static_cast<uint32_t>(1) << 30;
        memcpy(data, &state, sizeof(state));
        return sizeof(uint32_t);
      });
  at.onLoadState();

  EXPECT_TRUE(actionTriggerValue(at)->Flags &
              SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(actionTriggerValue(at)->ButtonMode, SUPLA_BUTTON_MODE_NOT_SET);
}

TEST_F(ActionTriggerTests, WeeklyScheduleModeIsSavedInStateStorage) {
  TimeInterfaceStub time;
  ClockStub clock;
  StorageMock storage;
  storage.defaultInitialization(4);
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillRepeatedly([](uint32_t, unsigned char *data, uint32_t, bool) {
        memset(data, 0, sizeof(uint32_t));
        return sizeof(uint32_t);
      });
  Supla::Control::ActionTrigger at;
  at.enableStateStorage();
  auto config = makeActionTriggerWeeklySchedule(SUPLA_BUTTON_MODE_LOCKED);
  ASSERT_EQ(at.handleWeeklySchedule(&config, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  EXPECT_CALL(storage, scheduleSave(5000, 2000));
  TSD_SuplaChannelNewValue command = {};
  reinterpret_cast<TActionTriggerProperties *>(command.value)->ButtonMode =
      SUPLA_BUTTON_MODE_CMD_WEEKLY_SCHEDULE;
  ASSERT_EQ(at.handleNewValueFromServer(&command), 1);

  EXPECT_CALL(storage, writeStorage(_, _, sizeof(uint32_t)))
      .WillOnce([](uint32_t, const unsigned char *data, uint32_t) {
        uint32_t state = 0;
        memcpy(&state, data, sizeof(state));
        EXPECT_EQ(state & (static_cast<uint32_t>(3) << 30),
                  static_cast<uint32_t>(1) << 30);
        EXPECT_EQ(state & ~(static_cast<uint32_t>(3) << 30), 0u);
        return sizeof(uint32_t);
      });
  at.onSaveState();
}

TEST_F(ActionTriggerTests, StateStorageDisabledDoesNotReadOrWriteMode) {
  StorageMock storage;
  storage.defaultInitialization(1);
  Supla::Control::ActionTrigger at;

  EXPECT_CALL(storage, readStorage(_, _, _, _)).Times(0);
  EXPECT_CALL(storage, writeStorage(_, _, _)).Times(0);
  EXPECT_CALL(storage, scheduleSave(_, _)).Times(0);

  at.onLoadState();
  at.onSaveState();

  TSD_SuplaChannelNewValue command = {};
  reinterpret_cast<TActionTriggerProperties *>(command.value)->ButtonMode =
      SUPLA_BUTTON_MODE_LOCKED;
  EXPECT_EQ(at.handleNewValueFromServer(&command), 1);
}

TEST_F(ActionTriggerTests, FirstEmptyServerConfigStillBuildsButtonHandlers) {
  Supla::Control::Button button(10);
  button.setMulticlickTime(300, true);
  Supla::Control::ActionTrigger at;
  at.attach(button);

  EXPECT_FALSE(button.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  applyActionTriggerServerConfig(&at, 0);
  EXPECT_TRUE(button.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
}

TEST_F(ActionTriggerTests, RepeatedServerConfigDoesNotScheduleAnotherSave) {
  StorageMock storage;
  Supla::Control::ActionTrigger at;
  at.enableStateStorage();
  EXPECT_CALL(storage, scheduleSave(2000, 0)).Times(2);

  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_TOGGLE_x2);
  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_TOGGLE_x2);
  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_TOGGLE_x3);
  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_TOGGLE_x3);
}

TEST_F(ActionTriggerTests,
       SameServerConfigIsReappliedAfterPublishingModeChange) {
  Supla::Control::Button button(10);
  button.setMulticlickTime(300, true);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock localHandler;
  button.addAction(Supla::INTERNAL_BUTTON_MOVE_UP, localHandler,
                   Supla::CONDITIONAL_ON_PRESS);
  button.addAction(Supla::INTERNAL_BUTTON_UP_STOP, localHandler,
                   Supla::CONDITIONAL_ON_RELEASE);
  at.attach(button);
  at.onInit();
  applyActionTriggerServerConfig(&at, 0);

  loadMqttActionTriggerMode(&at, 2);
  applyActionTriggerServerConfig(&at, 0);
  EXPECT_FALSE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_FALSE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_RELEASE)->isEnabled());

  loadMqttActionTriggerMode(&at, 0);
  applyActionTriggerServerConfig(&at, 0);
  EXPECT_TRUE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_TRUE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_RELEASE)->isEnabled());
}

TEST_F(ActionTriggerTests, AttachToMonostableButton) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
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

  at.handleChannelConfig(&result, false);
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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

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
  at.handleChannelConfig(&result, false);

  // it should be executed on ah mock
  b1.runAction(Supla::ON_CLICK_1);
}

TEST_F(ActionTriggerTests, AttachToBistableButton) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);
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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 2);
  EXPECT_EQ(propInRegister->disablesLocalOperation, SUPLA_ACTION_CAP_TOGGLE_x1);
}

TEST_F(ActionTriggerTests, AttachToMotionSensorButton) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);
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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 2);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_TURN_ON | SUPLA_ACTION_CAP_TURN_OFF);
}

TEST_F(ActionTriggerTests, SendActionOnce) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;
  Supla::Control::ActionTrigger at2;

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_TURN_ON;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result, false);

  config.ActiveActions = SUPLA_ACTION_CAP_SHORT_PRESS_x1;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at2.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_TURN_ON;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at.iterateConnected();
  at.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
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

TEST_F(ActionTriggerTests,
       PreserveCapabilitiesWithoutAttachedButtonOnChannelConfig) {
  Supla::Control::ActionTrigger at;

  at.activateAction(Supla::SEND_AT_TURN_ON);
  EXPECT_EQ(at.getChannel()->getActionTriggerCaps(),
            SUPLA_ACTION_CAP_TURN_ON);

  TSD_ChannelConfig config = {};
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger actionTriggerConfig = {};
  actionTriggerConfig.ActiveActions = 0;
  memcpy(config.Config, &actionTriggerConfig, sizeof(actionTriggerConfig));

  at.handleChannelConfig(&config);
  EXPECT_EQ(at.getChannel()->getActionTriggerCaps(),
            SUPLA_ACTION_CAP_TURN_ON);

  at.rebuildForAttachedButton();
  EXPECT_EQ(at.getChannel()->getActionTriggerCaps(),
            SUPLA_ACTION_CAP_TURN_ON);
}

TEST_F(ActionTriggerTests, RelatedChannel) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  TimeInterfaceStub time;
  Supla::Channel ch0;
  Supla::ChannelElement che1;
  Supla::Channel ch2;
  Supla::Channel ch3;
  Supla::ChannelElement che4;
  Supla::Control::ActionTrigger at;

  EXPECT_EQ(
      (Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()))[0], 0);

  at.setRelatedChannel(&che4);
  EXPECT_EQ(
      (Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()))[0], 5);

  at.setRelatedChannel(&ch0);
  EXPECT_EQ(
      (Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()))[0], 1);

  at.setRelatedChannel(ch3);
  EXPECT_EQ(
      (Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()))[0], 4);

  at.setRelatedChannel(che1);
  EXPECT_EQ(che1.getChannelNumber(), 1);
  EXPECT_EQ(
      (Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()))[0], 2);
}

TEST_F(ActionTriggerTests, RelatedChannelChangeSendsChannelValueUpdate) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  EXPECT_CALL(srpc, getChannelConfig(_, _)).Times(AnyNumber());
  TimeInterfaceStub time;
  Supla::Channel ch0;
  Supla::Channel ch1;
  Supla::Control::ActionTrigger at;

  at.setRelatedChannel(ch0);
  at.onRegistered(suplaSrpc);
  at.iterateConnected();
  testing::Mock::VerifyAndClearExpectations(&srpc);

  TActionTriggerProperties expected = {};
  expected.relatedChannelNumber = ch1.getChannelNumber() + 1;
  std::vector<char> expectedValue(SUPLA_CHANNELVALUE_SIZE, 0);
  memcpy(expectedValue.data(), &expected, sizeof(expected));
  EXPECT_CALL(srpc,
              valueChanged(nullptr,
                           at.getChannelNumber(),
                           ElementsAreArray(expectedValue),
                           0,
                           0));

  at.setRelatedChannel(ch1);
  at.iterateConnected();
}

TEST_F(ActionTriggerTests,
       InitialRelatedChannelDoesNotSendChannelValueAfterRegistration) {
  SrpcMock srpc;
  EXPECT_CALL(srpc, getChannelConfig(_, _)).Times(AnyNumber());
  TimeInterfaceStub time;
  Supla::Channel ch0;
  Supla::Control::ActionTrigger at;

  at.setRelatedChannel(ch0);
  at.onRegistered(suplaSrpc);

  EXPECT_CALL(srpc, valueChanged(_, _, _, _, _)).Times(0);
  at.iterateConnected();
}

TEST_F(ActionTriggerTests,
       ServerConfigRebuildWithSameValueDoesNotSendChannelValue) {
  SrpcMock srpc;
  EXPECT_CALL(srpc, getChannelConfig(_, _)).Times(AnyNumber());
  TimeInterfaceStub time;
  Supla::Channel relatedChannel;
  Supla::Control::Button button(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  button.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  at.attach(button);
  at.setRelatedChannel(relatedChannel);
  at.onInit();
  at.onRegistered(suplaSrpc);
  at.iterateConnected();
  testing::Mock::VerifyAndClearExpectations(&srpc);

  TSD_ChannelConfig config = {};
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger actionTriggerConfig = {};
  actionTriggerConfig.ActiveActions = 0;
  memcpy(config.Config, &actionTriggerConfig, sizeof(actionTriggerConfig));

  EXPECT_CALL(srpc, valueChanged(_, _, _, _, _)).Times(0);
  at.handleChannelConfig(&config);
  at.iterateConnected();
}

TEST_F(ActionTriggerTests, PendingActionDoesNotDropChannelValueUpdate) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  EXPECT_CALL(srpc, getChannelConfig(_, _)).Times(AnyNumber());
  TimeInterfaceStub time;
  Supla::Channel relatedChannel;
  Supla::Control::ActionTrigger at;

  at.onRegistered(suplaSrpc);

  TSD_ChannelConfig config = {};
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger actionTriggerConfig = {};
  actionTriggerConfig.ActiveActions = SUPLA_ACTION_CAP_TOGGLE_x1;
  memcpy(config.Config, &actionTriggerConfig, sizeof(actionTriggerConfig));
  at.handleChannelConfig(&config);

  at.handleAction(0, Supla::SEND_AT_TOGGLE_x1);

  TActionTriggerProperties expected = {};
  expected.relatedChannelNumber = relatedChannel.getChannelNumber() + 1;
  std::vector<char> expectedValue(SUPLA_CHANNELVALUE_SIZE, 0);
  memcpy(expectedValue.data(), &expected, sizeof(expected));
  EXPECT_CALL(srpc,
              actionTrigger(at.getChannelNumber(), SUPLA_ACTION_CAP_TOGGLE_x1));
  EXPECT_CALL(srpc,
              valueChanged(nullptr,
                           at.getChannelNumber(),
                           ElementsAreArray(expectedValue),
                           0,
                           0));

  at.setRelatedChannel(relatedChannel);
  at.iterateConnected();
  at.iterateConnected();
}

TEST_F(ActionTriggerTests, ManageLocalActionsForMonostableButtonOnPress) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);
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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
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

  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation, SUPLA_ACTION_CAP_HOLD);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation, SUPLA_ACTION_CAP_TOGGLE_x1 |
                                                    SUPLA_ACTION_CAP_TURN_ON);

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_TOGGLE_x1 | SUPLA_ACTION_CAP_TOGGLE_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

  EXPECT_TRUE(b1.getHandlerForFirstClient(Supla::ON_CHANGE)->isEnabled());
  EXPECT_FALSE(b1.getHandlerForFirstClient(Supla::ON_CLICK_1)->isEnabled());

  b1.runAction(Supla::ON_CHANGE);
  b1.runAction(Supla::ON_CLICK_1);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
}

TEST_F(ActionTriggerTests,
       ManageLocalActionsForBistableDirectionalPressAndRelease) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  Supla::Control::Button button(10);
  button.setMulticlickTime(500, true);
  Supla::Control::ActionTrigger actionTrigger;
  ActionHandlerMock localHandler;
  actionTrigger.setAlwaysUseOnClick1();

  button.addAction(Supla::INTERNAL_BUTTON_MOVE_UP,
                   localHandler,
                   Supla::CONDITIONAL_ON_PRESS);
  button.addAction(Supla::INTERNAL_BUTTON_UP_STOP,
                   localHandler,
                   Supla::CONDITIONAL_ON_RELEASE);
  actionTrigger.attach(button);

  actionTrigger.onInit();

  // Directional press/release actions must remain local actions. They must
  // not be converted to ON_CLICK_1 as an old bistable ON_CHANGE action was.
  EXPECT_TRUE(button.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_EQ(button.getHandlerForClient(&localHandler, Supla::ON_CLICK_1),
            nullptr);
  EXPECT_TRUE(button.isEventAlreadyUsed(Supla::CONDITIONAL_ON_PRESS, false));
  EXPECT_TRUE(
      button.isEventAlreadyUsed(Supla::CONDITIONAL_ON_RELEASE, false));
  EXPECT_TRUE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_TRUE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_RELEASE)->isEnabled());

  TSD_ChannelConfig result = {};
  result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions = SUPLA_ACTION_CAP_TOGGLE_x1;
  memcpy(result.Config, &config, sizeof(config));
  actionTrigger.handleChannelConfig(&result);

  // TOGGLE_x1 is the old bistable Action Trigger operation. It must disable
  // both new directional local edges, even though AT itself runs on click 1.
  EXPECT_FALSE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_FALSE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_RELEASE)->isEnabled());
  EXPECT_TRUE(
      button.getHandlerForClient(&actionTrigger, Supla::ON_CLICK_1)
          ->isEnabled());

  // No local movement may run in parallel with an active TOGGLE_x1 AT.
  EXPECT_CALL(localHandler, handleAction(_, _)).Times(0);
  button.runAction(Supla::CONDITIONAL_ON_PRESS);
  button.runAction(Supla::CONDITIONAL_ON_RELEASE);
  testing::Mock::VerifyAndClearExpectations(&localHandler);

  config.ActiveActions = SUPLA_ACTION_CAP_TURN_ON;
  memcpy(result.Config, &config, sizeof(config));
  actionTrigger.handleChannelConfig(&result);

  EXPECT_FALSE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_TRUE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_RELEASE)->isEnabled());

  config.ActiveActions = SUPLA_ACTION_CAP_TURN_OFF;
  memcpy(result.Config, &config, sizeof(config));
  actionTrigger.handleChannelConfig(&result);

  EXPECT_TRUE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_FALSE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_RELEASE)->isEnabled());

  config.ActiveActions = 0;
  memcpy(result.Config, &config, sizeof(config));
  actionTrigger.handleChannelConfig(&result);

  // Removing the AT configuration restores both local directional actions.
  EXPECT_TRUE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_PRESS)->isEnabled());
  EXPECT_TRUE(button.getHandlerForClient(
      &localHandler, Supla::CONDITIONAL_ON_RELEASE)->isEnabled());
  EXPECT_FALSE(button.getHandlerForClient(
      &actionTrigger, Supla::ON_PRESS)->isEnabled());
  EXPECT_FALSE(button.getHandlerForClient(
      &actionTrigger, Supla::ON_RELEASE)->isEnabled());

  EXPECT_CALL(localHandler,
              handleAction(Supla::CONDITIONAL_ON_PRESS,
                           Supla::INTERNAL_BUTTON_MOVE_UP));
  EXPECT_CALL(localHandler,
              handleAction(Supla::CONDITIONAL_ON_RELEASE,
                           Supla::INTERNAL_BUTTON_UP_STOP));
  button.runAction(Supla::CONDITIONAL_ON_PRESS);
  button.runAction(Supla::CONDITIONAL_ON_RELEASE);
}

TEST_F(ActionTriggerTests,
       ManageLocalActionsForBistableButtonConditionalOnChange) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation, SUPLA_ACTION_CAP_TOGGLE_x1);

  // another config from server which disables some actions
  config.ActiveActions =
      SUPLA_ACTION_CAP_TOGGLE_x1 | SUPLA_ACTION_CAP_TOGGLE_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);
}

TEST_F(ActionTriggerTests, RemoveSomeActionsFromATAttachWithStorage) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
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

  EXPECT_CALL(storage, scheduleSave(2000, 0));

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  // onLoadState expectations
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .Times(2)
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  EXPECT_EQ(
      Supla::RegisterDevice::getChannelFunctionList(at.getChannelNumber()),
      SUPLA_ACTION_CAP_SHORT_PRESS_x1 | SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
          SUPLA_ACTION_CAP_SHORT_PRESS_x5);
}

TEST_F(ActionTriggerTests, ManageLocalActionsForMonostableButtonWithCfg) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  // another config from server which disables some actions
  config.ActiveActions = SUPLA_ACTION_CAP_HOLD |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
                         SUPLA_ACTION_CAP_SHORT_PRESS_x5;
  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
  StorageMock storage;
  ConfigMock cfg;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg, getInt32(_, _)).WillRepeatedly([](const char *key,
                                                      int32_t *buf) {
    if (strcmp(key, "0_mqtt_at") == 0) {
      *buf = 2;
      return true;
    }
    EXPECT_STREQ(key, "0_at_unlock");
    return false;
  });
  expectActionTriggerConfigStorage(&cfg);

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

  EXPECT_CALL(storage, scheduleSave(2000, 0));

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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  EXPECT_EQ(
      Supla::RegisterDevice::getChannelFunctionList(at.getChannelNumber()),
      SUPLA_ACTION_CAP_SHORT_PRESS_x1 | SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
          SUPLA_ACTION_CAP_SHORT_PRESS_x5);
}

TEST_F(ActionTriggerTests, ActionHandlingType_PublishAllDisableNoneTest) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  StorageMock storage;
  ConfigMock cfg;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg, getInt32(_, _)).WillRepeatedly([](const char *key,
                                                      int32_t *buf) {
    if (strcmp(key, "0_mqtt_at") == 0) {
      *buf = 1;
      return true;
    }
    EXPECT_STREQ(key, "0_at_unlock");
    return false;
  });
  expectActionTriggerConfigStorage(&cfg);

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

  EXPECT_CALL(storage, scheduleSave(2000, 0)).Times(2);
  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  // onLoadState expectations
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .Times(2)
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

  EXPECT_EQ(propInRegister->relatedChannelNumber, 0);
  EXPECT_EQ(propInRegister->disablesLocalOperation,
            SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  EXPECT_EQ(
      Supla::RegisterDevice::getChannelFunctionList(at.getChannelNumber()),
      SUPLA_ACTION_CAP_SHORT_PRESS_x1 | SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
          SUPLA_ACTION_CAP_SHORT_PRESS_x4 | SUPLA_ACTION_CAP_SHORT_PRESS_x5);
}

TEST_F(ActionTriggerTests, ActionHandlingType_RelayOnSuplaServerTest) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  StorageMock storage;
  ConfigMock cfg;
  TimeInterfaceStub time;
  Supla::Control::Button b1(10);
  Supla::Control::ActionTrigger at;
  ActionHandlerMock ah;

  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg, getInt32(_, _)).WillRepeatedly([](const char *key,
                                                      int32_t *buf) {
    if (strcmp(key, "0_mqtt_at") == 0) {
      *buf = 0;
      return true;
    }
    EXPECT_STREQ(key, "0_at_unlock");
    return false;
  });
  expectActionTriggerConfigStorage(&cfg);

  // initial configuration
  b1.addAction(Supla::TOGGLE, ah, Supla::ON_PRESS);
  at.attach(b1);
  at.enableStateStorage();

  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_CLICK_1, false));
  EXPECT_TRUE(b1.isEventAlreadyUsed(Supla::ON_PRESS, false));
  EXPECT_FALSE(b1.isEventAlreadyUsed(Supla::ON_RELEASE, false));

  EXPECT_CALL(storage, scheduleSave(2000, 0)).Times(2);
  storage.defaultInitialization(4);

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  // onLoadState expectations
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .Times(2)
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
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
  at.handleChannelConfig(&result, false);

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
  at.handleChannelConfig(&result, false);

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
  ignoreAtValueUpdates(&srpc);
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

  EXPECT_CALL(ah, handleAction(_, Supla::TURN_ON)).Times(3);

  EXPECT_TRUE(b1.isMonostable());
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
  testing::Mock::VerifyAndClearExpectations(&ah);

  at.onInit();

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

  TSD_ChannelConfig result = {};
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger config = {};
  config.ActiveActions =
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x1 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x2 | SUPLA_ACTION_CAP_SHORT_PRESS_x3 |
      SUPLA_ACTION_CAP_SHORT_PRESS_x4 | SUPLA_ACTION_CAP_SHORT_PRESS_x5;

  memcpy(result.Config, &config, sizeof(TChannelConfig_ActionTrigger));

  at.handleChannelConfig(&result, false);
  b1.runAction(Supla::ON_PRESS);
  b1.runAction(Supla::ON_CLICK_1);
  b1.runAction(Supla::ON_HOLD);
  b1.runAction(Supla::ON_CLICK_6);
  b1.runAction(Supla::ON_CLICK_5);

  for (int i = 0; i < 10; i++) {
    at.iterateConnected();
  }
  testing::Mock::VerifyAndClearExpectations(&srpc);
  testing::Mock::VerifyAndClearExpectations(&mqtt);

  TActionTriggerProperties *propInRegister =
      reinterpret_cast<TActionTriggerProperties *>(
          Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));

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
  at.handleChannelConfig(&result, false);

  EXPECT_CALL(ah, handleAction(Supla::ON_CLICK_1, Supla::TURN_ON));
  EXPECT_CALL(srpc, actionTrigger(_, _)).Times(0);
  EXPECT_CALL(mqtt, publishTest(_, _, _, _)).Times(0);

  // It should be executed locally, without publishing an action trigger.
  b1.runAction(Supla::ON_CLICK_1);
  at.iterateConnected();
}

TEST_F(ActionTriggerTests,
       MqttModeCloudConfigPublishesOnlyConfiguredActionsToBothProtocols) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;

  loadMqttActionTriggerMode(&at, 0);
  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_HOLD);

  MqttMock mqtt(&sd);
  mqtt.onInit();
  mqtt.setRegisteredAndReady();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(
      mqtt,
      publishTest("supla/devices/supla-device/channels/0/button_long_press",
                  "button_long_press",
                  0,
                  false));

  at.handleAction(0, Supla::SEND_AT_HOLD);
  at.handleAction(0, Supla::SEND_AT_SHORT_PRESS_x1);
  at.iterateConnected();
  at.iterateConnected();

  testing::Mock::VerifyAndClearExpectations(&srpc);
  testing::Mock::VerifyAndClearExpectations(&mqtt);

  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_SHORT_PRESS_x1);

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(
      mqtt,
      publishTest("supla/devices/supla-device/channels/0/button_short_press",
                  "button_short_press",
                  0,
                  false));

  at.handleAction(0, Supla::SEND_AT_HOLD);
  at.handleAction(0, Supla::SEND_AT_SHORT_PRESS_x1);
  at.iterateConnected();
  at.iterateConnected();
}

TEST_F(ActionTriggerTests,
       MqttModePublishAllDisableNonePublishesAllToBothProtocols) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;

  loadMqttActionTriggerMode(&at, 1);
  applyActionTriggerServerConfig(&at, SUPLA_ACTION_CAP_HOLD);

  MqttMock mqtt(&sd);
  mqtt.onInit();
  mqtt.setRegisteredAndReady();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(
      mqtt,
      publishTest("supla/devices/supla-device/channels/0/button_long_press",
                  "button_long_press",
                  0,
                  false));
  EXPECT_CALL(
      mqtt,
      publishTest("supla/devices/supla-device/channels/0/button_short_press",
                  "button_short_press",
                  0,
                  false));

  at.handleAction(0, Supla::SEND_AT_HOLD);
  at.handleAction(0, Supla::SEND_AT_SHORT_PRESS_x1);
  at.iterateConnected();
  at.iterateConnected();
}

TEST_F(ActionTriggerTests,
       MqttModePublishAllDisableAllSurvivesEmptyCloudConfig) {
  SrpcMock srpc;
  ignoreAtValueUpdates(&srpc);
  TimeInterfaceStub time;
  Supla::Control::ActionTrigger at;

  loadMqttActionTriggerMode(&at, 2);
  applyActionTriggerServerConfig(&at, 0);

  MqttMock mqtt(&sd);
  mqtt.onInit();
  mqtt.setRegisteredAndReady();

  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_HOLD));
  EXPECT_CALL(srpc, actionTrigger(0, SUPLA_ACTION_CAP_SHORT_PRESS_x1));
  EXPECT_CALL(
      mqtt,
      publishTest("supla/devices/supla-device/channels/0/button_long_press",
                  "button_long_press",
                  0,
                  false));
  EXPECT_CALL(
      mqtt,
      publishTest("supla/devices/supla-device/channels/0/button_short_press",
                  "button_short_press",
                  0,
                  false));

  at.handleAction(0, Supla::SEND_AT_HOLD);
  at.handleAction(0, Supla::SEND_AT_SHORT_PRESS_x1);
  at.iterateConnected();
  at.iterateConnected();
}
