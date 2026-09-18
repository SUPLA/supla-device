// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_simulator.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <output_mock.h>
#include <protocol_layer_mock.h>
#include <simple_time.h>
#include <string.h>
#include <supla/actions.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <supla/control/hvac_base.h>
#include <supla/control/relay.h>
#include <supla/events.h>
#include <supla/storage/state_storage_interface.h>
#include <supla/storage/storage.h>
#include <supla_io_mock.h>

#include <vector>

using ::testing::_;

namespace {

TEST(LegacyChannelCompatibilityTests,
     HvacCanUploadNewLocalConfigAfterServerReject) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ConfigSimulator cfg;
  OutputSimulator output;
  testing::NiceMock<ProtocolLayerMock> proto;
  Supla::Control::HvacBase hvac(&output);
  hvac.onLoadConfig(nullptr);
  hvac.onInit();
  hvac.onRegistered(nullptr);
  hvac.handleChannelConfigFinished();
  int sends = 0;
  ON_CALL(proto, setChannelConfig(_, _, _, _, _)).WillByDefault(
      [&](uint8_t, _supla_int_t, void *, int, uint8_t type) {
        if (type == SUPLA_CONFIG_TYPE_DEFAULT) {
          ++sends;
        }
        return true;
      });
  time.advance(10000);
  for (int i = 0; i < 10; ++i) {
    hvac.iterateConnected();
  }
  ASSERT_EQ(sends, 1);
  TSDS_SetChannelConfigResult ack = {};
  ack.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  ack.Result = SUPLA_CONFIG_RESULT_FALSE;
  hvac.handleSetChannelConfigResult(&ack);
  ASSERT_TRUE(hvac.setMinOnTimeS(45));
  uint32_t pendingTypes = 0;
  ASSERT_TRUE(cfg.getUInt32("0_cfg_chng_t", &pendingTypes));
  EXPECT_NE(pendingTypes & (1 << SUPLA_CONFIG_TYPE_DEFAULT), 0);
  time.advance(6000);
  for (int i = 0; i < 10; ++i) {
    hvac.iterateConnected();
  }
  EXPECT_EQ(sends, 2);
}

TEST(LegacyChannelCompatibilityTests,
     HvacAcceptsLegacyWeeklyScheduleArguments) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ConfigSimulator cfg;
  cfg.setChannelFunction(0, SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  OutputSimulator output;
  Supla::Control::HvacBase hvac(&output);
  hvac.onLoadConfig(nullptr);
  hvac.onInit();

  TSD_ChannelConfig remote = {};
  remote.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  remote.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  TChannelConfig_WeeklySchedule schedule = {};
  schedule.Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  schedule.Program[0].SetpointTemperatureHeat = 2100;
  schedule.Program[0].SetpointTemperatureCool = INT16_MIN;
  memset(schedule.Quarters, 0x11, sizeof(schedule.Quarters));
  remote.ConfigSize = sizeof(schedule);
  memcpy(remote.Config, &schedule, sizeof(schedule));
  ASSERT_EQ(hvac.handleWeeklySchedule(&remote), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac.getProgramById(1).SetpointTemperatureHeat, 2100);

  remote.ConfigType = SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE;
  schedule.Program[0].Mode = SUPLA_HVAC_MODE_COOL;
  schedule.Program[0].SetpointTemperatureHeat = INT16_MIN;
  schedule.Program[0].SetpointTemperatureCool = 2700;
  memcpy(remote.Config, &schedule, sizeof(schedule));
  ASSERT_EQ(hvac.handleWeeklySchedule(&remote, true), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac.getProgramById(1, true).SetpointTemperatureCool, 2700);

  uint32_t pendingTypes = 0;
  cfg.getUInt32("0_cfg_chng_t", &pendingTypes);
  // Both calls default to a server-originated config.
  EXPECT_EQ(pendingTypes, 0);
}

class HvacPendingScheduleRestoreTests : public ::testing::TestWithParam<int> {
};

// -1 is the legacy flag; other values are persisted config-type bitmaps.
INSTANTIATE_TEST_SUITE_P(
    StoredFlags, HvacPendingScheduleRestoreTests,
    ::testing::Values(-1, 1 << SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE,
                      1 << SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE,
                      (1 << SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) |
                          (1 << SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE)));

