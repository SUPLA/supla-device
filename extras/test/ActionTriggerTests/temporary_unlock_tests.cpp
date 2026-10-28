// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <SuplaDevice.h>
#include <arduino_mock.h>
#include <clock_stub.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <storage_mock.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <supla/control/weekly_schedule_buffer.h>
#include <supla/device/register_device.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/storage/key_value.h>
#include <array>
#include <cstring>
#include <vector>

namespace {
using Supla::Control::ActionTrigger;
class MemoryConfig : public Supla::KeyValue {
 public:
  bool init() override { return true; }
  bool commitWithResult() override { ++commits; return !fail; }
  int commits = 0;
  bool fail = false;
};
class DurableMemoryConfig : public MemoryConfig {
 public:
  bool commitWithResult() override {
    if (!MemoryConfig::commitWithResult()) return false;
    std::array<uint8_t, 4096> bytes = {};
    const size_t size = serializeToMemory(bytes.data(), bytes.size());
    if (size == SIZE_MAX) return false;
    disk.assign(bytes.begin(), bytes.begin() + size);
    return true;
  }
  bool setUInt32(const char *key, uint32_t value) override {
    return !(failMarkerWrite && strstr(key, "cfg_chng_t")) &&
        !(failActionsWrite && strstr(key, "at_actions")) &&
        MemoryConfig::setUInt32(key, value);
  }
  void reboot() {
    removeAllMemory();
    ASSERT_TRUE(initFromMemory(disk.data(), disk.size()));
  }
  bool failMarkerWrite = false;
  bool failActionsWrite = false;
  std::vector<uint8_t> disk;
};
class InspectableTrigger : public ActionTrigger {
 public:
  bool hasTemporaryState() const { return temporaryUnlock != nullptr; }
  bool hasPendingLocalConfig() const {
    return isLocalChannelConfigChangePending(SUPLA_CONFIG_TYPE_DEFAULT);
  }
  void acknowledgeLocalConfig() {
    setChannelConfigState(Supla::ChannelConfigState::LocalChangeSent);
    TSDS_SetChannelConfigResult result = {};
    result.Result = SUPLA_CONFIG_RESULT_TRUE;
    result.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
    handleSetChannelConfigResult(&result);
  }
  void publishAll() {
    actionHandlingType = Supla::ActionHandlingType_PublishAllDisableAll;
  }
};
class PhysicalButton : public Supla::Control::Button {
 public:
  PhysicalButton() : Button(10) {}
  bool locked() const { return runtimeFlags.actionTriggerModeLocked; }
  void dispatch() { runActionWithActionTriggerPolicy(Supla::ON_PRESS); }
};
class Counter : public Supla::ActionHandler {
 public:
  void handleAction(int, int) override { ++calls; }
  int calls = 0;
};
class TemporaryUnlock : public testing::Test {
 protected:
  SuplaDeviceClass sd;
  SimpleTime time;
  void SetUp() override { Supla::Channel::resetToDefaults(); time.advance(1); }
  void TearDown() override { Supla::Channel::resetToDefaults(); }
  uint8_t configure(ActionTrigger &at, uint32_t duration = 5,
                    uint8_t edge = 0, int sensor = -1,
                    uint32_t active = SUPLA_ACTION_CAP_ROTATE_LEFT) {
    TChannelConfig_ActionTrigger wire = {};
    wire.ActiveActions = active;
    wire.TemporaryUnlockDurationSec = duration;
    wire.TemporaryUnlockSensorEdge = edge;
    wire.TemporaryUnlockSensorIsSet = sensor >= 0;
    wire.TemporaryUnlockSensorChannelNo = sensor >= 0 ? sensor : 0;
    TSD_ChannelConfig request = {};
    request.ConfigSize = sizeof(wire);
    memcpy(request.Config, &wire, sizeof(wire));
    return at.handleChannelConfig(&request);
  }
  int command(ActionTrigger &at, uint8_t mode, uint32_t duration = 0) {
    TSD_SuplaChannelNewValue request = {};
    reinterpret_cast<TActionTriggerProperties *>(request.value)->ButtonMode =
        mode;
    request.DurationSec = duration;
    return at.handleNewValueFromServer(&request);
  }
  const TActionTriggerProperties &properties(ActionTrigger &at) {
    return *reinterpret_cast<const TActionTriggerProperties *>(
        Supla::RegisterDevice::getChannelValuePtr(at.getChannelNumber()));
  }
};

TEST_F(TemporaryUnlock,
       ManualTimerReportsModeSixAndRestartsWithoutFlashWrites) {
  MemoryConfig cfg;
  ActionTrigger at;
  ASSERT_EQ(configure(at), SUPLA_CONFIG_RESULT_TRUE);
  const int commits = cfg.commits;
  ASSERT_EQ(command(at, 6), 1);
  EXPECT_TRUE(at.isEffectivelyLocked());
  ASSERT_EQ(command(at, 7), 1);
  EXPECT_EQ(properties(at).ButtonMode, 6);
  EXPECT_TRUE(properties(at).Flags &
              SUPLA_ACTION_TRIGGER_FLAG_TEMPORARY_UNLOCK_ACTIVE);
  time.advance(4000);
  ASSERT_EQ(command(at, 7), 1);
  time.advance(4000);
  at.iterateAlways();
  EXPECT_FALSE(at.isEffectivelyLocked());
  time.advance(1000);
  at.iterateAlways();
  EXPECT_TRUE(at.isEffectivelyLocked());
  EXPECT_EQ(properties(at).ButtonMode, 6);
  EXPECT_FALSE(properties(at).Flags &
               SUPLA_ACTION_TRIGGER_FLAG_TEMPORARY_UNLOCK_ACTIVE);
  EXPECT_EQ(cfg.commits, commits);
}

TEST_F(TemporaryUnlock, LimitsModeExitAndMonotonicWrap) {
  ActionTrigger at;
  EXPECT_EQ(configure(at, 64801), SUPLA_CONFIG_RESULT_DATA_ERROR);
  ASSERT_EQ(configure(at, 64800), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(command(at, 7), 0);
  ASSERT_EQ(command(at, 6), 1);
  EXPECT_EQ(command(at, 7, 64801), 0);
  time.value = UINT32_MAX - 1000;
  ASSERT_EQ(command(at, 7), 1);
  time.advance(64799999);
  EXPECT_TRUE(at.isTemporaryUnlockActive());
  time.advance(1);
  at.iterateAlways();
  EXPECT_TRUE(at.isEffectivelyLocked());
  ASSERT_EQ(command(at, 7, 1), 1);
  ASSERT_EQ(command(at, 1), 1);
  EXPECT_FALSE(at.isTemporaryUnlockActive());
  ASSERT_EQ(command(at, 6), 1);
  EXPECT_TRUE(at.isEffectivelyLocked());
}

TEST_F(TemporaryUnlock, SourceZeroBaselineOfflineAndActiveEdges) {
  Supla::Sensor::VirtualBinary sensor;
  ActionTrigger at;
  ASSERT_EQ(sensor.getChannelNumber(), 0);
  ASSERT_EQ(configure(at, 5, SUPLA_AT_SENSOR_EDGE_BOTH, 0),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(command(at, 6), 1);
  sensor.getChannel()->setNewValue(true);
  at.iterateAlways();  // First true is a baseline, not a rising edge.
  EXPECT_FALSE(at.isTemporaryUnlockActive());
  sensor.getChannel()->setNewValue(false);
  at.iterateAlways();
  EXPECT_TRUE(at.isTemporaryUnlockActive());
  time.advance(4000);
  sensor.getChannel()->setNewValue(true);
  at.iterateAlways();  // Must not extend the timer.
  time.advance(1000);
  at.iterateAlways();
  EXPECT_TRUE(at.isEffectivelyLocked());
  sensor.getChannel()->setStateOffline();
  at.iterateAlways();
  sensor.getChannel()->setNewValue(false);
  sensor.getChannel()->setStateOnline();
  at.iterateAlways();  // Recovery is also a baseline.
  EXPECT_FALSE(at.isTemporaryUnlockActive());
  sensor.getChannel()->setNewValue(true);
  at.iterateAlways();
  EXPECT_TRUE(at.isTemporaryUnlockActive());
}

TEST_F(TemporaryUnlock, ConfigChangesWithoutActionChangesAndLegacyPrefix) {
  MemoryConfig cfg;
  ActionTrigger at;
  ASSERT_EQ(configure(at, 5), SUPLA_CONFIG_RESULT_TRUE);
  const int commits = cfg.commits;
  ASSERT_EQ(configure(at, 5), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(cfg.commits, commits);
  ASSERT_EQ(configure(at, 12), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(cfg.commits, commits + 1);
  TSD_ChannelConfig legacy = {};
  legacy.ConfigSize = 4;
  uint32_t actions = SUPLA_ACTION_CAP_ROTATE_RIGHT;
  memcpy(legacy.Config, &actions, 4);
  ASSERT_EQ(at.handleChannelConfig(&legacy), SUPLA_CONFIG_RESULT_TRUE);
  TChannelConfig_ActionTrigger result = {};
  int size = 0;
  at.fillChannelConfig(&result, &size, 0);
  EXPECT_EQ(size, 16);
  EXPECT_EQ(result.TemporaryUnlockDurationSec, 12);
  EXPECT_EQ(result.ActiveActions, actions);
  for (int invalid : {1, 3, 5, 15, 17}) {
    legacy.ConfigSize = invalid;
    EXPECT_EQ(at.handleChannelConfig(&legacy), SUPLA_CONFIG_RESULT_DATA_ERROR);
  }
}

TEST_F(TemporaryUnlock, RestartLoadsConfigButNeverTimer) {
  MemoryConfig cfg;
  {
    ActionTrigger at;
    ASSERT_EQ(configure(at, 19, SUPLA_AT_SENSOR_EDGE_RISING, 0),
              SUPLA_CONFIG_RESULT_TRUE);
    ASSERT_EQ(command(at, 6), 1);
    ASSERT_EQ(command(at, 7), 1);
  }
  Supla::Channel::resetToDefaults();
  ActionTrigger restored;
  restored.onLoadConfig(&sd);
  EXPECT_FALSE(restored.isTemporaryUnlockActive());
  TChannelConfig_ActionTrigger wire = {};
  int size = 0;
  restored.fillChannelConfig(&wire, &size, 0);
  EXPECT_EQ(wire.TemporaryUnlockDurationSec, 19);
  EXPECT_EQ(wire.TemporaryUnlockSensorIsSet, 1);
  EXPECT_EQ(wire.TemporaryUnlockSensorChannelNo, 0);
}

TEST_F(TemporaryUnlock, ButtonAndBaseDispatchUseEffectiveLock) {
  testing::NiceMock<DigitalInterfaceMock> io;
  ActionTrigger at;
  PhysicalButton button;
  Counter counter;
  button.addAction(Supla::TURN_ON, counter, Supla::ON_PRESS, true);
  at.attach(button);
  ASSERT_EQ(configure(at), SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(command(at, 6), 1);
  EXPECT_TRUE(button.locked());
  button.dispatch();
  EXPECT_EQ(counter.calls, 0);
  at.handleAction(0, Supla::SEND_AT_ROTATE_LEFT);
  EXPECT_EQ(static_cast<Supla::AtChannel *>(at.getChannel())->popAction(), 0);
  ASSERT_EQ(command(at, 7), 1);
  EXPECT_FALSE(button.locked());
  button.dispatch();
  EXPECT_EQ(counter.calls, 1);
  at.handleAction(0, Supla::SEND_AT_ROTATE_LEFT);
  EXPECT_EQ(static_cast<Supla::AtChannel *>(at.getChannel())->popAction(),
            SUPLA_ACTION_CAP_ROTATE_LEFT);
  time.advance(5000);
  // The button timer can run before ActionTrigger::iterateAlways().
  button.onTimer();
  EXPECT_TRUE(button.locked());
  at.iterateAlways();
  EXPECT_TRUE(button.locked());
}

TEST_F(TemporaryUnlock, WeeklyProgramOverridesDurationAndValidatesTimes) {
  ClockStub clock;
  time.advance(31000);
  ActionTrigger at;
  ASSERT_EQ(configure(at, 20), SUPLA_CONFIG_RESULT_TRUE);
  at.setWeeklyScheduleAvailable();
  TSD_ChannelConfig request = {};
  request.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  request.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  auto *schedule = reinterpret_cast<TChannelConfig_WeeklySchedule *>(
      request.Config);
  schedule->Program[0].Mode = 6;
  schedule->Program[0].ButtonTemporaryUnlockDurationS = 2;
  Supla::Control::WeeklyScheduleBuffer buffer;
  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; ++i)
    buffer.setWeeklySchedule(schedule, i, 1);
  ASSERT_EQ(at.handleWeeklySchedule(&request, false, false),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(command(at, 4), 1);
  EXPECT_EQ(properties(at).ButtonMode, 6);
  ASSERT_EQ(command(at, 7), 1);
  time.advance(2000);
  at.iterateAlways();
  EXPECT_TRUE(at.isEffectivelyLocked());
  EXPECT_TRUE(properties(at).Flags &
              SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  schedule->Program[0].ButtonTemporaryUnlockDurationS = 64801;
  EXPECT_EQ(at.handleWeeklySchedule(&request, false, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
}

TEST_F(TemporaryUnlock, EdgeSelectionAndChangedSensorWithSameActions) {
  Supla::Sensor::VirtualBinary sensor;
  ActionTrigger at;
  for (uint8_t edge : {SUPLA_AT_SENSOR_EDGE_RISING,
                       SUPLA_AT_SENSOR_EDGE_FALLING,
                       SUPLA_AT_SENSOR_EDGE_BOTH}) {
    ASSERT_EQ(command(at, 1), 1);
    ASSERT_EQ(configure(at, 1, edge, 0), SUPLA_CONFIG_RESULT_TRUE);
    ASSERT_EQ(command(at, 6), 1);
    sensor.getChannel()->setNewValue(false);
    at.iterateAlways();
    sensor.getChannel()->setNewValue(true);
    at.iterateAlways();
    EXPECT_EQ(at.isTemporaryUnlockActive(),
              edge != SUPLA_AT_SENSOR_EDGE_FALLING);
    time.advance(1000);
    at.iterateAlways();
    at.iterateAlways();  // Establish a fresh baseline after timer expiry.
    sensor.getChannel()->setNewValue(false);
    at.iterateAlways();
    EXPECT_EQ(at.isTemporaryUnlockActive(),
              edge != SUPLA_AT_SENSOR_EDGE_RISING);
  }
}

TEST_F(TemporaryUnlock, FailedPersistenceDoesNotApplyCandidate) {
  MemoryConfig cfg;
  ActionTrigger at;
  ASSERT_EQ(configure(at, 5), SUPLA_CONFIG_RESULT_TRUE);
  cfg.fail = true;
  EXPECT_EQ(configure(at, 11), SUPLA_CONFIG_RESULT_DATA_ERROR);
  TChannelConfig_ActionTrigger wire = {};
  int size = 0;
  at.fillChannelConfig(&wire, &size, 0);
  EXPECT_EQ(wire.TemporaryUnlockDurationSec, 5);
  cfg.fail = false;
  EXPECT_EQ(configure(at, 11), SUPLA_CONFIG_RESULT_TRUE);
}

TEST_F(TemporaryUnlock, ModeSixStateRestoresLockedWithoutTimerOrExtraBytes) {
  StorageMock storage;
  storage.defaultInitialization(4);
  ActionTrigger at;
  at.enableStateStorage();
  EXPECT_CALL(storage, scheduleSave(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ASSERT_EQ(command(at, 6), 1);
  ASSERT_EQ(command(at, 7, 60), 1);
  uint32_t saved = 0;
  EXPECT_CALL(storage, readStorage(testing::_, testing::_, 4, testing::_))
      .WillRepeatedly([&](uint32_t, unsigned char *data, uint32_t, bool) {
        memcpy(data, &saved, 4);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(testing::_, testing::_, 4))
      .WillOnce([&](uint32_t, const unsigned char *data, uint32_t) {
        memcpy(&saved, data, 4);
        return 4;
      });
  at.onSaveState();
  EXPECT_EQ(saved, 0xC0000000u);
  EXPECT_CALL(storage, readStorage(testing::_, testing::_, 4, testing::_))
      .WillOnce([&](uint32_t, unsigned char *data, uint32_t, bool) {
        memcpy(data, &saved, 4);
        return 4;
      });
  at.onLoadState();
  EXPECT_EQ(properties(at).ButtonMode, 6);
  EXPECT_TRUE(at.isEffectivelyLocked());
  EXPECT_FALSE(at.isTemporaryUnlockActive());
}

TEST_F(TemporaryUnlock, TimerDoesNotScheduleStateWritesEvenWhenStorageEnabled) {
  StorageMock storage;
  storage.defaultInitialization(4);
  ActionTrigger at;
  EXPECT_CALL(storage, scheduleSave(testing::_, testing::_))
      .Times(testing::AnyNumber());
  ASSERT_EQ(configure(at), SUPLA_CONFIG_RESULT_TRUE);
  at.enableStateStorage();
  ASSERT_EQ(command(at, 6), 1);
  testing::Mock::VerifyAndClearExpectations(&storage);
  EXPECT_CALL(storage, scheduleSave(testing::_, testing::_)).Times(0);
  ASSERT_EQ(command(at, 7), 1);
  time.advance(2000);
  ASSERT_EQ(command(at, 7), 1);
  time.advance(5000);
  at.iterateAlways();
  EXPECT_TRUE(at.isEffectivelyLocked());
}

TEST_F(TemporaryUnlock, LegacyAllActionsSurviveModeSixStateRestart) {
  MemoryConfig cfg;
  StorageMock storage;
  storage.defaultInitialization(4);
  uint32_t saved = 0;
  EXPECT_CALL(storage, readStorage(testing::_, testing::_, 4, testing::_))
      .WillRepeatedly([&](uint32_t, unsigned char *data, uint32_t, bool) {
        memcpy(data, &saved, 4);
        return 4;
      });
  EXPECT_CALL(storage, scheduleSave(testing::_, testing::_))
      .Times(testing::AnyNumber());
  {
    ActionTrigger at;
    at.enableStateStorage();
    TSD_ChannelConfig legacy = {};
    legacy.ConfigSize = 4;
    const uint32_t allActions = 0xFFFFFFFFUL;
    memcpy(legacy.Config, &allActions, 4);
    ASSERT_EQ(at.handleChannelConfig(&legacy), SUPLA_CONFIG_RESULT_TRUE);
    ASSERT_EQ(command(at, 6), 1);
    EXPECT_CALL(storage, writeStorage(testing::_, testing::_, 4))
        .WillOnce([&](uint32_t, const unsigned char *data, uint32_t) {
          memcpy(&saved, data, 4);
          return 4;
        });
    at.onSaveState();
  }
  Supla::Channel::resetToDefaults();
  ActionTrigger restored;
  restored.enableStateStorage();
  restored.onLoadConfig(&sd);
  restored.onLoadState();
  TChannelConfig_ActionTrigger wire = {};
  int size = 0;
  restored.fillChannelConfig(&wire, &size, 0);
  EXPECT_EQ(wire.ActiveActions, 0xFFFFFFFFUL);
  EXPECT_EQ(properties(restored).ButtonMode, 6);
  EXPECT_TRUE(restored.isEffectivelyLocked());
}

TEST_F(TemporaryUnlock, RuntimeAllocationIsDeferredUntilNeeded) {
  InspectableTrigger at;
  EXPECT_FALSE(at.hasTemporaryState());
  ASSERT_EQ(configure(at, 0, SUPLA_AT_SENSOR_EDGE_NONE),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_FALSE(at.hasTemporaryState());
  ASSERT_EQ(command(at, 6), 1);
  EXPECT_TRUE(at.hasTemporaryState());
}

TEST_F(TemporaryUnlock, PublishAllDoesNotOverwriteCloudActiveActionsOnFill) {
  InspectableTrigger at;
  at.publishAll();
  ASSERT_EQ(configure(at, 5, 0, -1, SUPLA_ACTION_CAP_ROTATE_LEFT),
            SUPLA_CONFIG_RESULT_TRUE);
  TChannelConfig_ActionTrigger wire = {};
  int size = 0;
  at.fillChannelConfig(&wire, &size, 0);
  EXPECT_EQ(size, 16);
  EXPECT_EQ(wire.ActiveActions, SUPLA_ACTION_CAP_ROTATE_LEFT);
}

TEST_F(TemporaryUnlock, LocalChangeIsProtectedUntilCloudAcknowledgesIt) {
  MemoryConfig cfg;
  ActionTrigger at;
  TChannelConfig_ActionTrigger wire = {};
  wire.TemporaryUnlockDurationSec = 17;
  wire.ActiveActions = SUPLA_ACTION_CAP_ROTATE_LEFT;
  TSD_ChannelConfig request = {};
  request.ConfigSize = sizeof(wire);
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(at.handleChannelConfig(&request, true), SUPLA_CONFIG_RESULT_TRUE);
  wire.TemporaryUnlockDurationSec = 31;
  wire.ActiveActions = SUPLA_ACTION_CAP_ROTATE_RIGHT;
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(at.handleChannelConfig(&request, false), SUPLA_CONFIG_RESULT_TRUE);
  int size = 0;
  at.fillChannelConfig(&wire, &size, 0);
  EXPECT_EQ(wire.TemporaryUnlockDurationSec, 17u);
  EXPECT_EQ(wire.ActiveActions, SUPLA_ACTION_CAP_ROTATE_LEFT);
}

TEST_F(TemporaryUnlock, EdgeBeforeExpiryCannotReviveExpiredTimer) {
  Supla::Sensor::VirtualBinary sensor;
  ActionTrigger at;
  ASSERT_EQ(configure(at, 5, SUPLA_AT_SENSOR_EDGE_BOTH, 0),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(command(at, 6), 1);
  sensor.getChannel()->setNewValue(false);
  at.iterateAlways();
  sensor.getChannel()->setNewValue(true);
  at.iterateAlways();
  ASSERT_TRUE(at.isTemporaryUnlockActive());
  time.advance(4999);
  sensor.getChannel()->setNewValue(false);  // Before the deadline.
  time.advance(2);  // No AT iteration occurred between the edge and expiry.
  // Another event may notice expiry before the sensor is sampled.
  at.handleAction(0, Supla::SEND_AT_ROTATE_LEFT);
  at.iterateAlways();
  EXPECT_FALSE(at.isTemporaryUnlockActive());
  EXPECT_TRUE(at.isEffectivelyLocked());
}

TEST_F(TemporaryUnlock, UnavailableOrUpdatingSensorCannotUnlock) {
  Supla::Sensor::VirtualBinary sensor;
  ActionTrigger at;
  ASSERT_EQ(configure(at, 5, SUPLA_AT_SENSOR_EDGE_RISING, 0),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(command(at, 6), 1);
  for (bool updating : {false, true}) {
    sensor.getChannel()->setStateOnline();
    sensor.getChannel()->setNewValue(false);
    at.iterateAlways();
    if (updating) {
      sensor.getChannel()->setStateFirmwareUpdateOngoing();
    } else {
      sensor.getChannel()->setStateOnlineAndNotAvailable();
    }
    sensor.getChannel()->setNewValue(true);
    at.iterateAlways();
    EXPECT_FALSE(at.isTemporaryUnlockActive());
    ASSERT_EQ(command(at, 1), 1);
    ASSERT_EQ(command(at, 6), 1);
  }
}

TEST_F(TemporaryUnlock, RecoveryFromUnavailableOrUpdatingSensorIsBaseline) {
  Supla::Sensor::VirtualBinary sensor;
  ActionTrigger at;
  ASSERT_EQ(configure(at, 5, SUPLA_AT_SENSOR_EDGE_RISING, 0),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(command(at, 6), 1);
  for (bool updating : {false, true}) {
    sensor.getChannel()->setStateOnline();
    sensor.getChannel()->setNewValue(false);
    at.iterateAlways();
    if (updating) {
      sensor.getChannel()->setStateFirmwareUpdateOngoing();
    } else {
      sensor.getChannel()->setStateOnlineAndNotAvailable();
    }
    at.iterateAlways();
    sensor.getChannel()->setNewValue(true);
    sensor.getChannel()->setStateOnline();
    at.iterateAlways();
    EXPECT_FALSE(at.isTemporaryUnlockActive());
    sensor.getChannel()->setNewValue(false);
    at.iterateAlways();
    sensor.getChannel()->setNewValue(true);
    at.iterateAlways();
    EXPECT_TRUE(at.isTemporaryUnlockActive());
    ASSERT_EQ(command(at, 1), 1);
    ASSERT_EQ(command(at, 6), 1);
  }
}

TEST_F(TemporaryUnlock, RepeatedModeSixPreservesOriginalDeadline) {
  for (bool weeklyAvailable : {false, true}) {
    ActionTrigger at;
    at.setWeeklyScheduleAvailable(weeklyAvailable);
    ASSERT_EQ(configure(at), SUPLA_CONFIG_RESULT_TRUE);
    ASSERT_EQ(command(at, 6), 1);
    ASSERT_EQ(command(at, 7), 1);
    time.advance(1000);
    ASSERT_EQ(command(at, 6), 1);
    EXPECT_TRUE(at.isTemporaryUnlockActive());
    EXPECT_EQ(properties(at).ButtonMode, 6);
    EXPECT_TRUE(properties(at).Flags &
                SUPLA_ACTION_TRIGGER_FLAG_TEMPORARY_UNLOCK_ACTIVE);
    time.advance(3999);
    at.iterateAlways();
    EXPECT_TRUE(at.isTemporaryUnlockActive());
    time.advance(1);
    at.iterateAlways();
    EXPECT_TRUE(at.isEffectivelyLocked());
  }
}

TEST_F(TemporaryUnlock, LeavingWeeklyModeSixForManualKeepsCurrentTimer) {
  ClockStub clock;
  time.advance(31000);
  ActionTrigger at;
  ASSERT_EQ(configure(at, 20), SUPLA_CONFIG_RESULT_TRUE);
  at.setWeeklyScheduleAvailable();
  TSD_ChannelConfig request = {};
  request.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  request.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  auto *schedule = reinterpret_cast<TChannelConfig_WeeklySchedule *>(
      request.Config);
  schedule->Program[0].Mode = 6;
  schedule->Program[0].ButtonTemporaryUnlockDurationS = 2;
  Supla::Control::WeeklyScheduleBuffer buffer;
  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; ++i)
    buffer.setWeeklySchedule(schedule, i, 1);
  ASSERT_EQ(at.handleWeeklySchedule(&request, false, false),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(command(at, 4), 1);
  ASSERT_EQ(command(at, 7), 1);
  time.advance(500);
  ASSERT_EQ(command(at, 6), 1);
  EXPECT_FALSE(properties(at).Flags &
               SUPLA_ACTION_TRIGGER_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_TRUE(at.isTemporaryUnlockActive());
  time.advance(1499);
  at.iterateAlways();
  EXPECT_TRUE(at.isTemporaryUnlockActive());
  time.advance(1);
  at.iterateAlways();
  EXPECT_TRUE(at.isEffectivelyLocked());
  // New manual unlocks use the default duration, not the former program.
  ASSERT_EQ(command(at, 7), 1);
  time.advance(2000);
  at.iterateAlways();
  EXPECT_TRUE(at.isTemporaryUnlockActive());
  time.advance(18000);
  at.iterateAlways();
  EXPECT_TRUE(at.isEffectivelyLocked());
}

TEST_F(TemporaryUnlock, LeavingModeSixStillCancelsTimerWithWeeklySupport) {
  ActionTrigger at;
  at.setWeeklyScheduleAvailable();
  ASSERT_EQ(configure(at), SUPLA_CONFIG_RESULT_TRUE);
  for (uint8_t mode : {0, 1, 5}) {
    ASSERT_EQ(command(at, 6), 1);
    ASSERT_EQ(command(at, 7), 1);
    ASSERT_EQ(command(at, mode), 1);
    EXPECT_FALSE(at.isTemporaryUnlockActive());
    EXPECT_FALSE(properties(at).Flags &
                 SUPLA_ACTION_TRIGGER_FLAG_TEMPORARY_UNLOCK_ACTIVE);
  }
}

TEST_F(TemporaryUnlock, LocalConfigAndPendingMarkerSurviveStorageReboot) {
  DurableMemoryConfig cfg;
  TChannelConfig_ActionTrigger wire = {};
  wire.TemporaryUnlockDurationSec = 17;
  TSD_ChannelConfig request = {};
  request.ConfigSize = sizeof(wire);
  memcpy(request.Config, &wire, sizeof(wire));
  {
    InspectableTrigger at;
    ASSERT_EQ(at.handleChannelConfig(&request, true), SUPLA_CONFIG_RESULT_TRUE);
    EXPECT_TRUE(at.hasPendingLocalConfig());
  }
  cfg.reboot();  // No delayed save or cloud acknowledgement took place.
  Supla::Channel::resetToDefaults();
  InspectableTrigger restored;
  restored.onLoadConfig(&sd);
  EXPECT_TRUE(restored.hasPendingLocalConfig());
  wire.TemporaryUnlockDurationSec = 31;
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(restored.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  int size = 0;
  restored.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(wire.TemporaryUnlockDurationSec, 17u);

  restored.acknowledgeLocalConfig();
  EXPECT_FALSE(restored.hasPendingLocalConfig());
  ASSERT_TRUE(cfg.commitWithResult());
  cfg.reboot();
  restored.onLoadConfig(&sd);
  EXPECT_FALSE(restored.hasPendingLocalConfig());
  ASSERT_EQ(restored.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  restored.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(wire.TemporaryUnlockDurationSec, 31u);
}

TEST_F(TemporaryUnlock, FailedLocalSaveDoesNotApplySettingsOrPendingMarker) {
  for (int failure : {0, 1, 2}) {
    DurableMemoryConfig cfg;
    InspectableTrigger at;
    ASSERT_EQ(configure(at, 5), SUPLA_CONFIG_RESULT_TRUE);
    TChannelConfig_ActionTrigger wire = {};
    wire.TemporaryUnlockDurationSec = 17;
    TSD_ChannelConfig request = {};
    request.ConfigSize = sizeof(wire);
    memcpy(request.Config, &wire, sizeof(wire));
    cfg.fail = failure == 0;
    cfg.failMarkerWrite = failure == 1;
    cfg.failActionsWrite = failure == 2;
    EXPECT_EQ(at.handleChannelConfig(&request, true),
              SUPLA_CONFIG_RESULT_DATA_ERROR);
    EXPECT_FALSE(at.hasPendingLocalConfig());
    int size = 0;
    at.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
    EXPECT_EQ(wire.TemporaryUnlockDurationSec, 5u);
    cfg.fail = false;
    cfg.failMarkerWrite = false;
    cfg.failActionsWrite = false;
    cfg.reboot();
    at.onLoadConfig(&sd);
    EXPECT_FALSE(at.hasPendingLocalConfig());
    at.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
    EXPECT_EQ(wire.TemporaryUnlockDurationSec, 5u);
    EXPECT_EQ(at.handleChannelConfig(&request, true), SUPLA_CONFIG_RESULT_TRUE);
    EXPECT_TRUE(at.hasPendingLocalConfig());
  }
}

TEST_F(TemporaryUnlock, LocalActionsOnlyChangePersistsPendingMarkerOnce) {
  DurableMemoryConfig cfg;
  {
    InspectableTrigger at;
    ASSERT_EQ(configure(at), SUPLA_CONFIG_RESULT_TRUE);
    TChannelConfig_ActionTrigger wire = {};
    wire.TemporaryUnlockDurationSec = 5;
    wire.ActiveActions = SUPLA_ACTION_CAP_ROTATE_RIGHT;
    TSD_ChannelConfig request = {};
    request.ConfigSize = sizeof(wire);
    memcpy(request.Config, &wire, sizeof(wire));
    const int commits = cfg.commits;
    ASSERT_EQ(at.handleChannelConfig(&request, true), SUPLA_CONFIG_RESULT_TRUE);
    EXPECT_EQ(cfg.commits, commits + 1);
    ASSERT_EQ(at.handleChannelConfig(&request, true), SUPLA_CONFIG_RESULT_TRUE);
    EXPECT_EQ(cfg.commits, commits + 1);
  }
  cfg.reboot();
  Supla::Channel::resetToDefaults();
  InspectableTrigger restored;
  restored.onLoadConfig(&sd);
  EXPECT_TRUE(restored.hasPendingLocalConfig());
  TChannelConfig_ActionTrigger wire = {};
  int size = 0;
  restored.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(wire.ActiveActions, SUPLA_ACTION_CAP_ROTATE_RIGHT);
}

TEST_F(TemporaryUnlock, PublishAllRestartsWithOriginalActionsAndPendingConfig) {
  for (bool publishAll : {false, true}) {
    SCOPED_TRACE(publishAll);
    DurableMemoryConfig cfg;
    StorageMock storage;
    storage.defaultInitialization(4);
    ASSERT_TRUE(cfg.setInt32("0_mqtt_at", publishAll ? 2 : 0));
    uint32_t saved = 0;
    EXPECT_CALL(storage, scheduleSave(testing::_, testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(storage, readStorage(testing::_, testing::_, 4, testing::_))
        .WillRepeatedly([&](uint32_t, unsigned char *data, uint32_t, bool) {
          memcpy(data, &saved, 4);
          return 4;
        });
    {
      InspectableTrigger at;
      at.enableStateStorage();
      at.onLoadConfig(&sd);
      TChannelConfig_ActionTrigger wire = {};
      wire.ActiveActions = SUPLA_ACTION_CAP_ROTATE_RIGHT;
      wire.TemporaryUnlockDurationSec = 17;
      TSD_ChannelConfig request = {};
      request.ConfigSize = sizeof(wire);
      memcpy(request.Config, &wire, sizeof(wire));
      ASSERT_EQ(at.handleChannelConfig(&request, true),
                SUPLA_CONFIG_RESULT_TRUE);
      EXPECT_CALL(storage, writeStorage(testing::_, testing::_, 4))
          .WillOnce([&](uint32_t, const unsigned char *data, uint32_t) {
            memcpy(&saved, data, 4);
            return 4;
          });
      at.onSaveState();
    }
    cfg.reboot();
    Supla::Channel::resetToDefaults();
    {
      InspectableTrigger restored;
      restored.enableStateStorage();
      restored.onLoadConfig(&sd);
      restored.onLoadState();
      restored.onInit();
      ASSERT_TRUE(restored.hasPendingLocalConfig());
      TChannelConfig_ActionTrigger wire = {};
      int size = 0;
      restored.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
      EXPECT_EQ(wire.ActiveActions, SUPLA_ACTION_CAP_ROTATE_RIGHT);
      EXPECT_EQ(wire.TemporaryUnlockDurationSec, 17u);
      restored.handleAction(0, Supla::SEND_AT_ROTATE_LEFT);
      auto *channel = static_cast<Supla::AtChannel *>(restored.getChannel());
      EXPECT_EQ(channel->popAction(),
                publishAll ? SUPLA_ACTION_CAP_ROTATE_LEFT : 0);
    }
    Supla::Channel::resetToDefaults();
  }
}

TEST_F(TemporaryUnlock, ServerActionsUpdateStoredSelectionAfterLocalAck) {
  DurableMemoryConfig cfg;
  {
    InspectableTrigger at;
    TChannelConfig_ActionTrigger wire = {};
    wire.ActiveActions = SUPLA_ACTION_CAP_ROTATE_RIGHT;
    wire.TemporaryUnlockDurationSec = 5;
    TSD_ChannelConfig request = {};
    request.ConfigSize = sizeof(wire);
    memcpy(request.Config, &wire, sizeof(wire));
    ASSERT_EQ(at.handleChannelConfig(&request, true),
              SUPLA_CONFIG_RESULT_TRUE);
    at.acknowledgeLocalConfig();
    wire.ActiveActions = SUPLA_ACTION_CAP_ROTATE_LEFT;
    memcpy(request.Config, &wire, sizeof(wire));
    ASSERT_EQ(at.handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
  }
  cfg.reboot();
  Supla::Channel::resetToDefaults();
  InspectableTrigger restored;
  restored.onLoadConfig(&sd);
  EXPECT_FALSE(restored.hasPendingLocalConfig());
  TChannelConfig_ActionTrigger wire = {};
  int size = 0;
  restored.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(wire.ActiveActions, SUPLA_ACTION_CAP_ROTATE_LEFT);
}
}  // namespace
