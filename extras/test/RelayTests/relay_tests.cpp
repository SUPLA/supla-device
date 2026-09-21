// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <arduino_mock.h>
#include <board_mock.h>
#include <clock_stub.h>
#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <protocol_layer_mock.h>
#include <simple_time.h>
#include <storage_mock.h>
#include <supla/actions.h>
#include <supla/channel.h>
#include <supla/clock/clock.h>
#include <supla/condition.h>
#include <supla/condition_getter.h>
#include <supla/control/light_relay.h>
#include <supla/control/relay.h>
#include <supla/control/relay_weekly_schedule.h>
#include <supla/control/weekly_schedule_buffer.h>
#include <supla/control/weekly_schedule_component.h>
#include <supla/events.h>
#include <supla/device/register_device.h>
#include <supla/io.h>
#include <supla_io_mock.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

namespace {

class CountingActionHandler : public Supla::ActionHandler {
 public:
  void handleAction(int event, int action) override {
    lastEvent = event;
    lastAction = action;
    count++;
  }

  int count = 0;
  int lastEvent = -1;
  int lastAction = -1;
};

class RelayWithCustomWeeklySchedule : public Supla::Control::Relay {
 public:
  explicit RelayWithCustomWeeklySchedule(int pin)
      : Supla::Control::Relay(pin) {
    setWeeklyScheduleAvailable();
  }

  bool hasWeeklyScheduleController() const {
    return weeklyScheduleComponents.getController() != nullptr;
  }

 protected:
  void fillDefaultWeeklySchedule(
      TChannelConfig_WeeklySchedule *schedule) override {
    schedule->Program[0].Mode = SUPLA_RELAY_MODE_FORCED_OFF;
    Supla::Control::WeeklyScheduleBuffer buffer;
    buffer.setWeeklySchedule(schedule, 0, 1);
  }
};

class RelayWeeklyScheduleProbe : public Supla::Control::Relay {
 public:
  using Supla::Control::Relay::Relay;

  bool hasWeeklyScheduleController() const {
    return weeklyScheduleComponents.getController() != nullptr;
  }
};

class TimedWeeklyRelay : public Supla::Control::Relay {
 public:
  explicit TimedWeeklyRelay(int pin) : Supla::Control::Relay(pin) {
    setWeeklyScheduleAvailable();
  }
  uint32_t timer() const { return durationMs; }
  uint32_t storedDuration() const { return storedTurnOnDurationMs; }
};

class RestrictedWeeklyModesRelay : public Supla::Control::Relay {
 public:
  RestrictedWeeklyModesRelay(int pin, uint8_t modes)
      : Supla::Control::Relay(pin), modes_(modes) {}

  bool isWeeklyScheduleProgramModeSupported(uint8_t mode) const override {
    return mode < 8 && (modes_ & (1 << mode));
  }

 private:
  uint8_t modes_ = 0;
};

class DeferredWeeklyRelay : public TimedWeeklyRelay {
 public:
  using TimedWeeklyRelay::TimedWeeklyRelay;

  bool scheduleOutputReady = false;
  int scheduleOutputAttempts = 0;

 protected:
  bool applyWeeklyScheduleState(bool on) override {
    scheduleOutputAttempts++;
    return scheduleOutputReady &&
           TimedWeeklyRelay::applyWeeklyScheduleState(on);
  }
};

class AdjustableWeeklyClock : public ClockStub {
 public:
  bool ready = true;
  bool isReady() override { return ready; }
  void shift(int seconds) { now += seconds; }
};

class CountingRelayWeeklySchedule : public Supla::Control::RelayWeeklySchedule {
 public:
  explicit CountingRelayWeeklySchedule(Supla::Control::Relay *owner)
      : RelayWeeklySchedule(owner) {}

  bool resolveProgramTiming(
      const Supla::Control::WeeklyScheduleTimeSnapshot &time, bool alt,
      int programId, int32_t *occurrence, uint32_t *elapsedSeconds) override {
    timingResolveCount++;
    return Supla::Control::NativeWeeklyScheduleConfigHandler::
        resolveProgramTiming(time, alt, programId, occurrence, elapsedSeconds);
  }

  int timingResolveCount = 0;
};

class RelayWithAutomaticWeeklySchedule : public Supla::Control::Relay {
 public:
  explicit RelayWithAutomaticWeeklySchedule(int pin)
      : Supla::Control::Relay(pin) {
    setWeeklyScheduleAvailable();
  }

  int automaticModeIterationCount = 0;

  bool isWeeklyScheduleActive() const {
    auto *controller = weeklyScheduleComponents.getController();
    return controller != nullptr && controller->isActive();
  }

 protected:
  void iterateAutomaticMode() override {
    automaticModeIterationCount++;
  }
};

}  // namespace

class RelayFixture : public testing::Test {
 public:
  ::testing::NiceMock<DigitalInterfaceMock> ioMock;
  StorageMock storage;
  SimpleTime time;
  ProtocolLayerMock protoMock;

  RelayFixture() {
  }

  virtual ~RelayFixture() {
  }

  void SetUp() {
    Supla::Channel::resetToDefaults();
    setLastResetSoft(false);
    EXPECT_CALL(storage, scheduleSave(_, 2000)).WillRepeatedly(Return());
  }

  void TearDown() {
    setLastResetSoft(false);
    Supla::Channel::resetToDefaults();
  }

  void sendConfig(Supla::Control::Relay *r, uint32_t func, uint32_t timeMs) {
    TSD_ChannelConfig result = {};
    result.Func = func;
    result.ConfigType = 0;
    if (func == SUPLA_CHANNELFNC_STAIRCASETIMER) {
      result.ConfigSize = sizeof(TChannelConfig_StaircaseTimer);
      TChannelConfig_StaircaseTimer *config =
          reinterpret_cast<TChannelConfig_StaircaseTimer *>(&result.Config);
      config->TimeMS = timeMs;
    }
    r->handleChannelConfig(&result, false);
  }

  TSD_ChannelConfig makeSingleProgramWeeklySchedule(uint32_t func,
                                                    uint8_t programMode) {
    TSD_ChannelConfig result = {};
    result.ChannelNumber = 0;
    result.Func = func;
    result.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
    result.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);

    auto *schedule =
        reinterpret_cast<TChannelConfig_WeeklySchedule *>(result.Config);
    memset(schedule, 0, sizeof(TChannelConfig_WeeklySchedule));
    schedule->Program[0].Mode = programMode;

    Supla::Control::WeeklyScheduleBuffer buffer;
    for (int index = 0; index < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; index++) {
      buffer.setWeeklySchedule(schedule, index, 1);
    }

    return result;
  }

  TSD_ChannelConfig makeTransitionWeeklySchedule(uint32_t func,
                                                 uint8_t firstProgramMode,
                                                 uint8_t secondProgramMode) {
    TSD_ChannelConfig result = {};
    result.ChannelNumber = 0;
    result.Func = func;
    result.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
    result.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);

    auto *schedule =
        reinterpret_cast<TChannelConfig_WeeklySchedule *>(result.Config);
    memset(schedule, 0, sizeof(TChannelConfig_WeeklySchedule));
    schedule->Program[0].Mode = firstProgramMode;
    schedule->Program[1].Mode = secondProgramMode;

    Supla::Control::WeeklyScheduleBuffer buffer;
    for (int index = 0; index < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; index++) {
      buffer.setWeeklySchedule(schedule, index, 2);
    }
    buffer.setWeeklySchedule(schedule, 0, 1);

    return result;
  }

  const TRelayChannel_Value *relayValue(const Supla::Control::Relay &relay) {
    return reinterpret_cast<const TRelayChannel_Value *>(
        Supla::RegisterDevice::getChannelValuePtr(relay.getChannelNumber()));
  }

  void enableWeeklySchedule(Supla::Control::Relay *relay) {
    TSD_SuplaChannelNewValue newValue = {};
    reinterpret_cast<TRelayChannel_Value *>(newValue.value)->RelayMode =
        SUPLA_RELAY_MODE_CMD_WEEKLY_SCHEDULE;
    EXPECT_EQ(relay->handleNewValueFromServer(&newValue), 1);
  }

  int32_t sendRelayMode(Supla::Control::Relay *relay, uint8_t mode) {
    TSD_SuplaChannelNewValue newValue = {};
    auto *relayValue =
        reinterpret_cast<TRelayChannel_Value *>(newValue.value);
    relayValue->RelayMode = mode;
    relayValue->hi = mode == SUPLA_RELAY_MODE_START_ON ||
            mode == SUPLA_RELAY_MODE_FORCED_ON
        ? 1
        : 0;
    return relay->handleNewValueFromServer(&newValue);
  }
};

TEST_F(RelayFixture, LightRelayIoPinConstructorUsesConfiguredIoAndPolarity) {
  SuplaIoMock outputIo;
  Supla::Io::IoPin outputPin(11, &outputIo);
  outputPin.setActiveHigh(false);
  outputPin.setMode(OUTPUT);

  EXPECT_CALL(outputIo, customPinMode(0, 11, OUTPUT));
  EXPECT_CALL(outputIo, customDigitalWrite(0, 11, HIGH)).Times(2);

  Supla::Control::LightRelay relay(outputPin);
  relay.onInit();
}

TEST_F(RelayFixture, weeklyScheduleKeepsUnsavedInactiveConfig) {
  AdjustableWeeklyClock clock;
  TimedWeeklyRelay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);
  relay.onInit();
  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                               SUPLA_RELAY_MODE_START_ON);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  time.advance(16000);
  relay.iterateAlways();
  enableWeeklySchedule(&relay);
  EXPECT_TRUE(relayValue(relay)->flags &
              SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
}

TEST_F(RelayFixture, weeklyDurationManualActionAtProgramBoundary) {
  // Cover both no-op -> timed and timed -> timed, on either side of the
  // boundary. An action before the boundary must not suppress the new program.
  for (bool timedFirst : {false, true}) {
    for (bool afterBoundary : {false, true}) {
      SCOPED_TRACE(::testing::Message() << timedFirst << ", " << afterBoundary);
      AdjustableWeeklyClock clock;
      int pin = 0;
      ON_CALL(ioMock, digitalRead(1))
          .WillByDefault(::testing::ReturnPointee(&pin));
      ON_CALL(ioMock, digitalWrite(1, _))
          .WillByDefault(::testing::SaveArg<1>(&pin));
      TimedWeeklyRelay relay(1);
      relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
      relay.onLoadConfig(nullptr);
      relay.onInit();
      auto config = makeTransitionWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
          timedFirst ? SUPLA_RELAY_MODE_START_ON : SUPLA_RELAY_MODE_NOT_SET,
          SUPLA_RELAY_MODE_START_ON);
      auto *schedule =
          reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
      schedule->Program[0].RelayModeDurationS = timedFirst ? 600 : 0;
      schedule->Program[1].RelayModeDurationS = 600;
      ASSERT_EQ(relay.handleChannelConfig(&config, false),
                SUPLA_CONFIG_RESULT_TRUE);
      enableWeeklySchedule(&relay);
      clock.shift(afterBoundary ? 900 : 899);
      relay.handleAction(0, Supla::TURN_OFF);
      ASSERT_FALSE(relay.isOn());
      if (!afterBoundary) {
        clock.shift(1);
      }
      relay.iterateAlways();
      EXPECT_EQ(relay.isOn(), !afterBoundary);
      clock.shift(10);
      relay.iterateAlways();
      EXPECT_EQ(relay.isOn(), !afterBoundary);
    }
  }
}

