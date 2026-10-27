// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <clock_stub.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/at_channel.h>
#include <supla/control/action_trigger_parsed.h>
#include <supla/control/weekly_schedule_buffer.h>

#include <cstring>

namespace {
class Sd4linuxActionTriggerTests : public ::testing::Test {
 protected:
  void SetUp() override { Supla::Channel::resetToDefaults(); }
  void TearDown() override { Supla::Channel::resetToDefaults(); }

  void configure(Supla::Control::ActionTriggerParsed *at, uint32_t actions) {
    TSD_ChannelConfig config = {};
    config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
    config.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
    TChannelConfig_ActionTrigger value = {};
    value.ActiveActions = actions;
    memcpy(config.Config, &value, sizeof(value));
    at->handleChannelConfig(&config);
  }

  void setMode(Supla::Control::ActionTriggerParsed *at, uint8_t mode) {
    TSD_SuplaChannelNewValue command = {};
    reinterpret_cast<TActionTriggerProperties *>(command.value)->ButtonMode =
        mode;
    ASSERT_EQ(at->handleNewValueFromServer(&command), 1);
  }
};
}  // namespace

TEST_F(Sd4linuxActionTriggerTests, AllSupportedYamlNumbersKeepTheirMeaning) {
  Supla::Control::ActionTriggerParsed at("numbering");
  configure(&at, 0xFFFF);
  auto *channel = static_cast<Supla::AtChannel *>(at.getChannel());
  const int actions[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15};
  uint32_t caps = 0;
  for (int action : actions) {
    const uint32_t cap = uint32_t{1} << action;
    at.activateAction(action);
    caps |= cap;
    EXPECT_EQ(channel->getActionTriggerCaps(), caps);
    at.sendActionTrigger(action);
    EXPECT_EQ(channel->popAction(), cap);
    const int id = at.actionTriggerCapToActionId(cap);
    ASSERT_GE(id, 0);
    EXPECT_EQ(at.getActionTriggerCap(id), cap);
  }
}

TEST_F(Sd4linuxActionTriggerTests, InvalidNumbersAreIgnored) {
  Supla::Control::ActionTriggerParsed at("invalid");
  configure(&at, 0xFFFFFFFF);
  for (int action : {-1, 9, 16, 31, 32, 255}) {
    at.activateAction(action);
    at.sendActionTrigger(action);
  }
  EXPECT_EQ(at.getChannel()->getActionTriggerCaps(), 0);
  EXPECT_EQ(static_cast<Supla::AtChannel *>(at.getChannel())->popAction(), 0);
}

TEST_F(Sd4linuxActionTriggerTests, ServerSelectionDisableAndManualLock) {
  Supla::Control::ActionTriggerParsed at("manual");
  auto *channel = static_cast<Supla::AtChannel *>(at.getChannel());
  configure(&at, SUPLA_ACTION_CAP_ROTATE_RIGHT);
  at.sendActionTrigger(8);
  EXPECT_EQ(channel->popAction(), 0);
  at.sendActionTrigger(7);
  EXPECT_EQ(channel->popAction(), SUPLA_ACTION_CAP_ROTATE_RIGHT);
  at.disable();
  at.sendActionTrigger(7);
  EXPECT_EQ(channel->popAction(), 0);
  at.enable();
  setMode(&at, SUPLA_BUTTON_MODE_LOCKED);
  at.sendActionTrigger(7);
  EXPECT_EQ(channel->popAction(), 0);
  setMode(&at, SUPLA_BUTTON_MODE_NOT_SET);
  at.sendActionTrigger(7);
  EXPECT_EQ(channel->popAction(), SUPLA_ACTION_CAP_ROTATE_RIGHT);
}

TEST_F(Sd4linuxActionTriggerTests, WeeklyScheduleLockBlocksParsedEvents) {
  SimpleTime time;
  time.advance(1);
  ClockStub clock;
  Supla::Control::ActionTriggerParsed at("weekly");
  auto *channel = static_cast<Supla::AtChannel *>(at.getChannel());
  configure(&at, SUPLA_ACTION_CAP_ROTATE_LEFT);
  at.setWeeklyScheduleAvailable();
  TSD_ChannelConfig config = {};
  config.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  config.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  schedule->Program[0].Mode = SUPLA_BUTTON_MODE_LOCKED;
  Supla::Control::WeeklyScheduleBuffer buffer;
  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; i++) {
    buffer.setWeeklySchedule(schedule, i, 1);
  }
  ASSERT_EQ(at.handleWeeklySchedule(&config, false, false),
            SUPLA_CONFIG_RESULT_TRUE);
  setMode(&at, SUPLA_BUTTON_MODE_CMD_WEEKLY_SCHEDULE);
  ASSERT_EQ(channel->getButtonMode(), SUPLA_BUTTON_MODE_LOCKED);
  at.sendActionTrigger(8);
  EXPECT_EQ(channel->popAction(), 0);
  setMode(&at, SUPLA_BUTTON_MODE_NOT_SET);
  at.sendActionTrigger(8);
  EXPECT_EQ(channel->popAction(), SUPLA_ACTION_CAP_ROTATE_LEFT);
}