TEST_P(HvacPendingScheduleRestoreTests, PreservesOnlyLocallyChangedSchedules) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ConfigSimulator cfg;
  cfg.setChannelFunction(0, SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  const int pendingTypes = GetParam() < 0
      ? (1 << SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) |
            (1 << SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE)
      : GetParam();
  if (GetParam() < 0) {
    cfg.setUInt8("0_weekly_chng", 1);
  } else {
    cfg.setUInt32("0_cfg_chng_t", pendingTypes);
  }
  TChannelConfig_WeeklySchedule local = {};
  local.Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  local.Program[0].SetpointTemperatureHeat = 2100;
  local.Program[0].SetpointTemperatureCool = INT16_MIN;
  memset(local.Quarters, 0x11, sizeof(local.Quarters));
  cfg.setBlob("0_hvac_weekly", reinterpret_cast<char *>(&local), sizeof(local));
  local.Program[0].Mode = SUPLA_HVAC_MODE_COOL;
  local.Program[0].SetpointTemperatureHeat = INT16_MIN;
  local.Program[0].SetpointTemperatureCool = 2700;
  cfg.setBlob("0_hvac_aweekly", reinterpret_cast<char *>(&local),
              sizeof(local));
  OutputSimulator output;
  Supla::Control::HvacBase hvac(&output);
  hvac.onLoadConfig(nullptr);
  hvac.onInit();
  uint32_t storedTypes = 0;
  ASSERT_TRUE(cfg.getUInt32("0_cfg_chng_t", &storedTypes));
  EXPECT_EQ(storedTypes, pendingTypes);
  if (GetParam() < 0) {
    uint8_t legacyChanged = 1;
    ASSERT_TRUE(cfg.getUInt8("0_weekly_chng", &legacyChanged));
    EXPECT_EQ(legacyChanged, 0);
  }
  ASSERT_EQ(hvac.getProgramById(1, true).SetpointTemperatureCool, 2700);
  ASSERT_EQ(hvac.getProgramById(1, false).SetpointTemperatureHeat, 2100);
  for (bool alt : {false, true}) {
    TSD_ChannelConfig remote = {};
    remote.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
    remote.ConfigType = alt ? SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE
                            : SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
    remote.ConfigSize = sizeof(local);
    local.Program[0].Mode = alt ? SUPLA_HVAC_MODE_COOL : SUPLA_HVAC_MODE_HEAT;
    local.Program[0].SetpointTemperatureHeat = alt ? INT16_MIN : 2200;
    local.Program[0].SetpointTemperatureCool = alt ? 3000 : INT16_MIN;
    memcpy(remote.Config, &local, sizeof(local));
    ASSERT_EQ(hvac.handleWeeklySchedule(&remote, alt, false),
              SUPLA_CONFIG_RESULT_TRUE);
    const bool pending = pendingTypes & (1 << remote.ConfigType);
    if (alt) {
      EXPECT_EQ(hvac.getProgramById(1, true).SetpointTemperatureCool,
                pending ? 2700 : 3000);
    } else {
      EXPECT_EQ(hvac.getProgramById(1, false).SetpointTemperatureHeat,
                pending ? 2100 : 2200);
    }
  }
}

class LegacyStateCapture : public Supla::StateStorageInterface {
 public:
  LegacyStateCapture() : StateStorageInterface(nullptr, 0) {}
  void initSectionPreamble(Supla::SectionPreamble *) override {}
  bool writeSectionPreamble() override { return true; }
  bool initFromStorage() override { return true; }
  void deleteAll() override {}
  bool prepareSaveState() override { return true; }
  bool prepareSizeCheck() override { return true; }
  bool prepareLoadState() override { return true; }
  bool readState(unsigned char *out, int size) override {
    if (pos + size > data.size()) {
      return false;
    }
    memcpy(out, data.data() + pos, size);
    pos += size;
    return true;
  }
  bool writeState(const unsigned char *in, int size) override {
    data.insert(data.end(), in, in + size);
    sizes.push_back(size);
    return true;
  }
  bool finalizeSaveState() override { return true; }
  bool finalizeSizeCheck() override { return true; }
  bool finalizeLoadState() override { return true; }
  std::vector<unsigned char> data;
  std::vector<int> sizes;
  size_t pos = 0;
};
class LegacyStorageCapture : public Supla::Storage {
 public:
  LegacyStorageCapture() {
    capture = new LegacyStateCapture;
    stateStorage = capture;
  }
  int readStorage(unsigned int, unsigned char *, unsigned int, bool) override {
    return 0;
  }
  int writeStorage(unsigned int, const unsigned char *, unsigned int) override {
    return 0;
  }
  void commit() override {}
  LegacyStateCapture *capture;
};
TEST(LegacyChannelCompatibilityTests, SerializeLegacyRecords) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  LegacyStorageCapture storage;
  OutputSimulator output;
  Supla::Control::HvacBase hvac(&output);
  Supla::Control::Relay relay(-1);
  Supla::Control::ActionTrigger at;
  hvac.onSaveState();
  ASSERT_EQ(storage.capture->data.size(), 21 + sizeof(time_t));
  relay.onSaveState();
  ASSERT_EQ(storage.capture->data.size(), 26 + sizeof(time_t));
  at.onSaveState();
  ASSERT_EQ(storage.capture->data.size(), 26 + sizeof(time_t));
  at.enableStateStorage();
  at.onSaveState();
  EXPECT_THAT(storage.capture->sizes,
              ::testing::ElementsAre(8, 8, sizeof(time_t), 2, 2, 1, 4, 1, 4));

  // Legacy layout: two HVAC values, timer, two unset setpoints, manual mode,
  // then the Relay duration/flags and the opt-in AT active-actions word.
  std::vector<unsigned char> expected(16, 0);
  const time_t timer = 1;
  const auto *timerBytes = reinterpret_cast<const unsigned char *>(&timer);
  expected.insert(expected.end(), timerBytes, timerBytes + sizeof(timer));
  const int16_t unsetSetpoints[] = {INT16_MIN, INT16_MIN};
  const auto *setpoints =
      reinterpret_cast<const unsigned char *>(unsetSetpoints);
  expected.insert(expected.end(), setpoints,
                  setpoints + sizeof(unsetSetpoints));
  expected.resize(expected.size() + 10, 0);
  EXPECT_EQ(storage.capture->data, expected);
}