TEST_F(RelayFixture, weeklyDurationResolvesClockAndManualOverride) {
  AdjustableWeeklyClock clock;
  int pin = 0;
  ON_CALL(ioMock, digitalRead(1)).WillByDefault(::testing::ReturnPointee(&pin));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&pin));
  TimedWeeklyRelay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);
  relay.onInit();
  auto config = makeTransitionWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                             SUPLA_RELAY_MODE_START_ON,
                                             SUPLA_RELAY_MODE_START_OFF);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  schedule->Program[0].RelayModeDurationS = 10;
  schedule->Program[0].RelayOppositeModeDurationS = 20;
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  time.advance(15000);  // Activation in the OFF phase must not pulse ON.
  enableWeeklySchedule(&relay);
  EXPECT_FALSE(relay.isOn());
  EXPECT_EQ(relay.timer(), 0);
  time.advance(15000);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
  time.advance(10000);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
  relay.handleAction(0, Supla::TURN_ON);
  time.advance(90000);
  relay.iterateAlways();  // Another OFF phase, suppressed by manual ON.
  EXPECT_TRUE(relay.isOn());
  EXPECT_TRUE(relayValue(relay)->flags &
              SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  enableWeeklySchedule(
      &relay);  // Explicit reactivation resumes the clock phase.
  EXPECT_FALSE(relay.isOn());
  time.advance(770000);  // Program 2 at 00:15.
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
  clock.shift(7 * 24 * 3600 - 900);
  relay.iterateAlways();  // Next week's program 1 starts in its ON phase.
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, weeklyDurationKeepsManualActionBeforeClockIsReady) {
  AdjustableWeeklyClock clock;
  clock.ready = false;
  int pin = 0;
  ON_CALL(ioMock, digitalRead(1)).WillByDefault(::testing::ReturnPointee(&pin));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&pin));
  TimedWeeklyRelay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);
  relay.onInit();
  auto config = makeTransitionWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                             SUPLA_RELAY_MODE_START_ON,
                                             SUPLA_RELAY_MODE_START_ON);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  schedule->Program[0].RelayModeDurationS = 600;
  schedule->Program[1].RelayModeDurationS = 600;
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  relay.turnOn();
  relay.handleAction(0, Supla::TURN_OFF);
  ASSERT_FALSE(relay.isOn());

  clock.ready = true;
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());

  clock.shift(15 * 60);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, weeklyDurationRetriesDeferredPhaseOutput) {
  AdjustableWeeklyClock clock;
  int pin = 0;
  ON_CALL(ioMock, digitalRead(1)).WillByDefault(::testing::ReturnPointee(&pin));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&pin));
  DeferredWeeklyRelay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);
  relay.onInit();
  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_START_ON);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  schedule->Program[0].RelayModeDurationS = 600;
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);

  enableWeeklySchedule(&relay);
  EXPECT_EQ(relay.scheduleOutputAttempts, 1);
  EXPECT_FALSE(relay.isOn());
  relay.scheduleOutputReady = true;
  relay.iterateAlways();
  EXPECT_EQ(relay.scheduleOutputAttempts, 2);
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, weeklyDurationCachesOccurrenceTiming) {
  AdjustableWeeklyClock clock;
  int pin = 0;
  ON_CALL(ioMock, digitalRead(1)).WillByDefault(::testing::ReturnPointee(&pin));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&pin));
  TimedWeeklyRelay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  auto *controller = new CountingRelayWeeklySchedule(&relay);
  ASSERT_TRUE(
      relay.setWeeklyScheduleController(controller, controller, controller));
  relay.onLoadConfig(nullptr);
  relay.onInit();
  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_START_ON);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  schedule->Program[0].RelayModeDurationS = 2;
  schedule->Program[0].RelayOppositeModeDurationS = 3;
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  time.advance(1000);
  enableWeeklySchedule(&relay);
  ASSERT_EQ(controller->timingResolveCount, 1);

  for (int i = 0; i < 20; i++) {
    time.advance(1000);
    relay.iterateAlways();
  }
  EXPECT_EQ(controller->timingResolveCount, 1);

  time.advance(15 * 60 * 1000);
  relay.iterateAlways();
  EXPECT_EQ(controller->timingResolveCount, 1);

  clock.shift(30 * 60);
  relay.iterateAlways();
  EXPECT_EQ(controller->timingResolveCount, 2);
}

TEST_F(RelayFixture,
       weeklyDurationOverridesTimedFunctionWithoutChangingConfig) {
  for (auto function :
       {SUPLA_CHANNELFNC_STAIRCASETIMER, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE}) {
    AdjustableWeeklyClock clock;
    int pin = 0;
    ON_CALL(ioMock, digitalRead(1))
        .WillByDefault(::testing::ReturnPointee(&pin));
    ON_CALL(ioMock, digitalWrite(1, _))
        .WillByDefault(::testing::SaveArg<1>(&pin));
    TimedWeeklyRelay relay(1);
    relay.setDefaultFunction(function);
    relay.onLoadConfig(nullptr);
    relay.onInit();
    const auto stored = relay.storedDuration();
    auto config =
        makeSingleProgramWeeklySchedule(function, SUPLA_RELAY_MODE_START_ON);
    auto *schedule =
        reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
    schedule->Program[0].RelayModeDurationS = 30;
    ASSERT_EQ(relay.handleChannelConfig(&config, false),
              SUPLA_CONFIG_RESULT_TRUE);
    enableWeeklySchedule(&relay);
    EXPECT_TRUE(relay.isOn());
    EXPECT_EQ(relay.timer(), 0);
    EXPECT_EQ(relay.storedDuration(), stored);
    clock.shift(30);
    relay.iterateAlways();
    EXPECT_FALSE(relay.isOn());
    EXPECT_EQ(relay.storedDuration(), stored);
    relay.handleAction(0, Supla::TURN_ON);
    EXPECT_EQ(relay.timer(), stored);
    clock.shift(-30);
  }
}

TEST_F(RelayFixture, weeklyDurationValidationAndAbi) {
  EXPECT_EQ(sizeof(TWeeklyScheduleProgram), 5);
  EXPECT_EQ(sizeof(TChannelConfig_WeeklySchedule), 356);
  TimedWeeklyRelay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);
  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_START_ON);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  auto &program = schedule->Program[0];
  program.RelayModeDurationS = 65535;
  program.RelayOppositeModeDurationS = 32768;
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  program.RelayModeDurationS = 0;
  EXPECT_NE(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  program.RelayModeDurationS = 10;
  program.Mode = SUPLA_RELAY_MODE_FORCED_ON;
  EXPECT_NE(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  program.Mode = SUPLA_RELAY_MODE_START_ON;
  relay.setDefaultFunction(SUPLA_CHANNELFNC_STAIRCASETIMER);
  config.Func = SUPLA_CHANNELFNC_STAIRCASETIMER;
  EXPECT_NE(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
}

TEST_F(RelayFixture, weeklyDurationWaitsForClockAndHonorsOvercurrent) {
  AdjustableWeeklyClock clock;
  clock.ready = false;
  int pin = 0;
  ON_CALL(ioMock, digitalRead(1)).WillByDefault(::testing::ReturnPointee(&pin));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&pin));
  TimedWeeklyRelay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);
  relay.onInit();
  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_START_ON);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  schedule->Program[0].RelayModeDurationS = 10;
  schedule->Program[0].RelayOppositeModeDurationS = 10;
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  time.advance(40000);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
  clock.ready = true;
  relay.getChannel()->setRelayOvercurrentCutOff(true);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
  time.advance(20000);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
  EXPECT_TRUE(relay.getChannel()->isRelayOvercurrentCutOff());
  relay.handleAction(0, Supla::TURN_ON);
  EXPECT_FALSE(relay.getChannel()->isRelayOvercurrentCutOff());
  time.advance(10000);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, weeklyDurationSpansWeekAndStopsAtNoOp) {
  AdjustableWeeklyClock clock;
  clock.shift(-10);  // Saturday 23:59:50.
  int pin = 0;
  ON_CALL(ioMock, digitalRead(1)).WillByDefault(::testing::ReturnPointee(&pin));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&pin));
  TimedWeeklyRelay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);
  relay.onInit();
  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_START_OFF);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  memset(schedule->Quarters, 0, sizeof(schedule->Quarters));
  Supla::Control::WeeklyScheduleBuffer buffer;
  buffer.setWeeklySchedule(schedule, 671, 1);
  buffer.setWeeklySchedule(schedule, 0, 1);
  schedule->Program[0].RelayModeDurationS = 910;
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  EXPECT_FALSE(relay.isOn());
  clock.shift(19);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());  // 909 seconds since Saturday 23:45.
  clock.shift(1);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
  clock.shift(890);  // End of Sunday's first quarter: no-op.
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
  clock.shift(-900);  // Back into the OFF phase after a clock correction.
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture,
       weeklyDurationUniformCycleReanchorsOnlyForFreshController) {
  AdjustableWeeklyClock clock;
  clock.shift(-1);  // Saturday 23:59:59.
  int continuousPin = 0;
  int freshPin = 0;
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&continuousPin));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&continuousPin));
  ON_CALL(ioMock, digitalRead(2))
      .WillByDefault(::testing::ReturnPointee(&freshPin));
  ON_CALL(ioMock, digitalWrite(2, _))
      .WillByDefault(::testing::SaveArg<1>(&freshPin));
  TimedWeeklyRelay continuousRelay(1);
  continuousRelay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  continuousRelay.onLoadConfig(nullptr);
  continuousRelay.onInit();
  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_START_ON);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  schedule->Program[0].RelayModeDurationS = 2;
  schedule->Program[0].RelayOppositeModeDurationS = 9;
  ASSERT_EQ(continuousRelay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&continuousRelay);
  ASSERT_FALSE(continuousRelay.isOn());

  clock.shift(1);  // Sunday 00:00:00; the running cycle remains continuous.
  continuousRelay.iterateAlways();
  EXPECT_FALSE(continuousRelay.isOn());

  // A fresh controller resolves the uniform schedule from this Sunday's
  // midnight, matching the behavior after a restart and config reload.
  TimedWeeklyRelay freshRelay(2);
  freshRelay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  freshRelay.onLoadConfig(nullptr);
  freshRelay.onInit();
  ASSERT_EQ(freshRelay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&freshRelay);
  EXPECT_TRUE(freshRelay.isOn());
}

TEST_F(RelayFixture, weeklyCyclePreservesManualTimerAndIdenticalConfig) {
  AdjustableWeeklyClock clock;
  int pin = 0;
  ON_CALL(ioMock, digitalRead(1)).WillByDefault(::testing::ReturnPointee(&pin));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&pin));
  TimedWeeklyRelay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);
  relay.onInit();
  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_START_OFF);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  schedule->Program[0].RelayModeDurationS = 10;
  schedule->Program[0].RelayOppositeModeDurationS = 20;
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  relay.turnOn(1000);
  enableWeeklySchedule(&relay);
  EXPECT_FALSE(relay.isOn());
  EXPECT_EQ(relay.timer(), 0);
  time.advance(15000);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
  TSD_SuplaChannelNewValue manual = {};
  manual.DurationMS = 2000;
  EXPECT_EQ(relay.handleNewValueFromServer(&manual), 1);
  EXPECT_FALSE(relay.isOn());
  EXPECT_EQ(relay.timer(), 2000);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
  time.advance(2001);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
  time.advance(13000);  // Weekly is in OFF phase, but still suspended.
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
  schedule->Program[0].RelayModeDurationS = 15;
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_TRUE(relay.isOn());  // New period: t=30 is in the ON phase.
  time.advance(4999);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());  // New period starts exactly at t=35.
}

TEST_F(RelayFixture, relayFlagsOperatingModeEncodingIsOtaCompatible) {
  Supla::Control::Relay::RelayFlags flags;

  flags.flags.operatingMode = RELAY_STORED_MODE_AUTOMATIC;
  EXPECT_EQ(flags.rawValue, RELAY_FLAGS_AUTOMATIC_MODE);
  flags.rawValue = 0;
  flags.flags.operatingMode = RELAY_STORED_MODE_FORCED_OFF;
  EXPECT_EQ(flags.rawValue, 1 << 6);
  flags.rawValue = 0;
  flags.flags.operatingMode = RELAY_STORED_MODE_FORCED_ON;
  EXPECT_EQ(flags.rawValue, (1 << 5) | (1 << 6));
}

TEST_F(RelayFixture, IoPinConstructorUsesConfiguredIoAndPolarity) {
  SuplaIoMock outputIo;
  Supla::Io::IoPin outputPin(11, &outputIo);
  outputPin.setActiveHigh(false);
  outputPin.setMode(OUTPUT);

  EXPECT_CALL(outputIo, customPinMode(0, 11, OUTPUT));
  EXPECT_CALL(outputIo, customDigitalWrite(0, 11, HIGH)).Times(2);
  EXPECT_CALL(outputIo, customDigitalRead(0, 11)).WillOnce(Return(HIGH));

  Supla::Control::Relay relay(outputPin);
  relay.onInit();
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture, SoftResetKeepsLegacyInitOrderByDefault) {
  const int gpio = 11;
  Supla::Control::Relay relay(gpio);
  relay.setDefaultStateOn();
  setLastResetSoft(true);

  InSequence sequence;
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  relay.onInit();
}

TEST_F(RelayFixture, SoftResetPreloadsStateBeforePinModeWhenEnabled) {
  const int gpio = 11;
  Supla::Control::Relay relay(gpio);
  relay.setDefaultStateOn();
  relay.setPreloadStateOnSoftReset();
  setLastResetSoft(true);

  InSequence sequence;
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  relay.onInit();
}

TEST_F(RelayFixture, RestoredImpulseStateStartsOff) {
  const int gpio = 11;
  Supla::Control::Relay relay(gpio);
  relay.setDefaultStateRestore();
  storage.defaultInitialization(5);

  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillOnce(DoAll(SetArgPointee<1>(
                          RELAY_FLAGS_IMPULSE_FUNCTION | RELAY_FLAGS_ON),
                      Return(1)));

  int gpioValue = 1;
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  relay.onLoadState();
  relay.onInit();

  EXPECT_EQ(0, gpioValue);
  EXPECT_EQ(500u, relay.getStoredTurnOnDurationMs());
  EXPECT_TRUE(relay.isImpulseFunction());
}

TEST_F(RelayFixture, UnsetIoPinDoesNothing) {
  SuplaIoMock outputIo;
  Supla::Io::IoPin outputPin;
  outputPin.io = &outputIo;

  EXPECT_CALL(outputIo, customPinMode).Times(0);
  EXPECT_CALL(outputIo, customDigitalRead).Times(0);
  EXPECT_CALL(outputIo, customDigitalWrite).Times(0);

  Supla::Control::Relay relay(outputPin);
  relay.onInit();
}

TEST_F(RelayFixture, basicTests) {
  int gpio1 = 1;
  int gpio2 = 2;
  int gpio3 = 3;
  Supla::Control::Relay r1(gpio1);
  Supla::Control::Relay r2(gpio2, false);
  Supla::Control::Relay r3(gpio3, false, SUPLA_BIT_FUNC_POWERSWITCH);

  int number1 = r1.getChannelNumber();
  int number2 = r2.getChannelNumber();
  int number3 = r3.getChannelNumber();
  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  ASSERT_EQ(number1, 0);
  ASSERT_EQ(number2, 1);
  ASSERT_EQ(number3, 2);
  EXPECT_EQ(number1, Supla::RegisterDevice::getChannelNumber(number1));
  EXPECT_EQ(Supla::RegisterDevice::getChannelType(number1),
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFunctionList(number1),
            (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  EXPECT_EQ(Supla::RegisterDevice::getChannelDefaultFunction(number1), 0);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number1),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
                SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED |
                SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number1),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(number2, Supla::RegisterDevice::getChannelNumber(number2));
  EXPECT_EQ(Supla::RegisterDevice::getChannelType(number2),
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFunctionList(number2),
            (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  EXPECT_EQ(Supla::RegisterDevice::getChannelDefaultFunction(number2), 0);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number2),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
                SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED |
                SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number2),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(number3, Supla::RegisterDevice::getChannelNumber(number3));
  EXPECT_EQ(Supla::RegisterDevice::getChannelType(number3),
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFunctionList(number3),
            SUPLA_BIT_FUNC_POWERSWITCH);
  EXPECT_EQ(Supla::RegisterDevice::getChannelDefaultFunction(number3), 0);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number3),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
                SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED |
                SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number3),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(2);
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio2, 1)).Times(2);
  EXPECT_CALL(ioMock, pinMode(gpio2, OUTPUT));

  r2.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio3, 1)).Times(2);
  EXPECT_CALL(ioMock, pinMode(gpio3, OUTPUT));

  r3.onInit();

  r1.disableCountdownTimerFunction();
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number1),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
                SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_FALSE(r1.isCountdownTimerFunctionEnabled());
  r1.enableCountdownTimerFunction();
  EXPECT_TRUE(r1.isCountdownTimerFunctionEnabled());
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number1),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
                SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED |
                SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
}

TEST_F(RelayFixture, weeklyScheduleCapabilityIsIndependentOfFunction) {
  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();

  relay.onLoadConfig(nullptr);
  EXPECT_TRUE(relay.getChannel()->isWeeklyScheduleAvailable());

  const uint32_t supportedFunctions[] = {
      SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK,
      SUPLA_CHANNELFNC_CONTROLLINGTHEGATE,
      SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR,
      SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK,
      SUPLA_CHANNELFNC_POWERSWITCH,
      SUPLA_CHANNELFNC_LIGHTSWITCH,
      SUPLA_CHANNELFNC_STAIRCASETIMER,
  };
  for (auto function : supportedFunctions) {
    EXPECT_TRUE(relay.setAndSaveFunction(function));
    EXPECT_TRUE(relay.getChannel()->isWeeklyScheduleAvailable());
  }

  const uint32_t unsupportedFunctions[] = {
      SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER,
      SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW,
      SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND,
      SUPLA_CHANNELFNC_TERRACE_AWNING,
      SUPLA_CHANNELFNC_PROJECTOR_SCREEN,
      SUPLA_CHANNELFNC_CURTAIN,
      SUPLA_CHANNELFNC_VERTICAL_BLIND,
      SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR,
      SUPLA_CHANNELFNC_PUMPSWITCH,
      SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH,
  };
  for (auto function : unsupportedFunctions) {
    EXPECT_TRUE(relay.setAndSaveFunction(function));
    EXPECT_TRUE(relay.getChannel()->isWeeklyScheduleAvailable());
  }
  const auto flags = relay.getChannel()->getFlags();
  EXPECT_NE(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_NOT_SET_SUPPORTED, 0);
  EXPECT_NE(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_START_SUPPORTED, 0);
  EXPECT_NE(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED, 0);
}

TEST_F(RelayFixture,
       WeeklyScheduleIsOptInAndControllerIsNotAllocatedByDefault) {
  RelayWeeklyScheduleProbe relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);

  EXPECT_FALSE(relay.isWeeklyScheduleAvailable());
  EXPECT_FALSE(relay.getChannel()->isWeeklyScheduleAvailable());
  EXPECT_FALSE(relay.hasWeeklyScheduleController());

  relay.setWeeklyScheduleAvailable();
  EXPECT_TRUE(relay.isWeeklyScheduleAvailable());
  EXPECT_TRUE(relay.getChannel()->isWeeklyScheduleAvailable());
  EXPECT_TRUE(relay.hasWeeklyScheduleController());

  relay.setWeeklyScheduleAvailable(false);
  EXPECT_FALSE(relay.isWeeklyScheduleAvailable());
  EXPECT_FALSE(relay.getChannel()->isWeeklyScheduleAvailable());
}

TEST_F(RelayFixture, nativeWeeklyScheduleControllerIsAllocatedLazily) {
  RelayWithCustomWeeklySchedule relay(1);

  EXPECT_FALSE(relay.hasWeeklyScheduleController());

  EXPECT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_PUMPSWITCH));
  EXPECT_FALSE(relay.hasWeeklyScheduleController());

  EXPECT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE));
  EXPECT_TRUE(relay.hasWeeklyScheduleController());
}

TEST_F(RelayFixture, weeklyScheduleConfigIsRejectedForUnsupportedFunction) {
  Supla::Control::Relay relay(1);
  ASSERT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_PUMPSWITCH));

  TSD_ChannelConfig config = {};
  config.Func = SUPLA_CHANNELFNC_PUMPSWITCH;
  config.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;

  EXPECT_EQ(relay.handleWeeklySchedule(&config, false, false),
            SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED);
}

TEST_F(RelayFixture, weeklyNoOpCapabilityMatchesProgramValidation) {
  Supla::Control::Relay regular(1);
  regular.setWeeklyScheduleAvailable();
  regular.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  regular.onLoadConfig(nullptr);
  EXPECT_NE(regular.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_NOT_SET_SUPPORTED, 0);
  RestrictedWeeklyModesRelay restricted(
      2, (1 << SUPLA_RELAY_MODE_FORCED_ON) |
             (1 << SUPLA_RELAY_MODE_FORCED_OFF));
  restricted.setWeeklyScheduleAvailable();
  restricted.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  restricted.onLoadConfig(nullptr);
  const auto flags = restricted.getChannel()->getFlags();
  EXPECT_EQ(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_NOT_SET_SUPPORTED, 0);
  EXPECT_EQ(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_START_SUPPORTED, 0);
  EXPECT_NE(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED, 0);

  TChannelConfig_WeeklySchedule defaultSchedule = {};
  int defaultSize = 0;
  restricted.fillChannelConfig(&defaultSchedule, &defaultSize,
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  EXPECT_EQ(defaultSize, sizeof(defaultSchedule));
  EXPECT_EQ(defaultSchedule.Program[0].Mode,
            SUPLA_RELAY_MODE_FORCED_OFF);
  for (auto quarters : defaultSchedule.Quarters) {
    EXPECT_EQ(quarters, 0x11);
  }
  TSD_ChannelConfig defaultConfig = {};
  defaultConfig.ChannelNumber = restricted.getChannelNumber();
  defaultConfig.Func = SUPLA_CHANNELFNC_LIGHTSWITCH;
  defaultConfig.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  defaultConfig.ConfigSize = sizeof(defaultSchedule);
  memcpy(defaultConfig.Config, &defaultSchedule, sizeof(defaultSchedule));
  EXPECT_EQ(restricted.handleChannelConfig(&defaultConfig, false),
            SUPLA_CONFIG_RESULT_TRUE);

  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_FORCED_ON);
  config.ChannelNumber = restricted.getChannelNumber();
  EXPECT_EQ(restricted.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  auto *schedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config.Config);
  schedule->Quarters[0] = 0;  // Implicit no-op (program zero).
  EXPECT_EQ(restricted.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  schedule->Quarters[0] = 0x11;
  schedule->Program[0].Mode = SUPLA_RELAY_MODE_NOT_SET;
  EXPECT_EQ(restricted.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
}

TEST_F(RelayFixture, weeklyModeGroupsSelectSafeDefaults) {
  struct TestCase {
    uint8_t modes;
    uint64_t expectedFlag;
    uint8_t expectedDefault;
    bool automatic;
  };
  const TestCase testCases[] = {
      {static_cast<uint8_t>((1 << SUPLA_RELAY_MODE_START_ON) |
                            (1 << SUPLA_RELAY_MODE_START_OFF)),
       SUPLA_CHANNEL_FLAG_RELAY_MODE_START_SUPPORTED,
       SUPLA_RELAY_MODE_START_OFF,
       false},
      {0,
       SUPLA_CHANNEL_FLAG_RELAY_MODE_AUTOMATIC_SUPPORTED,
       SUPLA_RELAY_MODE_AUTOMATIC,
       true},
  };
  int pin = 10;
  for (const auto &testCase : testCases) {
    RestrictedWeeklyModesRelay relay(pin++, testCase.modes);
    relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
    relay.setWeeklyScheduleAvailable();
    if (testCase.automatic) {
      relay.setAutomaticModeSupported();
    }
    relay.onLoadConfig(nullptr);
    const auto flags = relay.getChannel()->getFlags();
    EXPECT_NE(flags & SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE, 0);
    EXPECT_NE(flags & testCase.expectedFlag, 0);
    EXPECT_EQ(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_NOT_SET_SUPPORTED, 0);

    TChannelConfig_WeeklySchedule schedule = {};
    int size = 0;
    relay.fillChannelConfig(&schedule, &size,
                            SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
    EXPECT_EQ(size, sizeof(schedule));
    EXPECT_EQ(schedule.Program[0].Mode, testCase.expectedDefault);
    for (auto quarters : schedule.Quarters) {
      EXPECT_EQ(quarters, 0x11);
    }
  }
}

TEST_F(RelayFixture, weeklyAutomaticUsesRelayCapabilityOnly) {
  RestrictedWeeklyModesRelay relay(
      19, 1 << SUPLA_RELAY_MODE_AUTOMATIC);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);
  const auto flags = relay.getChannel()->getFlags();
  EXPECT_EQ(flags & SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE, 0);
  EXPECT_EQ(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_AUTOMATIC_SUPPORTED, 0);

  auto config = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH, SUPLA_RELAY_MODE_AUTOMATIC);
  config.ChannelNumber = relay.getChannelNumber();
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED);
}