TEST(LegacyChannelCompatibilityTests,
     LegacyRelayLocalServerAndTimerWithoutModes) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ConfigSimulator cfg;
  cfg.setChannelFunction(0, SUPLA_CHANNELFNC_POWERSWITCH);
  testing::NiceMock<SuplaIoMock> io;
  int physicalState = 0;
  ON_CALL(io, customDigitalWrite(_, _, _)).WillByDefault(
      [&](int, uint8_t, uint8_t value) { physicalState = value; });
  ON_CALL(io, customDigitalRead(_, _)).WillByDefault(
      [&](int, uint8_t) { return physicalState; });
  Supla::Control::Relay relay(&io, 5);
  relay.onLoadConfig(nullptr);
  relay.onInit();
  ASSERT_FALSE(relay.isOn());
  relay.handleAction(Supla::ON_PRESS, Supla::TURN_ON);
  ASSERT_TRUE(relay.isOn());
  relay.handleAction(Supla::ON_PRESS, Supla::TOGGLE);
  ASSERT_FALSE(relay.isOn());
  TSD_SuplaChannelNewValue command = {};
  command.value[0] = 1;
  command.DurationMS = 1000;
  ASSERT_EQ(relay.handleNewValueFromServer(&command), 1);
  ASSERT_TRUE(relay.isOn());
  time.advance(999);
  relay.iterateAlways();
  ASSERT_TRUE(relay.isOn());
  time.advance(2);
  relay.iterateAlways();
  ASSERT_FALSE(relay.isOn());
  command.DurationMS = 0;
  ASSERT_EQ(relay.handleNewValueFromServer(&command), 1);
  time.advance(60000);
  relay.iterateAlways();
  ASSERT_TRUE(relay.isOn());
  command.value[0] = 0;
  ASSERT_EQ(relay.handleNewValueFromServer(&command), 1);
  ASSERT_FALSE(relay.isOn());
}

class LegacyActionCounter : public Supla::ActionHandler {
 public:
  void handleAction(int, int action) override {
    if (action == Supla::TOGGLE) {
      ++toggles;
    }
    if (action == Supla::TOGGLE_CONFIG_MODE) {
      ++cfgs;
    }
  }
  int toggles = 0;
  int cfgs = 0;
};

TEST(LegacyChannelCompatibilityTests,
     LegacyAtCloudSelectionPreservesAlwaysEnabled) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  ConfigSimulator cfg;
  Supla::Control::Button button(-1);
  Supla::Control::ActionTrigger at;
  LegacyActionCounter counter;
  button.addAction(Supla::TOGGLE, counter, Supla::ON_PRESS);
  button.addAction(Supla::TOGGLE_CONFIG_MODE, counter, Supla::ON_HOLD, true);
  at.attach(button);
  at.onLoadConfig(nullptr);
  at.onInit();
  button.runAction(Supla::ON_PRESS);
  button.runAction(Supla::ON_HOLD);
  ASSERT_EQ(counter.toggles, 1);
  ASSERT_EQ(counter.cfgs, 1);
  TSD_ChannelConfig config = {};
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  TChannelConfig_ActionTrigger data = {};
  data.ActiveActions = SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_SHORT_PRESS_x2;
  memcpy(config.Config, &data, sizeof(data));
  at.handleChannelConfig(&config, false);
  button.runAction(Supla::ON_PRESS);
  button.runAction(Supla::ON_CLICK_1);
  button.runAction(Supla::ON_HOLD);
  EXPECT_EQ(counter.toggles, 2);
  EXPECT_EQ(counter.cfgs, 2);
}

}  // namespace