TEST_F(RelayFixture, weeklyModeGroupsRejectUnpairedModes) {
  const uint8_t modes[] = {
      SUPLA_RELAY_MODE_START_ON,
      SUPLA_RELAY_MODE_START_OFF,
      SUPLA_RELAY_MODE_FORCED_ON,
      SUPLA_RELAY_MODE_FORCED_OFF,
  };
  int pin = 20;
  for (auto mode : modes) {
    RestrictedWeeklyModesRelay relay(pin++, 1 << mode);
    relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
    relay.setWeeklyScheduleAvailable();
    relay.onLoadConfig(nullptr);
    const auto flags = relay.getChannel()->getFlags();
    EXPECT_EQ(flags & SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE, 0);
    EXPECT_EQ(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_START_SUPPORTED, 0);
    EXPECT_EQ(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED, 0);

    auto config = makeSingleProgramWeeklySchedule(
        SUPLA_CHANNELFNC_LIGHTSWITCH, mode);
    config.ChannelNumber = relay.getChannelNumber();
    EXPECT_EQ(relay.handleChannelConfig(&config, false),
              SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED);
  }
}

TEST_F(RelayFixture, impulseWeeklyScheduleFunctionsSupportOnlyStartModes) {
  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();

  const uint32_t impulseFunctions[] = {
      SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK,
      SUPLA_CHANNELFNC_CONTROLLINGTHEGATE,
      SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR,
      SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK,
  };
  for (auto function : impulseFunctions) {
    ASSERT_TRUE(relay.setAndSaveFunction(function));
    const auto flags = relay.getChannel()->getFlags();
    EXPECT_NE(flags & SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE, 0);
    EXPECT_NE(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_START_SUPPORTED, 0);
    EXPECT_NE(flags & SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED, 0);

    auto startConfig = makeSingleProgramWeeklySchedule(
        function, SUPLA_RELAY_MODE_START_ON);
    EXPECT_EQ(relay.handleChannelConfig(&startConfig, false),
              SUPLA_CONFIG_RESULT_TRUE);

    auto forcedConfig = makeSingleProgramWeeklySchedule(
        function, SUPLA_RELAY_MODE_FORCED_ON);
    EXPECT_EQ(relay.handleChannelConfig(&forcedConfig, false),
              SUPLA_CONFIG_RESULT_DATA_ERROR);
  }

  ASSERT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_STAIRCASETIMER));
  EXPECT_NE(relay.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED,
            0);
  auto staircaseForcedConfig = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_STAIRCASETIMER, SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_EQ(relay.handleChannelConfig(&staircaseForcedConfig, false),
            SUPLA_CONFIG_RESULT_TRUE);

  relay.setAutomaticModeSupported();
  EXPECT_NE(relay.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_AUTOMATIC_SUPPORTED,
            0);

  ASSERT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_LIGHTSWITCH));
  EXPECT_NE(relay.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED,
            0);
}

TEST_F(RelayFixture,
       externalWeeklyScheduleReportsAutomaticWithoutNativeConfig) {
  ::testing::NiceMock<ConfigMock> cfg;
  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  ASSERT_TRUE(relay.setWeeklyScheduleController(
      new Supla::Control::ExternalManagedWeeklySchedule()));

  relay.onLoadConfig(nullptr);

  EXPECT_FALSE(relay.getChannel()->isWeeklyScheduleAvailable());
  EXPECT_NE(relay.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_AUTOMATIC_SUPPORTED,
            0);

  TSD_ChannelConfig weeklyConfig = {};
  weeklyConfig.Func = SUPLA_CHANNELFNC_LIGHTSWITCH;
  weeklyConfig.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  EXPECT_EQ(relay.handleWeeklySchedule(&weeklyConfig, false, false),
            SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED);

  enableWeeklySchedule(&relay);
  const auto *value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_AUTOMATIC);

  TSD_SuplaChannelNewValue manualValue = {};
  reinterpret_cast<TRelayChannel_Value *>(manualValue.value)->RelayMode =
      SUPLA_RELAY_MODE_CMD_SWITCH_TO_MANUAL;
  EXPECT_EQ(relay.handleNewValueFromServer(&manualValue), 1);
  EXPECT_FALSE(relayValue(relay)->flags &
               SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_NOT_SET);

  enableWeeklySchedule(&relay);
  ASSERT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_PUMPSWITCH));
  EXPECT_NE(relay.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_AUTOMATIC_SUPPORTED,
            0);
  EXPECT_FALSE(relayValue(relay)->flags &
               SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
}

TEST_F(RelayFixture,
       nativeWeeklyScheduleSwitchesFromAutomaticProgramToForcedOff) {
  ClockStub clock;
  ::testing::NiceMock<ConfigMock> cfg;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  RelayWithAutomaticWeeklySchedule relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setAutomaticModeSupported();
  relay.onLoadConfig(nullptr);

  EXPECT_TRUE(relay.getChannel()->isWeeklyScheduleAvailable());
  EXPECT_NE(relay.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_AUTOMATIC_SUPPORTED,
            0);

  int relayPinValue = 0;
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  relay.onInit();
  relay.turnOn();
  ASSERT_TRUE(relay.isOn());

  auto config = makeTransitionWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH,
      SUPLA_RELAY_MODE_AUTOMATIC,
      SUPLA_RELAY_MODE_FORCED_OFF);
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(relay.automaticModeIterationCount, 0);

  enableWeeklySchedule(&relay);
  EXPECT_EQ(relay.automaticModeIterationCount, 0);

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_EQ(relay.automaticModeIterationCount, 1);
  EXPECT_TRUE(relay.isAutomaticMode());
  EXPECT_TRUE(relay.isOn());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_AUTOMATIC);

  time.advance(15 * 60 * 1000);
  relay.iterateAlways();
  EXPECT_EQ(relay.automaticModeIterationCount, 1);
  EXPECT_FALSE(relay.isAutomaticMode());
  EXPECT_FALSE(relay.isOn());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);
}

TEST_F(RelayFixture, automaticModeCanBeEnabledWithoutWeeklySchedule) {
  ::testing::NiceMock<ConfigMock> cfg;
  storage.defaultInitialization(5);
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  RelayWithAutomaticWeeklySchedule relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setAutomaticModeSupported();
  relay.onLoadConfig(nullptr);
  relay.onInit();

  TSD_SuplaChannelNewValue automaticValue = {};
  reinterpret_cast<TRelayChannel_Value *>(automaticValue.value)->RelayMode =
      SUPLA_RELAY_MODE_AUTOMATIC;
  EXPECT_EQ(relay.handleNewValueFromServer(&automaticValue), 1);
  EXPECT_TRUE(relay.isAutomaticMode());
  EXPECT_FALSE(relayValue(relay)->flags &
               SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);

  relay.iterateAlways();
  EXPECT_EQ(relay.automaticModeIterationCount, 1);

  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&storage));
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce(Return(0));
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillOnce(Return(0));
  EXPECT_CALL(storage, writeStorage(_, _, sizeof(uint32_t)))
      .WillOnce(Return(sizeof(uint32_t)));
  EXPECT_CALL(storage, writeStorage(_, _, sizeof(uint8_t)))
      .WillOnce([](uint32_t, const unsigned char *data, int32_t) {
        Supla::Control::Relay::RelayFlags flags;
        flags.rawValue = *data;
        EXPECT_EQ(flags.flags.weeklySchedule, 0);
        EXPECT_EQ(flags.flags.operatingMode, RELAY_STORED_MODE_AUTOMATIC);
        return sizeof(uint8_t);
      });
  relay.onSaveState();

  EXPECT_CALL(storage, scheduleSave(5000, 2000)).Times(1);
  TSD_SuplaChannelNewValue manualValue = {};
  reinterpret_cast<TRelayChannel_Value *>(manualValue.value)->RelayMode =
      SUPLA_RELAY_MODE_CMD_SWITCH_TO_MANUAL;
  EXPECT_EQ(relay.handleNewValueFromServer(&manualValue), 1);
  EXPECT_FALSE(relay.isAutomaticMode());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
}

TEST_F(RelayFixture, automaticModeCapabilityDoesNotDependOnWeeklySchedule) {
  ::testing::NiceMock<ConfigMock> cfg;
  RelayWithAutomaticWeeklySchedule relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  relay.setAutomaticModeSupported();

  relay.onLoadConfig(nullptr);

  EXPECT_TRUE(relay.getChannel()->isWeeklyScheduleAvailable());
  EXPECT_NE(relay.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_AUTOMATIC_SUPPORTED,
            0);

  TSD_SuplaChannelNewValue automaticValue = {};
  reinterpret_cast<TRelayChannel_Value *>(automaticValue.value)->RelayMode =
      SUPLA_RELAY_MODE_AUTOMATIC;
  EXPECT_EQ(relay.handleNewValueFromServer(&automaticValue), 1);
  EXPECT_TRUE(relay.isAutomaticMode());
}

TEST_F(RelayFixture, setAutomaticModeDisablesActiveWeeklySchedule) {
  ClockStub clock;
  ::testing::NiceMock<ConfigMock> cfg;
  RelayWithAutomaticWeeklySchedule relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setAutomaticModeSupported();
  relay.onLoadConfig(nullptr);

  auto config = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH, SUPLA_RELAY_MODE_FORCED_OFF);
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  ASSERT_TRUE(relayValue(relay)->flags &
              SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);

  EXPECT_TRUE(relay.setAutomaticMode(true));

  EXPECT_FALSE(relay.isWeeklyScheduleActive());
  EXPECT_FALSE(relayValue(relay)->flags &
               SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_TRUE(relay.isAutomaticMode());
}

TEST_F(RelayFixture, automaticModeIsRestoredWithoutWeeklySchedule) {
  ::testing::NiceMock<ConfigMock> cfg;
  storage.defaultInitialization(5);
  RelayWithAutomaticWeeklySchedule relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setAutomaticModeSupported();
  relay.onLoadConfig(nullptr);

  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        memset(data, 0, sizeof(uint32_t));
        return sizeof(uint32_t);
      });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillOnce(DoAll(SetArgPointee<1>(RELAY_FLAGS_AUTOMATIC_MODE), Return(1)));

  relay.onLoadState();

  EXPECT_TRUE(relay.isAutomaticMode());
  EXPECT_FALSE(relayValue(relay)->flags &
               SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
}

TEST_F(RelayFixture, automaticModeCommandIsRejectedWhenUnsupported) {
  ::testing::NiceMock<ConfigMock> cfg;
  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);

  TSD_SuplaChannelNewValue automaticValue = {};
  reinterpret_cast<TRelayChannel_Value *>(automaticValue.value)->RelayMode =
      SUPLA_RELAY_MODE_AUTOMATIC;
  EXPECT_EQ(relay.handleNewValueFromServer(&automaticValue), 0);
  EXPECT_FALSE(relay.isAutomaticMode());
}

TEST_F(RelayFixture, manualForcedModeBlocksOrdinaryCommandsAndActions) {
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  int relayPinValue = 0;
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);
  relay.onInit();

  EXPECT_NE(relay.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED,
            0);
  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_ON), 1);
  EXPECT_TRUE(relay.isOn());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);

  TSD_SuplaChannelNewValue turnOn = {};
  turnOn.value[0] = 1;
  EXPECT_EQ(relay.handleNewValueFromServer(&turnOn), 0);
  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_START_ON), 0);
  TSD_SuplaChannelNewValue turnOff = {};
  EXPECT_EQ(relay.handleNewValueFromServer(&turnOff), 0);
  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_START_OFF), 0);
  relay.handleAction(0, Supla::TURN_ON);
  relay.handleAction(0, Supla::TURN_OFF);
  relay.handleAction(0, Supla::TOGGLE);
  EXPECT_TRUE(relay.isOn());

  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_OFF), 1);
  EXPECT_FALSE(relay.isOn());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);

  EXPECT_EQ(relay.handleNewValueFromServer(&turnOn), 0);
  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_START_ON), 0);
  EXPECT_EQ(relay.handleNewValueFromServer(&turnOff), 0);
  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_START_OFF), 0);
  relay.handleAction(0, Supla::TURN_ON);
  relay.handleAction(0, Supla::TURN_OFF);
  relay.handleAction(0, Supla::TOGGLE);
  EXPECT_FALSE(relay.isOn());

  EXPECT_EQ(
      sendRelayMode(&relay, SUPLA_RELAY_MODE_CMD_SWITCH_TO_MANUAL), 1);
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
  EXPECT_EQ(relay.handleNewValueFromServer(&turnOn), 1);
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, manualForcedModeCancelsExistingTimer) {
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  int relayPinValue = 0;
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);
  relay.onInit();

  TSD_SuplaChannelNewValue timedOn = {};
  timedOn.value[0] = 1;
  timedOn.DurationMS = 1000;
  ASSERT_EQ(relay.handleNewValueFromServer(&timedOn), 1);
  ASSERT_TRUE(relay.isOn());

  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_ON), 1);
  time.advance(1001);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, automaticCommandExitsManualForcedMode) {
  ::testing::NiceMock<ConfigMock> cfg;
  RelayWithAutomaticWeeklySchedule relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setAutomaticModeSupported();
  relay.onLoadConfig(nullptr);

  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_OFF), 1);
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);

  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_AUTOMATIC), 1);
  EXPECT_TRUE(relay.isAutomaticMode());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_AUTOMATIC);
}

TEST_F(RelayFixture, weeklyScheduleCommandExitsManualForcedMode) {
  ClockStub clock;
  ::testing::NiceMock<ConfigMock> cfg;
  RelayWithAutomaticWeeklySchedule relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);

  auto config = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH, SUPLA_RELAY_MODE_START_ON);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_OFF), 1);

  EXPECT_EQ(
      sendRelayMode(&relay, SUPLA_RELAY_MODE_CMD_WEEKLY_SCHEDULE), 1);
  EXPECT_TRUE(relayValue(relay)->flags &
              SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_NOT_SET);

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_START_ON);
}

TEST_F(RelayFixture, manualForcedModeIsStoredAndRestored) {
  ::testing::NiceMock<ConfigMock> cfg;
  storage.defaultInitialization(5);
  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  ASSERT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_ON), 1);
  ASSERT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);

  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&storage));
  EXPECT_CALL(storage, scheduleSave(5000, 2000)).Times(2);
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce(Return(0));
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillOnce(Return(0));
  EXPECT_CALL(storage, writeStorage(_, _, sizeof(uint32_t)))
      .WillOnce(Return(sizeof(uint32_t)));
  EXPECT_CALL(storage, writeStorage(_, _, sizeof(uint8_t)))
      .WillOnce([](uint32_t, const unsigned char *data, int32_t) {
        Supla::Control::Relay::RelayFlags flags;
        flags.rawValue = *data;
        EXPECT_EQ(flags.flags.weeklySchedule, 0);
        EXPECT_EQ(flags.flags.operatingMode, RELAY_STORED_MODE_FORCED_ON);
        return sizeof(uint8_t);
      });
  relay.onSaveState();

  Supla::Control::Relay restoredRelay(2);
  restoredRelay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  restoredRelay.setWeeklyScheduleAvailable();
  restoredRelay.onLoadConfig(nullptr);
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        memset(data, 0, sizeof(uint32_t));
        return sizeof(uint32_t);
      });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        Supla::Control::Relay::RelayFlags flags;
        flags.flags.operatingMode = RELAY_STORED_MODE_FORCED_ON;
        *data = flags.rawValue;
        return sizeof(uint8_t);
      });
  restoredRelay.onLoadState();

  EXPECT_EQ(relayValue(restoredRelay)->RelayMode,
            SUPLA_RELAY_MODE_FORCED_ON);
  int restoredPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(2)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(2, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(2))
      .WillByDefault(::testing::ReturnPointee(&restoredPinValue));
  ON_CALL(ioMock, digitalWrite(2, _))
      .WillByDefault(::testing::SaveArg<1>(&restoredPinValue));
  EXPECT_CALL(ioMock, pinMode(2, OUTPUT));
  restoredRelay.onInit();
  EXPECT_TRUE(restoredRelay.isOn());
}

TEST_F(RelayFixture, manualForcedOffModeIsRestored) {
  ::testing::NiceMock<ConfigMock> cfg;
  storage.defaultInitialization(5);
  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        memset(data, 0, sizeof(uint32_t));
        return sizeof(uint32_t);
      });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        Supla::Control::Relay::RelayFlags flags;
        flags.flags.relayOn = 1;
        flags.flags.operatingMode = RELAY_STORED_MODE_FORCED_OFF;
        *data = flags.rawValue;
        return sizeof(uint8_t);
      });
  relay.onLoadState();

  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);
  int relayPinValue = 1;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  relay.onInit();
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture, overcurrentKeepsRestoredManualForcedOnPhysicallyOff) {
  ::testing::NiceMock<ConfigMock> cfg;
  storage.defaultInitialization(5);
  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        memset(data, 0, sizeof(uint32_t));
        return sizeof(uint32_t);
      });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        Supla::Control::Relay::RelayFlags flags;
        flags.flags.overcurrent = 1;
        flags.flags.operatingMode = RELAY_STORED_MODE_FORCED_ON;
        *data = flags.rawValue;
        return sizeof(uint8_t);
      });
  relay.onLoadState();

  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_TRUE(relay.getChannel()->isRelayOvercurrentCutOff());
  int relayPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  relay.onInit();

  EXPECT_FALSE(relay.isOn());
  EXPECT_TRUE(relay.getChannel()->isRelayOvercurrentCutOff());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);
}

TEST_F(RelayFixture,
       manualForcedModeIsAdvertisedButRejectedForImpulseFunction) {
  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  EXPECT_NE(relay.getChannel()->getFlags() &
                SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED,
            0);
  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_ON), 0);
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
}

TEST_F(RelayFixture, explicitManualForcedOnClearsOvercurrentCutoff) {
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  int relayPinValue = 0;
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);
  relay.onInit();
  relay.getChannel()->setRelayOvercurrentCutOff(true);

  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_ON), 1);
  EXPECT_FALSE(relay.getChannel()->isRelayOvercurrentCutOff());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_TRUE(relay.isOn());

  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_OFF), 1);
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture, weeklyScheduleControllerCannotChangeAfterConfigLoad) {
  ::testing::NiceMock<ConfigMock> cfg;
  ON_CALL(cfg, getBlobSize(_)).WillByDefault(Return(-1));
  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  auto *external =
      new Supla::Control::ExternalManagedWeeklySchedule();
  EXPECT_FALSE(relay.setWeeklyScheduleController(external));
  delete external;
  EXPECT_TRUE(relay.getChannel()->isWeeklyScheduleAvailable());
}

TEST_F(RelayFixture, missingWeeklyScheduleKeepsLegacyRelayBehavior) {
  ::testing::NiceMock<ConfigMock> cfg;
  EXPECT_CALL(cfg, getBlob(_, _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);
  EXPECT_CALL(cfg, setBlob(_, _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.onLoadConfig(nullptr);

  EXPECT_FALSE(relay.getChannel()->isWeeklyScheduleAvailable());
  const auto *value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);

  TSD_ChannelConfig emptyConfig = {};
  emptyConfig.Func = SUPLA_CHANNELFNC_LIGHTSWITCH;
  emptyConfig.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  EXPECT_EQ(relay.handleWeeklySchedule(&emptyConfig, false, false),
            SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED);

  TChannelConfig_WeeklySchedule schedule;
  memset(&schedule, 0xA5, sizeof(schedule));
  int size = -1;
  relay.fillChannelConfig(
      &schedule, &size, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  EXPECT_EQ(size, 0);

  value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
}

TEST_F(RelayFixture, relayClassCanDefineItsDefaultWeeklySchedule) {
  RelayWithCustomWeeklySchedule relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);

  TChannelConfig_WeeklySchedule schedule;
  memset(&schedule, 0xA5, sizeof(schedule));
  int size = -1;
  relay.fillChannelConfig(
      &schedule, &size, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);

  ASSERT_EQ(size, sizeof(TChannelConfig_WeeklySchedule));
  EXPECT_EQ(schedule.Program[0].Mode, SUPLA_RELAY_MODE_FORCED_OFF);
  EXPECT_EQ(Supla::Control::getWeeklyScheduleProgramId(&schedule, 0), 1);
  EXPECT_EQ(Supla::Control::getWeeklyScheduleProgramId(&schedule, 1), 0);
}

TEST_F(RelayFixture, noOpWeeklyScheduleIsStoredWithoutEnablingSchedule) {
  ::testing::NiceMock<ConfigMock> cfg;
  ON_CALL(cfg, getBlobSize(_)).WillByDefault(Return(-1));

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  auto configured = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH, SUPLA_RELAY_MODE_START_OFF);
  EXPECT_EQ(relay.handleChannelConfig(&configured, true),
            SUPLA_CONFIG_RESULT_TRUE);
  auto *value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_r_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([](const char *, const char *data, size_t size) {
        const TChannelConfig_WeeklySchedule expected = {};
        EXPECT_EQ(size, sizeof(expected));
        EXPECT_EQ(memcmp(data, &expected, sizeof(expected)), 0);
        return true;
      });
  EXPECT_CALL(cfg, saveWithDelay(5000)).Times(1);

  TSD_ChannelConfig noOp = {};
  noOp.Func = SUPLA_CHANNELFNC_LIGHTSWITCH;
  noOp.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  noOp.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  EXPECT_EQ(relay.handleWeeklySchedule(&noOp, false, true),
            SUPLA_CONFIG_RESULT_TRUE);

  value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);

  TSD_SuplaChannelNewValue newValue = {};
  reinterpret_cast<TRelayChannel_Value *>(newValue.value)->RelayMode =
      SUPLA_RELAY_MODE_CMD_WEEKLY_SCHEDULE;
  EXPECT_EQ(relay.handleNewValueFromServer(&newValue), 1);
  value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
}

TEST_F(RelayFixture, storedWeeklyScheduleLoadsOnlyAfterEnableCommand) {
  ClockStub clock;
  ::testing::NiceMock<ConfigMock> cfg;
  auto configured = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH, SUPLA_RELAY_MODE_FORCED_ON);

  EXPECT_CALL(cfg, getBlobSize(StrEq("0_r_weekly")))
      .WillOnce(Return(sizeof(TChannelConfig_WeeklySchedule)));
  EXPECT_CALL(cfg, getBlob(_, _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  auto *value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);

  ::testing::Mock::VerifyAndClearExpectations(&cfg);
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_r_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([&configured](const char *, char *data, size_t size) {
        memcpy(data, configured.Config, size);
        return true;
      });

  enableWeeklySchedule(&relay);

  value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);

  time.advance(1000);
  relay.iterateAlways();

  value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);
}

TEST_F(RelayFixture, invalidStoredWeeklyScheduleFallsBackToManualMode) {
  ClockStub clock;
  ::testing::NiceMock<ConfigMock> cfg;
  TChannelConfig_WeeklySchedule invalidSchedule = {};
  invalidSchedule.Program[0].Mode = 0xFF;

  EXPECT_CALL(cfg, getBlobSize(StrEq("0_r_weekly")))
      .WillOnce(Return(sizeof(TChannelConfig_WeeklySchedule)));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_r_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([&invalidSchedule](const char *, char *data, size_t size) {
        memcpy(data, &invalidSchedule, size);
        return true;
      });

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);
  TSD_SuplaChannelNewValue newValue = {};
  reinterpret_cast<TRelayChannel_Value *>(newValue.value)->RelayMode =
      SUPLA_RELAY_MODE_CMD_WEEKLY_SCHEDULE;
  EXPECT_EQ(relay.handleNewValueFromServer(&newValue), 0);

  time.advance(1000);
  relay.iterateAlways();

  const auto *value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);

  relay.iterateAlways();
}

TEST_F(RelayFixture, weeklyScheduleModeIsStoredInRelayState) {
  ::testing::NiceMock<ConfigMock> cfg;
  ON_CALL(cfg, getBlobSize(_)).WillByDefault(Return(-1));
  storage.defaultInitialization(5);

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  auto config = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH, SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);

  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&storage));
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce(Return(0));
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillOnce(Return(0));
  EXPECT_CALL(storage, scheduleSave(5000, 2000)).Times(1);
  enableWeeklySchedule(&relay);

  EXPECT_CALL(storage, writeStorage(_, _, sizeof(uint32_t)))
      .WillOnce(Return(sizeof(uint32_t)));
  EXPECT_CALL(storage, writeStorage(_, _, sizeof(uint8_t)))
      .WillOnce([](uint32_t, const unsigned char *data, int32_t) {
        Supla::Control::Relay::RelayFlags flags;
        flags.rawValue = *data;
        EXPECT_EQ(flags.flags.weeklySchedule, 1);
        return sizeof(uint8_t);
      });
  relay.onSaveState();
}

TEST_F(RelayFixture, restoredWeeklyScheduleWaitsForClock) {
  ::testing::NiceMock<ConfigMock> cfg;
  storage.defaultInitialization(5);
  auto configured = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH, SUPLA_RELAY_MODE_FORCED_ON);

  EXPECT_CALL(cfg, getBlobSize(StrEq("0_r_weekly")))
      .WillOnce(Return(sizeof(TChannelConfig_WeeklySchedule)));
  EXPECT_CALL(cfg, getBlob(_, _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        memset(data, 0, sizeof(uint32_t));
        return sizeof(uint32_t);
      });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        Supla::Control::Relay::RelayFlags flags;
        flags.flags.weeklySchedule = 1;
        *data = flags.rawValue;
        return sizeof(uint8_t);
      });
  relay.onLoadState();

  int relayPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  relay.onInit();

  ASSERT_FALSE(Supla::Clock::IsReady());
  auto value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);

  time.advance(5000);
  relay.iterateAlways();
  EXPECT_EQ(relayPinValue, 0);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&cfg));

  EXPECT_CALL(cfg,
              getBlob(StrEq("0_r_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([&configured](const char *, char *data, size_t size) {
        memcpy(data, configured.Config, size);
        return true;
      });
  ClockStub clock;
  relay.iterateAlways();
  EXPECT_EQ(relayPinValue, 1);
}

TEST_F(RelayFixture, restoredWeeklyScheduleWaitsForClockAfterTimeout) {
  ::testing::NiceMock<ConfigMock> cfg;
  storage.defaultInitialization(5);
  auto configured = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH, SUPLA_RELAY_MODE_FORCED_ON);

  EXPECT_CALL(cfg, getBlobSize(StrEq("0_r_weekly")))
      .WillOnce(Return(sizeof(TChannelConfig_WeeklySchedule)));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_r_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([&configured](const char *, char *data, size_t size) {
        memcpy(data, configured.Config, size);
        return true;
      });

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);

  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint32_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        memset(data, 0, sizeof(uint32_t));
        return sizeof(uint32_t);
      });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillOnce([](uint32_t, unsigned char *data, int32_t, bool) {
        Supla::Control::Relay::RelayFlags flags;
        flags.flags.weeklySchedule = 1;
        *data = flags.rawValue;
        return sizeof(uint8_t);
      });
  relay.onLoadState();

  int relayPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  relay.onInit();

  ASSERT_FALSE(Supla::Clock::IsReady());
  time.advance(30000);
  relay.iterateAlways();
  EXPECT_EQ(relayPinValue, 0);

  time.advance(1);
  relay.iterateAlways();
  EXPECT_EQ(relayPinValue, 0);
  auto value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
  ClockStub clock;
  relay.iterateAlways();
  EXPECT_EQ(relayPinValue, 1);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);
}

TEST_F(RelayFixture, weeklyScheduleStartOnTriggersOnlyOnTransition) {
  ClockStub clock;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  EXPECT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_LIGHTSWITCH));
  relay.onLoadConfig(nullptr);

  int relayPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));

  relay.onInit();

  auto config = makeTransitionWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                             SUPLA_RELAY_MODE_START_OFF,
                                             SUPLA_RELAY_MODE_START_ON);
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  auto value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  enableWeeklySchedule(&relay);

  TChannelConfig_WeeklySchedule stored = {};
  int storedSize = 0;
  relay.fillChannelConfig(
      &stored, &storedSize, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  EXPECT_EQ(storedSize, sizeof(TChannelConfig_WeeklySchedule));
  EXPECT_EQ(
      memcmp(&stored, config.Config, sizeof(TChannelConfig_WeeklySchedule)), 0);
  EXPECT_FALSE(relay.getChannel()->isRelayOvercurrentCutOff());

  time.advance(1000);
  EXPECT_TRUE(Supla::Clock::IsReady());
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());

  ::testing::Mock::VerifyAndClearExpectations(&ioMock);
  ON_CALL(ioMock, digitalRead(0))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  time.advance(15 * 60 * 1000);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, weeklyScheduleStartOnDoesNotRetriggerImpulse) {
  ClockStub clock;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  ASSERT_TRUE(
      relay.setAndSaveFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE));
  relay.onLoadConfig(nullptr);

  int relayPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  relay.onInit();

  auto config = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, SUPLA_RELAY_MODE_START_ON);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());

  time.advance(501);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture,
       functionChangeToImpulseDisablesIncompatibleActiveSchedule) {
  ClockStub clock;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  ASSERT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_LIGHTSWITCH));
  relay.onLoadConfig(nullptr);

  int relayPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  relay.onInit();

  auto config = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_LIGHTSWITCH, SUPLA_RELAY_MODE_FORCED_ON);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  time.advance(1000);
  relay.iterateAlways();
  ASSERT_TRUE(relay.isOn());

  ASSERT_TRUE(
      relay.setAndSaveFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE));
  const auto *value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
  EXPECT_FALSE(relay.isOn());

  TSD_SuplaChannelNewValue weeklyValue = {};
  reinterpret_cast<TRelayChannel_Value *>(weeklyValue.value)->RelayMode =
      SUPLA_RELAY_MODE_CMD_WEEKLY_SCHEDULE;
  EXPECT_EQ(relay.handleNewValueFromServer(&weeklyValue), 0);

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture, weeklyScheduleForcedOnKeepsStaircaseOnWithoutTimer) {
  ClockStub clock;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  ASSERT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_STAIRCASETIMER));
  relay.setDefaultStaircaseDurationMs(500);
  relay.onLoadConfig(nullptr);

  int relayPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  relay.onInit();
  relay.turnOn();
  ASSERT_TRUE(relay.isOn());
  uint32_t remainingSec = 0;
  ASSERT_TRUE(relay.getRemainingCountdownTimerSec(&remainingSec));

  auto config = makeSingleProgramWeeklySchedule(
      SUPLA_CHANNELFNC_STAIRCASETIMER, SUPLA_RELAY_MODE_FORCED_ON);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
  EXPECT_FALSE(relay.getRemainingCountdownTimerSec(&remainingSec));

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());

  TSD_SuplaChannelNewValue manualValue = {};
  reinterpret_cast<TRelayChannel_Value *>(manualValue.value)->RelayMode =
      SUPLA_RELAY_MODE_CMD_SWITCH_TO_MANUAL;
  ASSERT_EQ(relay.handleNewValueFromServer(&manualValue), 1);
  relay.turnOff();
  relay.turnOn();
  ASSERT_TRUE(relay.isOn());

  time.advance(501);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture,
       weeklyScheduleForcedOnBlocksManualOffButStillAppliesState) {
  ClockStub clock;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT)).Times(AtLeast(1));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  EXPECT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_LIGHTSWITCH));
  relay.onLoadConfig(nullptr);

  int relayPinValue = 0;
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));

  relay.onInit();

  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  EXPECT_FALSE(relay.isOn());
  auto value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);

  TChannelConfig_WeeklySchedule stored = {};
  int storedSize = 0;
  relay.fillChannelConfig(
      &stored, &storedSize, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  EXPECT_EQ(storedSize, sizeof(TChannelConfig_WeeklySchedule));
  EXPECT_EQ(
      memcmp(&stored, config.Config, sizeof(TChannelConfig_WeeklySchedule)), 0);
  EXPECT_TRUE(relay.isWeeklyScheduleSupported());
  EXPECT_FALSE(relay.getChannel()->isRelayOvercurrentCutOff());
  EXPECT_TRUE(relay.isFullyInitialized());

  time.advance(1000);
  EXPECT_TRUE(Supla::Clock::IsReady());
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());

  ::testing::Mock::VerifyAndClearExpectations(&ioMock);
  TSD_SuplaChannelNewValue newValue = {};
  newValue.value[0] = 0;
  EXPECT_EQ(relay.handleNewValueFromServer(&newValue), 0);
  EXPECT_TRUE(relay.isOn());

  relay.handleAction(0, Supla::TURN_OFF);
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, weeklyScheduleForcedOffBlocksManualOn) {
  ClockStub clock;
  EXPECT_CALL(ioMock, digitalWrite(1, 0)).Times(2);
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  EXPECT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_LIGHTSWITCH));
  relay.onLoadConfig(nullptr);
  relay.onInit();

  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_FORCED_OFF);
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());

  ::testing::Mock::VerifyAndClearExpectations(&ioMock);
  TSD_SuplaChannelNewValue newValue = {};
  newValue.value[0] = 1;
  EXPECT_EQ(relay.handleNewValueFromServer(&newValue), 0);
  EXPECT_FALSE(relay.isOn());

  relay.handleAction(0, Supla::TURN_ON);
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture, weeklyForcedOnDoesNotClearOvercurrentCutoff) {
  ClockStub clock;
  EXPECT_CALL(ioMock, digitalWrite(1, 0)).Times(2);
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);
  relay.onInit();
  relay.getChannel()->setRelayOvercurrentCutOff(true);

  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_FORCED_ON);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  time.advance(1000);
  relay.iterateAlways();

  EXPECT_TRUE(relay.getChannel()->isRelayOvercurrentCutOff());
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture, ordinaryCommandsDoNotDisableWeeklyForcedMode) {
  ClockStub clock;
  int relayPinValue = 0;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));

  Supla::Control::Relay relay(1);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  relay.setWeeklyScheduleAvailable();
  relay.onLoadConfig(nullptr);
  relay.onInit();

  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_FORCED_OFF);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  time.advance(1000);
  relay.iterateAlways();
  ASSERT_TRUE(relayValue(relay)->flags &
              SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  ASSERT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);

  TSD_SuplaChannelNewValue turnOn = {};
  auto *turnOnValue =
      reinterpret_cast<TRelayChannel_Value *>(turnOn.value);
  turnOnValue->hi = 1;
  turnOnValue->flags = SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED;
  turnOnValue->RelayMode = SUPLA_RELAY_MODE_FORCED_OFF;
  EXPECT_EQ(relay.handleNewValueFromServer(&turnOn), 0);
  EXPECT_TRUE(relayValue(relay)->flags &
              SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);
  EXPECT_FALSE(relay.isOn());

  TSD_SuplaChannelNewValue turnOff = {};
  auto *turnOffValue =
      reinterpret_cast<TRelayChannel_Value *>(turnOff.value);
  turnOffValue->flags = SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED;
  turnOffValue->RelayMode = SUPLA_RELAY_MODE_FORCED_OFF;
  EXPECT_EQ(relay.handleNewValueFromServer(&turnOff), 1);
  EXPECT_TRUE(relayValue(relay)->flags &
              SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);
  EXPECT_FALSE(relay.isOn());

  EXPECT_EQ(sendRelayMode(&relay, SUPLA_RELAY_MODE_FORCED_OFF), 1);
  EXPECT_FALSE(relayValue(relay)->flags &
               SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(relayValue(relay)->RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture, weeklyScheduleForcedOffCancelsTimerTurnOn) {
  ClockStub clock;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  EXPECT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_LIGHTSWITCH));
  relay.onLoadConfig(nullptr);

  int relayPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  relay.onInit();

  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_FORCED_OFF);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  time.advance(1000);
  relay.iterateAlways();
  ASSERT_FALSE(relay.isOn());

  TSD_SuplaChannelNewValue newValue = {};
  newValue.DurationMS = 1000;
  ASSERT_EQ(relay.handleNewValueFromServer(&newValue), 1);
  ASSERT_FALSE(relay.isOn());

  time.advance(1001);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());

  time.advance(1001);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture, weeklyScheduleForcedOnCancelsTimerTurnOff) {
  ClockStub clock;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  EXPECT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_LIGHTSWITCH));
  relay.onLoadConfig(nullptr);

  int relayPinValue = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayPinValue));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayPinValue));
  relay.onInit();

  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_FORCED_ON);
  ASSERT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);
  time.advance(1000);
  relay.iterateAlways();
  ASSERT_TRUE(relay.isOn());

  TSD_SuplaChannelNewValue newValue = {};
  newValue.value[0] = 1;
  newValue.DurationMS = 1000;
  ASSERT_EQ(relay.handleNewValueFromServer(&newValue), 1);
  ASSERT_TRUE(relay.isOn());

  time.advance(1001);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());

  time.advance(1001);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, weeklyScheduleNotSetDoesNotChangeState) {
  ClockStub clock;
  EXPECT_CALL(ioMock, digitalWrite(1, 0)).Times(2);
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  EXPECT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_LIGHTSWITCH));
  relay.onInit();

  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_NOT_SET);
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);

  time.advance(1000);
  ::testing::Mock::VerifyAndClearExpectations(&ioMock);
  relay.iterateAlways();
  EXPECT_FALSE(relay.isOn());
}

TEST_F(RelayFixture, weeklyScheduleReportsModeAndSwitchesToManualAndBack) {
  ClockStub clock;
  EXPECT_CALL(ioMock, pinMode(1, OUTPUT)).Times(AtLeast(1));

  Supla::Control::Relay relay(1);
  relay.setWeeklyScheduleAvailable();
  EXPECT_TRUE(relay.setAndSaveFunction(SUPLA_CHANNELFNC_LIGHTSWITCH));
  relay.onLoadConfig(nullptr);

  int relayValueState = 0;
  EXPECT_CALL(ioMock, digitalRead(1)).Times(::testing::AnyNumber());
  EXPECT_CALL(ioMock, digitalWrite(1, _)).Times(::testing::AnyNumber());
  ON_CALL(ioMock, digitalRead(1))
      .WillByDefault(::testing::ReturnPointee(&relayValueState));
  ON_CALL(ioMock, digitalWrite(1, _))
      .WillByDefault(::testing::SaveArg<1>(&relayValueState));

  relay.onInit();

  auto config = makeSingleProgramWeeklySchedule(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                                SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_EQ(relay.handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  enableWeeklySchedule(&relay);

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_TRUE(relay.isOn());

  auto value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);

  TSD_SuplaChannelNewValue newValue = {};
  reinterpret_cast<TRelayChannel_Value *>(newValue.value)->RelayMode =
      SUPLA_RELAY_MODE_CMD_SWITCH_TO_MANUAL;
  EXPECT_EQ(relay.handleNewValueFromServer(&newValue), 1);

  value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_NOT_SET);
  EXPECT_TRUE(relay.isOn());

  reinterpret_cast<TRelayChannel_Value *>(newValue.value)->RelayMode =
      SUPLA_RELAY_MODE_CMD_WEEKLY_SCHEDULE;
  EXPECT_EQ(relay.handleNewValueFromServer(&newValue), 1);

  value = relayValue(relay);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(value->flags & SUPLA_RELAY_FLAG_WEEKLY_SCHEDULE_ENABLED);
  EXPECT_EQ(value->RelayMode, SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_TRUE(relay.isOn());
}

TEST_F(RelayFixture, stateOnInitTests) {
  int gpio1 = 1;
  int gpio2 = 2;
  int gpio3 = 3;
  Supla::Control::Relay r1(gpio1);
  Supla::Control::Relay r2(gpio2);
  Supla::Control::Relay r3(gpio3);

  r1.setDefaultStateOn();
  r2.setDefaultStateOff();
  r3.setDefaultStateRestore();

  storage.defaultInitialization(3 * 5);

  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpio1Value = 0;
  int gpio2Value = 0;
  int gpio3Value = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio1))
      .WillRepeatedly(::testing::ReturnPointee(&gpio1Value));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpio1Value));
  EXPECT_CALL(ioMock, digitalRead(gpio2))
      .WillRepeatedly(::testing::ReturnPointee(&gpio2Value));
  EXPECT_CALL(ioMock, digitalWrite(gpio2, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpio2Value));
  EXPECT_CALL(ioMock, digitalRead(gpio3))
      .WillRepeatedly(::testing::ReturnPointee(&gpio3Value));
  EXPECT_CALL(ioMock, digitalWrite(gpio3, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpio3Value));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(1, 0, 0, 0));

  // virtual Relay &keepTurnOnDuration(bool keep = true);
  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  EXPECT_EQ(gpio1Value, 1);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpio1Value, 1);

  // R2
  // init
  EXPECT_CALL(ioMock, pinMode(gpio2, OUTPUT));

  r2.onLoadConfig(nullptr);
  r2.onLoadState();
  r2.onInit();
  r2.onRegistered(nullptr);
  EXPECT_EQ(gpio2Value, 0);

  for (int i = 0; i < 10; i++) {
    r2.iterateAlways();
    r2.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpio2Value, 0);

  // R3
  // init
  EXPECT_CALL(ioMock, pinMode(gpio3, OUTPUT));

  r3.onLoadConfig(nullptr);
  r3.onLoadState();
  r3.onInit();
  EXPECT_EQ(gpio3Value, 1);

  for (int i = 0; i < 10; i++) {
    r3.iterateAlways();
    r3.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpio3Value, 1);
}

TEST_F(RelayFixture, startupTestsForLight) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);

  // data is read from storage, but it is not used by Relay
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 1000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));  //
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(gpioValue, 0);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  value[0] = 1;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_OFF);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  value[0] = 0;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  value[0] = 1;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);
  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);
  r1.toggle();
  EXPECT_EQ(gpioValue, 0);
  r1.toggle();
  EXPECT_EQ(gpioValue, 1);

  EXPECT_EQ(1, r1.isOn());

  // countdown timer checks
  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 2; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // check if duration wasn't stored
  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // Scenario1, from server:
  // turn on, turn off, turn off, turn on, turn on
  newValueFromServer.DurationMS = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnedOnTurnOffandTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnedOnTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // First we start with turnOff(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);

  // turnOff(2s) and then in the middle, turnOn(0)
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // turnOff(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // turnOff(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // we start with turnOn(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  //
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // turnOn(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // turnOn(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // turnOn(2s) and then in the middle, turnOn(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, turnOff(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, toggle(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  r1.toggle();
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
}

TEST_F(RelayFixture, durationMsTests) {
  int gpio1 = 1;
  Supla::Control::Relay r1(gpio1);

  r1.setDefaultStateOn();

  // duration should be ignored, becuase keepTurnOnDuration is not enabled
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));

  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(gpioValue, 1);
  r1.onRegistered(nullptr);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  r1.turnOn(1000);  // turn on for 1000 ms
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 15; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOn(200);  // turn on for 200 ms
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 5; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOn(1000);  // turn on for 1000 ms
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // additional turn on 2000 ms, when first is turn on is not completed yet
  // it should restart timer and turn off after 2 s
  r1.turnOn(2000);  // turn on for 2000 ms
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOff(1000);
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  ////////////////////////////////////////////////
  // Check handling of "new value from server"
  ////////////////////////////////////////////////

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.value[0] = 2;  // invalid value
  EXPECT_EQ(-1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.value[0] = 0;  // turn off with duration 1s - which is
                                    // currently ignored by s-d
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // there is no "keepTurnOnDuration" enabled
  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // when countdown timer was introduced, keepTurnOnDuration() method was
  // deprecated and it doesn't have any effect, so instead of it, we
  // send configuration vie channel config message
  r1.keepTurnOnDuration(true);
  TSD_ChannelConfig result = {};
  result.Func = SUPLA_CHANNELFNC_STAIRCASETIMER;
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_StaircaseTimer);
  TChannelConfig_StaircaseTimer *config =
      reinterpret_cast<TChannelConfig_StaircaseTimer *>(&result.Config);
  config->TimeMS = 1000;
  r1.handleChannelConfig(&result, false);

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // "keepTurnOnDuration" is enabled, so it should use 1000 ms as duration
  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // "keepTurnOnDuration" is enabled, so it should use 1000 ms as duration
  r1.turnOn(2000);
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
}

TEST_F(RelayFixture, TimerDurationKeepsInt32Range) {
  const int gpio = 1;
  Supla::Control::Relay relay(gpio);
  int gpioValue = 0;

  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  relay.onInit();

  const int32_t durations[] = {0, 1000, 32767, 32768, 60000, 120000};
  for (int32_t duration : durations) {
    relay.turnOn(duration);
    EXPECT_EQ(gpioValue, 1);

    if (duration == 0) {
      time.advance(120001);
      relay.iterateAlways();
      EXPECT_EQ(gpioValue, 1);
      continue;
    }

    time.advance(duration);
    relay.iterateAlways();
    EXPECT_EQ(gpioValue, 1);

    time.advance(1);
    relay.iterateAlways();
    EXPECT_EQ(gpioValue, 0);
  }
}

TEST_F(RelayFixture, CountdownTimerStartedAtMillisZeroRemainsActive) {
  const int gpio = 1;
  Supla::Control::Relay relay(gpio);
  relay.setDefaultStateRestore();
  storage.defaultInitialization(5);

  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  relay.onLoadState();
  relay.onInit();
  relay.onRegistered(nullptr);

  uint32_t remainingSec = 0;
  relay.turnOn(60000);
  EXPECT_TRUE(relay.getRemainingCountdownTimerSec(&remainingSec));
  EXPECT_EQ(remainingSec, 60u);

  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 59999, 0, 0));
  EXPECT_FALSE(relay.iterateConnected());

  time.advance(1000);
  EXPECT_TRUE(relay.getRemainingCountdownTimerSec(&remainingSec));
  EXPECT_EQ(remainingSec, 59u);

  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 58999u);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(RELAY_FLAGS_ON), 1))
      .WillOnce(Return(1));
  relay.onSaveState();

  relay.turnOff();
  EXPECT_FALSE(relay.getRemainingCountdownTimerSec(&remainingSec));
}

TEST_F(RelayFixture, CyclicModeServerOffZeroStopsButOffWithDurationContinues) {
  const int gpio = 1;
  Supla::Control::Relay relay(gpio);
  int gpioValue = 0;

  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  relay.enableCyclicMode(1000, 5000);
  relay.onInit();
  time.advance(1);

  TSD_SuplaChannelNewValue newValue = {};
  newValue.ChannelNumber = relay.getChannelNumber();
  newValue.value[0] = 0;
  newValue.DurationMS = 5000;

  EXPECT_EQ(1, relay.handleNewValueFromServer(&newValue));
  EXPECT_EQ(0, gpioValue);

  time.advance(4999);
  relay.iterateAlways();
  EXPECT_EQ(0, gpioValue);

  time.advance(1);
  relay.iterateAlways();
  EXPECT_EQ(0, gpioValue);

  time.advance(1);
  relay.iterateAlways();
  EXPECT_EQ(1, gpioValue);

  newValue.DurationMS = 0;
  EXPECT_EQ(1, relay.handleNewValueFromServer(&newValue));
  EXPECT_EQ(0, gpioValue);

  time.advance(10000);
  relay.iterateAlways();
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, CountdownTimerRemainingConditionFiresOnceOnThreshold) {
  int gpio = 1;
  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  CountingActionHandler actionCounter;
  Supla::Control::Relay relay(gpio);
  relay.onInit();

  relay.addAction(Supla::TURN_ON,
                  actionCounter,
                  Supla::ON_COUNTDOWN_TIMER,
                  OnLessEq(2, CountdownTimerRemainingSec()));

  time.advance(1);
  relay.turnOn(3000);

  relay.iterateAlways();
  EXPECT_EQ(actionCounter.count, 0);

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_EQ(actionCounter.count, 1);
  EXPECT_EQ(actionCounter.lastEvent, Supla::ON_COUNTDOWN_TIMER);
  EXPECT_EQ(actionCounter.lastAction, Supla::TURN_ON);

  time.advance(1000);
  relay.iterateAlways();
  EXPECT_EQ(actionCounter.count, 1);
}

TEST_F(RelayFixture, InactiveCountdownTimerDoesNotFireCondition) {
  int gpio = 1;
  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  CountingActionHandler actionCounter;
  Supla::Control::Relay relay(gpio);
  relay.onInit();

  relay.addAction(Supla::TURN_ON,
                  actionCounter,
                  Supla::ON_COUNTDOWN_TIMER,
                  OnLess(60, CountdownTimerRemainingSec()));

  for (int i = 0; i < 3; i++) {
    relay.iterateAlways();
    time.advance(1000);
  }

  EXPECT_EQ(actionCounter.count, 0);
}

TEST_F(RelayFixture, CountdownTimerConditionResetsAfterInactiveTransition) {
  int gpio = 1;
  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  CountingActionHandler actionCounter;
  Supla::Control::Relay relay(gpio);
  relay.onInit();

  relay.addAction(Supla::TURN_ON,
                  actionCounter,
                  Supla::ON_COUNTDOWN_TIMER,
                  OnLessEq(1, CountdownTimerRemainingSec()));

  time.advance(1);
  relay.turnOn(1500);
  time.advance(600);
  relay.iterateAlways();
  EXPECT_EQ(actionCounter.count, 1);

  relay.turnOff();
  relay.iterateAlways();
  EXPECT_EQ(actionCounter.count, 1);

  time.advance(1);
  relay.turnOn(1500);
  time.advance(600);
  relay.iterateAlways();
  EXPECT_EQ(actionCounter.count, 2);
}

TEST_F(RelayFixture, keepTurnOnDurationRestoreOnTests) {
  int gpio1 = 1;
  Supla::Control::Relay r1(gpio1);

  r1.setDefaultStateRestore();
  r1.keepTurnOnDuration(true);

  storage.defaultInitialization(5);

  unsigned char storedRelayFlags = RELAY_FLAGS_ON | RELAY_FLAGS_STAIRCASE;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(gpioValue, 1);
  r1.onRegistered(nullptr);

  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOn(5000);
  EXPECT_EQ(gpioValue, 1);

  // turn off will happen after 2.5s because of stored turn on duration
  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // turn off will last 1s, then it will turn on and off after timeout
  r1.turnOff(1000);
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 15; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 17; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
}

TEST_F(RelayFixture, keepTurnOnDurationRestoreOffTests) {
  int gpio1 = 1;
  Supla::Control::Relay r1(gpio1);

  r1.setDefaultStateRestore();
  r1.keepTurnOnDuration(true);

  storage.defaultInitialization(5);

  unsigned char storedRelayFlags = RELAY_FLAGS_STAIRCASE;  // ON flag is not set
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);

  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  r1.turnOn(5000);
  EXPECT_EQ(1, gpioValue);

  // turn off will happen after 2.5s because of stored turn on duration
  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  r1.turnOff(1000);
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  r1.turnOff();
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreTimerOn) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  time.advance(100);
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(1, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // make sure that duration wasn't remebered
  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreTimerOff) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  time.advance(100);
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // make sure that duration wasn't remebered
  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreOn) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(1, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreOff) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, checkTimerStateStorageForLight) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      // initial read in onLoadState
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      // read before first write
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      // read before second write
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 900;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      // initial read in onLoadState
      .WillOnce(DoAll(SetArgPointee<1>(0), Return(1)))
      // read before first write
      .WillOnce(DoAll(SetArgPointee<1>(0), Return(1)))
      // read before second write
      .WillOnce(DoAll(SetArgPointee<1>(1), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = 1;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 900);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));
  // save
  relayFlags = 0;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 0);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~1s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  // duration should be 0
  r1.onSaveState();
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreOnButConfiguredToOff) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateOff();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, checkTimerStateStorageForPowerSwitch) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 900;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillOnce(DoAll(SetArgPointee<1>(0), Return(1)))
      .WillOnce(DoAll(SetArgPointee<1>(0), Return(1)))
      .WillOnce(DoAll(SetArgPointee<1>(1), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = 1;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 900);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));
  // save
  relayFlags = 0;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 0);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_POWERSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~1s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  // duration should be 0
  r1.onSaveState();
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, checkTimerStateStorageForStaircaseTimer) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = RELAY_FLAGS_ON | RELAY_FLAGS_STAIRCASE;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 4000);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));
  // save
  relayFlags = RELAY_FLAGS_STAIRCASE;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 4000);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_STAIRCASETIMER, 4000);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~1s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  // duration should be 0
  r1.onSaveState();
}

TEST_F(RelayFixture, RelayStorageSaveDelayIsClampedBeforeScheduling) {
  Supla::Control::Relay relay(0);

  EXPECT_CALL(storage, scheduleSave(5000, 2000)).Times(2);
  EXPECT_CALL(storage, scheduleSave(60000, 2000));
  EXPECT_CALL(storage, scheduleSave(UINT16_MAX, 2000));

  Supla::Control::Relay::setRelayStorageSaveDelay(5000);
  sendConfig(&relay, SUPLA_CHANNELFNC_STAIRCASETIMER, 1000);

  Supla::Control::Relay::setRelayStorageSaveDelay(60000);
  sendConfig(&relay, SUPLA_CHANNELFNC_STAIRCASETIMER, 2000);

  Supla::Control::Relay::setRelayStorageSaveDelay(UINT16_MAX + 1u);
  sendConfig(&relay, SUPLA_CHANNELFNC_STAIRCASETIMER, 3000);

  Supla::Control::Relay::setRelayStorageSaveDelay(5000);
}

TEST_F(RelayFixture, checkTimerStateStorageForImpulseFunction) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = RELAY_FLAGS_ON | RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 500);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));
  // save
  relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 500);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  // currently impulse functions doesn't have channel config Config field
  // so it still relies on durationMs send in new value
  sendConfig(&r1, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, 500);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 500;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~0.3s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  r1.onSaveState();

  // check TOGGLE behavior
  r1.setRestartTimerOnToggle(true);
  r1.handleAction(0, Supla::TOGGLE);
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 4; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  r1.handleAction(0, Supla::TOGGLE);  // reset timer
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 4; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  r1.handleAction(0, Supla::TOGGLE);  // reset timer
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 4; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  r1.handleAction(0, Supla::TOGGLE);  // reset timer
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);  // timer elapsed, so relay off

  // check TOGGLE behavior (false)
  r1.setRestartTimerOnToggle(false);
  r1.handleAction(0, Supla::TOGGLE);
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 4; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  r1.handleAction(0, Supla::TOGGLE);  // standard toggle -> turn off
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, checkTimerStateStorageForImpulseFunctionOnLoad) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION), Return(1)))
      .WillOnce(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION), Return(1)))
      .WillOnce(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION), Return(1)))
      .WillOnce(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION | RELAY_FLAGS_ON),
                Return(1)));

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION | RELAY_FLAGS_ON;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 500);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // save
  relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  Supla::Storage::LoadStateStorage();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  // currently impulse functions doesn't have channel config Config field
  // so it still relies on durationMs send in new value
  sendConfig(&r1, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, 500);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  Supla::Storage::WriteStateStorage();

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 500;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~0.3s elapsed, then save state to storage
  Supla::Storage::WriteStateStorage();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  Supla::Storage::WriteStateStorage();
}

TEST_F(RelayFixture, checkTimerStateStorageForImpulseFunctionOnLoadNoRestore) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  storage.defaultInitialization(5);

  // data is read from storage
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      // Load state storage
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      // First write, no change
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      // Second write, change to 500, but we read previous value
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
      // Last write, no change
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION), Return(1)));

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t, const unsigned char *value, int32_t) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t *>(value), 500);
        return 4;
      });

  // test begins
  r1.onLoadConfig(nullptr);
  Supla::Storage::LoadStateStorage();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  // currently impulse functions doesn't have channel config Config field
  // so it still relies on durationMs send in new value
  sendConfig(&r1, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, 500);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  Supla::Storage::WriteStateStorage();

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 500;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~0.3s elapsed, then save state to storage
  Supla::Storage::WriteStateStorage();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  Supla::Storage::WriteStateStorage();
}

// test copied from Light version, just function changed
TEST_F(RelayFixture, startupTestsForPowerSwitch) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  storage.defaultInitialization(5);

  // data is read from storage, but it is not used by Relay
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
      .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_POWERSWITCH, 0);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  value[0] = 1;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_OFF);
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  value[0] = 0;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  value[0] = 1;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  r1.turnOff();
  EXPECT_EQ(0, gpioValue);
  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  r1.toggle();
  EXPECT_EQ(0, gpioValue);
  EXPECT_EQ(0, r1.isOn());
  r1.toggle();
  EXPECT_EQ(1, gpioValue);

  EXPECT_EQ(1, r1.isOn());

  // countdown timer checks
  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // check if duration wasn't stored
  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
  r1.turnOff();
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // Scenario1, from server:
  // turn on, turn off, turn off, turn on, turn on
  newValueFromServer.DurationMS = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnedOnTurnOffandTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnedOnTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // First we start with turnOff(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);

  // turnOff(2s) and then in the middle, turnOn(0)
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // turnOff(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // turnOff(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // we start with turnOn(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  //
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // turnOn(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // turnOn(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // turnOn(2s) and then in the middle, turnOn(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  r1.turnOff();
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, turnOff(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, toggle(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  r1.toggle();
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, hvacRelatedTest) {
  // all relays can be controller locally via turnOn/Off methods, but hvac
  // related (pump, heat or cold) can't be controlled remotely by server
  int gpio1 = 1;
  int gpio2 = 2;
  int gpio3 = 3;
  Supla::Control::Relay r1(gpio1, true, SUPLA_BIT_FUNC_PUMPSWITCH);
  Supla::Control::Relay r2(gpio2, true, SUPLA_BIT_FUNC_HEATORCOLDSOURCESWITCH);
  Supla::Control::Relay r3(gpio3, true, SUPLA_BIT_FUNC_POWERSWITCH);
  r1.setDefaultFunction(SUPLA_CHANNELFNC_PUMPSWITCH);
  r2.setDefaultFunction(SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH);
  r3.setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  int number1 = r1.getChannelNumber();
  int number2 = r2.getChannelNumber();
  int number3 = r3.getChannelNumber();
  ASSERT_EQ(number1, 0);
  ASSERT_EQ(number2, 1);
  ASSERT_EQ(number3, 2);

  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);

  r1.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio2, 0)).Times(1);
  EXPECT_CALL(ioMock, pinMode(gpio2, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio2, 0)).Times(1);

  r2.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio3, 0)).Times(1);
  EXPECT_CALL(ioMock, pinMode(gpio3, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio3, 0)).Times(1);

  r3.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1)).Times(1);
  r1.turnOn();

  EXPECT_CALL(ioMock, digitalWrite(gpio2, 1)).Times(1);
  r2.turnOn();

  EXPECT_CALL(ioMock, digitalWrite(gpio3, 1)).Times(1);
  r3.turnOn();

  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(1);
  r1.turnOff();

  EXPECT_CALL(ioMock, digitalWrite(gpio2, 0)).Times(1);
  r2.turnOff();

  EXPECT_CALL(ioMock, digitalWrite(gpio3, 0)).Times(1);
  r3.turnOff();

  TSD_SuplaChannelNewValue newValueFromServer = {};
  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(0, r1.handleNewValueFromServer(&newValueFromServer));

  newValueFromServer.ChannelNumber = 1;
  EXPECT_EQ(0, r2.handleNewValueFromServer(&newValueFromServer));

  newValueFromServer.ChannelNumber = 2;
  EXPECT_CALL(ioMock, digitalWrite(gpio3, 1)).Times(1);
  EXPECT_EQ(1, r3.handleNewValueFromServer(&newValueFromServer));

  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(0, r1.handleNewValueFromServer(&newValueFromServer));

  newValueFromServer.ChannelNumber = 1;
  EXPECT_EQ(0, r2.handleNewValueFromServer(&newValueFromServer));

  newValueFromServer.ChannelNumber = 2;
  EXPECT_CALL(ioMock, digitalWrite(gpio3, 0)).Times(1);
  EXPECT_EQ(1, r3.handleNewValueFromServer(&newValueFromServer));
}
