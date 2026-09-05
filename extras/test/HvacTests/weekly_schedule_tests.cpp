// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_mock.h>
#include <clock_mock.h>
#include <clock_stub.h>
#include <gtest/gtest.h>
#include <output_mock.h>
#include <protocol_layer_mock.h>
#include <simple_time.h>
#include <stdio.h>
#include <string.h>
#include <supla/control/hvac_base.h>
#include <supla/control/weekly_schedule_buffer.h>
#include <supla/control/weekly_schedule_cache_runtime.h>
#include <supla/control/weekly_schedule_component.h>
#include <supla/control/weekly_schedule_storage.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <supla/sensor/thermometer.h>

#include "gmock/gmock.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

TEST(WeeklyScheduleInfrastructureTests, CacheCanBecomeInactiveAtMillisZero) {
  Supla::Control::WeeklyScheduleCacheRuntime cache;

  cache.touch(false, 0);

  EXPECT_FALSE(cache.process(false, 14999));
  EXPECT_TRUE(cache.process(false, 15000));
}

TEST(WeeklyScheduleInfrastructureTests, SettingSameBufferKeepsOwnership) {
  Supla::Control::WeeklyScheduleBuffer buffer;
  auto *schedule = new TChannelConfig_WeeklySchedule{};
  buffer.set(false, schedule);

  buffer.set(false, schedule);

  ASSERT_EQ(buffer.get(false), schedule);
  schedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  EXPECT_EQ(buffer.get(false)->Program[0].Mode, SUPLA_HVAC_MODE_HEAT);
}

TEST(WeeklyScheduleInfrastructureTests, BufferReturnsProgramAndIdTogether) {
  Supla::Control::WeeklyScheduleBuffer buffer;
  TChannelConfig_WeeklySchedule schedule = {};
  schedule.Program[1].Mode = SUPLA_RELAY_MODE_FORCED_ON;
  ASSERT_TRUE(buffer.setWeeklySchedule(&schedule, 0, 2));

  TWeeklyScheduleProgram program = {};
  int programId = 0;
  EXPECT_TRUE(buffer.resolveProgramAt(&schedule, 0, &program, &programId));
  EXPECT_EQ(programId, 2);
  EXPECT_EQ(program.Mode, SUPLA_RELAY_MODE_FORCED_ON);
}

namespace {

class NativeWeeklyScheduleHandlerForTests
    : public Supla::Control::NativeWeeklyScheduleConfigHandler {
 public:
  explicit NativeWeeklyScheduleHandlerForTests(Supla::Element *owner,
                                               bool valid = false)
      : owner_(owner), valid_(valid) {
  }

  bool hasCachedSchedule() const {
    return getSchedule(false, false) != nullptr;
  }

  bool releaseInactiveSchedule(uint32_t now) {
    if (!processCache(false, now)) {
      return false;
    }
    unloadSchedule(false);
    return true;
  }

 protected:
  Supla::Element *getScheduleOwner() const override {
    return owner_;
  }
  const char *getDeviceLabel() const override {
    return "Test";
  }
  const char *getScheduleStorageTag(bool alt) const override {
    (void)(alt);
    return "weekly";
  }
  bool validateSchedule(const TChannelConfig_WeeklySchedule *schedule,
                        bool alt) const override {
    (void)(schedule);
    (void)(alt);
    return valid_;
  }
  void fillDefaultSchedule(TChannelConfig_WeeklySchedule *schedule,
                           bool alt) override {
    (void)(schedule);
    (void)(alt);
  }

 private:
  Supla::Element *owner_ = nullptr;
  bool valid_ = false;
};

class WeeklyScheduleProgramSourceForTests
    : public Supla::Control::WeeklyScheduleProgramSource {
 public:
  bool resolveProgram(
      const Supla::Control::WeeklyScheduleTimeSnapshot &time,
      bool alt,
      TWeeklyScheduleProgram *program,
      int *programId) override {
    resolveCount++;
    resolvedTime = time;
    resolvedAlt = alt;
    program->Mode = SUPLA_RELAY_MODE_FORCED_ON;
    *programId = resolvedProgramId;
    return true;
  }

  int resolvedProgramId = 1;
  int resolveCount = 0;
  bool resolvedAlt = false;
  Supla::Control::WeeklyScheduleTimeSnapshot resolvedTime;
};

class WeeklyScheduleControllerForTests
    : public Supla::Control::WeeklyScheduleController {
 public:
  bool canActivate() const override {
    return true;
  }
  bool isActive() const override {
    return active;
  }
  bool switchToWeeklySchedule() override {
    active = true;
    return true;
  }
  void switchToManualMode() override {
    active = false;
  }
  void restoreWeeklyScheduleMode(bool enabled) override {
    active = enabled;
  }
  bool processWeeklySchedule() override {
    return processCurrentProgram(startupDelay);
  }
  void useProgramSource(
      Supla::Control::WeeklyScheduleProgramSource *programSource) {
    setWeeklyScheduleProgramSource(programSource);
  }

  bool active = true;
  bool startupDelay = true;
  int appliedProgramId = -1;
  bool appliedProgramChanged = false;
  int applyCount = 0;
  bool useAltSchedule = false;
  Supla::Control::WeeklyScheduleClockState observedClockState =
      Supla::Control::WeeklyScheduleClockState::TimedOut;

 protected:
  void onWeeklyScheduleClockState(
      Supla::Control::WeeklyScheduleClockState state) override {
    observedClockState = state;
  }
  bool applyResolvedWeeklyScheduleProgram(
      const TWeeklyScheduleProgram &program,
      int programId,
      bool programChanged) override {
    applyCount++;
    appliedProgramId = programId;
    appliedProgramChanged = programChanged;
    return program.Mode == SUPLA_RELAY_MODE_FORCED_ON;
  }
  bool shouldUseAltWeeklySchedule() const override {
    return useAltSchedule;
  }
};

class WeeklyScheduleConfigHandlerForTests
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
    (void)(config);
    (void)(local);
    applyCount++;
    return Supla::ApplyConfigResult::Success;
  }
  void fillChannelConfig(void *config,
                         int *size,
                         uint8_t configType) override {
    (void)(config);
    (void)(configType);
    *size = 0;
  }
  void purgeConfig() override {
  }

  int loadCount = 0;
  int applyCount = 0;
};

class WeeklyScheduleElementForTests : public Supla::ChannelElement {
 public:
  using Supla::ChannelElement::ChannelElement;

  void loadWeeklyScheduleConfigForTests() {
    loadWeeklyScheduleConfig();
  }
};

}  // namespace

TEST(WeeklyScheduleInfrastructureTests, ControllerUsesOneClockSnapshot) {
  ClockMock clock;
  WeeklyScheduleProgramSourceForTests source;
  WeeklyScheduleControllerForTests controller;
  controller.useProgramSource(&source);

  struct tm localTime = {};
  localTime.tm_wday = Supla::DayOfWeek_Wednesday;
  localTime.tm_hour = 12;
  localTime.tm_min = 45;
  EXPECT_CALL(clock, isReady()).WillOnce(Return(true));
  EXPECT_CALL(clock, getLocalTime(_))
      .WillOnce(DoAll(SetArgPointee<0>(localTime), Return(true)));

  EXPECT_TRUE(controller.processWeeklySchedule());
  EXPECT_EQ(source.resolveCount, 1);
  EXPECT_EQ(controller.applyCount, 1);
  EXPECT_EQ(source.resolvedTime.state,
            Supla::Control::WeeklyScheduleClockState::Ready);
  EXPECT_EQ(source.resolvedTime.dayOfWeek, Supla::DayOfWeek_Wednesday);
  EXPECT_EQ(source.resolvedTime.hour, 12);
  EXPECT_EQ(source.resolvedTime.quarter, 3);
  EXPECT_EQ(controller.appliedProgramId, 1);
  EXPECT_TRUE(controller.appliedProgramChanged);
}

TEST(WeeklyScheduleInfrastructureTests,
     ControllerUsesIndependentProgramSource) {
  ClockMock clock;
  WeeklyScheduleProgramSourceForTests source;
  WeeklyScheduleControllerForTests controller;
  controller.useAltSchedule = true;
  controller.useProgramSource(&source);

  struct tm localTime = {};
  localTime.tm_wday = Supla::DayOfWeek_Friday;
  localTime.tm_hour = 18;
  localTime.tm_min = 30;
  EXPECT_CALL(clock, isReady()).WillOnce(Return(true));
  EXPECT_CALL(clock, getLocalTime(_))
      .WillOnce(DoAll(SetArgPointee<0>(localTime), Return(true)));

  EXPECT_TRUE(controller.processWeeklySchedule());
  EXPECT_EQ(source.resolveCount, 1);
  EXPECT_TRUE(source.resolvedAlt);
  EXPECT_EQ(source.resolvedTime.dayOfWeek, Supla::DayOfWeek_Friday);
  EXPECT_EQ(source.resolvedTime.hour, 18);
  EXPECT_EQ(source.resolvedTime.quarter, 2);
  EXPECT_EQ(controller.appliedProgramId, 1);
}

TEST(WeeklyScheduleInfrastructureTests,
     ControllerWaitsForClockAndKeepsProgramTransitionPending) {
  ClockMock clock;
  WeeklyScheduleProgramSourceForTests source;
  WeeklyScheduleControllerForTests controller;
  controller.useProgramSource(&source);

  EXPECT_CALL(clock, isReady()).WillOnce(Return(false));
  EXPECT_FALSE(controller.processWeeklySchedule());
  EXPECT_EQ(controller.observedClockState,
            Supla::Control::WeeklyScheduleClockState::Waiting);
  EXPECT_EQ(source.resolveCount, 0);
  EXPECT_EQ(controller.applyCount, 0);

  controller.startupDelay = false;
  EXPECT_CALL(clock, isReady()).WillOnce(Return(false));
  EXPECT_TRUE(controller.processWeeklySchedule());
  EXPECT_EQ(controller.observedClockState,
            Supla::Control::WeeklyScheduleClockState::TimedOut);
  EXPECT_EQ(source.resolveCount, 1);
  EXPECT_EQ(controller.applyCount, 1);
  EXPECT_TRUE(controller.appliedProgramChanged);

  EXPECT_CALL(clock, isReady()).WillOnce(Return(false));
  EXPECT_TRUE(controller.processWeeklySchedule());
  EXPECT_EQ(source.resolveCount, 2);
  EXPECT_EQ(controller.applyCount, 2);
  EXPECT_FALSE(controller.appliedProgramChanged);
}

TEST(WeeklyScheduleInfrastructureTests,
     ConfigHandlerIsIndependentFromRuntimeController) {
  WeeklyScheduleConfigHandlerForTests configHandler;
  WeeklyScheduleConfigHandlerForTests replacementConfigHandler;
  WeeklyScheduleElementForTests element(0);
  auto *controller = new Supla::Control::ExternalManagedWeeklySchedule();
  ASSERT_TRUE(element.setWeeklyScheduleController(controller, &configHandler));
  ASSERT_TRUE(element.setWeeklyScheduleController(
      controller, &replacementConfigHandler));

  element.loadWeeklyScheduleConfigForTests();
  EXPECT_EQ(configHandler.loadCount, 0);
  EXPECT_EQ(replacementConfigHandler.loadCount, 1);

  TSD_ChannelConfig config = {};
  config.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  EXPECT_EQ(element.handleWeeklySchedule(&config, false, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(configHandler.applyCount, 0);
  EXPECT_EQ(replacementConfigHandler.applyCount, 1);
}

TEST(WeeklyScheduleInfrastructureTests, InvalidStoredScheduleIsDiscarded) {
  ConfigMock cfg;
  Supla::ChannelElement storageOwner(0);
  NativeWeeklyScheduleHandlerForTests handler(&storageOwner);
  EXPECT_CALL(cfg, getBlobSize(StrEq("0_weekly")))
      .WillOnce(Return(sizeof(TChannelConfig_WeeklySchedule)));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce(Return(true));

  handler.onLoadConfig();
  TChannelConfig_WeeklySchedule schedule;
  memset(&schedule, 0xA5, sizeof(schedule));
  int size = 0;
  handler.fillChannelConfig(
      &schedule, &size, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);

  EXPECT_EQ(size, sizeof(schedule));
  EXPECT_FALSE(handler.hasCachedSchedule());
  const TChannelConfig_WeeklySchedule emptySchedule = {};
  EXPECT_EQ(memcmp(&schedule, &emptySchedule, sizeof(schedule)), 0);
}

TEST(WeeklyScheduleInfrastructureTests,
     LoadedStoredScheduleIsReleasedWhenInactive) {
  ConfigMock cfg;
  SimpleTime time;
  Supla::ChannelElement storageOwner(0);
  NativeWeeklyScheduleHandlerForTests handler(&storageOwner, true);
  TChannelConfig_WeeklySchedule storedSchedule = {};
  storedSchedule.Program[0].Mode = SUPLA_RELAY_MODE_FORCED_ON;

  EXPECT_CALL(cfg, getBlobSize(StrEq("0_weekly")))
      .WillOnce(Return(sizeof(TChannelConfig_WeeklySchedule)));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([&storedSchedule](const char *, char *buf, int size) {
        memcpy(buf, &storedSchedule, size);
        return true;
      });

  handler.onLoadConfig();
  TChannelConfig_WeeklySchedule schedule = {};
  int size = 0;
  handler.fillChannelConfig(
      &schedule, &size, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);

  EXPECT_EQ(size, sizeof(schedule));
  EXPECT_EQ(schedule.Program[0].Mode, SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_TRUE(handler.hasCachedSchedule());
  EXPECT_FALSE(handler.releaseInactiveSchedule(14999));
  EXPECT_TRUE(handler.releaseInactiveSchedule(15000));
  EXPECT_FALSE(handler.hasCachedSchedule());
}

class HvacBaseForTests : public Supla::Control::HvacBase {
 public:
  explicit HvacBaseForTests(OutputSimulatorWithCheck *output)
      : Supla::Control::HvacBase(output) {
  }

  bool setAndSaveFunctionForTesting(uint32_t channelFunction) {
    return setAndSaveFunction(channelFunction);
  }

  bool isLocalConfigChangePendingForTesting(int configType) const {
    return isLocalChannelConfigChangePending(configType);
  }

  void useCustomDefaultWeeklySchedule() {
    customDefaultWeeklySchedule = true;
  }

 protected:
  void fillDefaultWeeklySchedule(
      TChannelConfig_WeeklySchedule *schedule,
      bool isAltWeeklySchedule) override {
    if (!customDefaultWeeklySchedule) {
      HvacBase::fillDefaultWeeklySchedule(schedule, isAltWeeklySchedule);
      return;
    }
    schedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
    schedule->Program[0].SetpointTemperatureHeat = 2250;
    Supla::Control::WeeklyScheduleBuffer buffer;
    buffer.setWeeklySchedule(schedule, 0, 1);
  }

 private:
  bool customDefaultWeeklySchedule = false;
};

class HvacWeeklyScheduleTestsF : public ::testing::Test {
 protected:
  ConfigMock cfg;
  OutputSimulatorWithCheck output;
  SimpleTime time;
  HvacBaseForTests *hvac = {};
  Supla::Sensor::Thermometer *t1 = {};
  Supla::Sensor::ThermHygroMeter *t2 = {};

  void receiveCurrentConfigsFromDevice() {
    for (uint8_t configType : {SUPLA_CONFIG_TYPE_DEFAULT,
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE,
                               SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE}) {
      TSD_ChannelConfig serverConfig = {};
      serverConfig.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
      serverConfig.ConfigType = configType;
      int size = 0;
      hvac->fillChannelConfig(serverConfig.Config, &size, configType);
      ASSERT_GT(size, 0);
      serverConfig.ConfigSize = size;
      if (configType == SUPLA_CONFIG_TYPE_DEFAULT) {
        ASSERT_EQ(hvac->handleChannelConfig(&serverConfig, false),
                  SUPLA_CONFIG_RESULT_TRUE);
      } else {
        ASSERT_EQ(hvac->handleWeeklySchedule(
                      &serverConfig,
                      configType == SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE,
                      false),
                  SUPLA_CONFIG_RESULT_TRUE);
      }
    }
  }

  void SetUp() override {
    Supla::Channel::resetToDefaults();

    hvac = new HvacBaseForTests(&output);
    t1 = new Supla::Sensor::Thermometer();
    t2 = new Supla::Sensor::ThermHygroMeter();

    ASSERT_EQ(hvac->getChannelNumber(), 0);
    ASSERT_EQ(t1->getChannelNumber(), 1);
    ASSERT_EQ(t2->getChannelNumber(), 2);

    // init min max ranges for tempreatures setting and check again setters
    // for temperatures
    hvac->setTemperatureRoomMin(500);             // 5 degrees
    hvac->setTemperatureRoomMax(5000);            // 50 degrees
    hvac->setTemperatureHisteresisMin(20);        // 0.2 degree
    hvac->setTemperatureHisteresisMax(1000);      // 10 degree
    hvac->setTemperatureHeatCoolOffsetMin(200);   // 2 degrees
    hvac->setTemperatureHeatCoolOffsetMax(1000);  // 10 degrees
    hvac->setTemperatureAuxMin(500);              // 5 degrees
    hvac->setTemperatureAuxMax(7500);             // 75 degrees
    hvac->addAvailableAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  }

  void TearDown() override {
    delete hvac;
    delete t1;
    delete t2;
    Supla::Channel::resetToDefaults();
  }
};

TEST_F(HvacWeeklyScheduleTestsF, WeeklyScheduleBasicSetAndGet) {
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(output, setOutputValueCheck(0)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AnyNumber());
  EXPECT_CALL(cfg, setInt32(_, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, setUInt8(_, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, setBlob(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  hvac->onInit();

  // weekly schedule is not configured to off
  for (enum Supla::DayOfWeek day : {Supla::DayOfWeek_Sunday,
                                    Supla::DayOfWeek_Monday,
                                    Supla::DayOfWeek_Tuesday,
                                    Supla::DayOfWeek_Wednesday,
                                    Supla::DayOfWeek_Thursday,
                                    Supla::DayOfWeek_Friday,
                                    Supla::DayOfWeek_Saturday}) {
    for (int hour = 0; hour < 24; ++hour) {
      for (int quarter = 0; quarter < 4; ++quarter) {
        auto program =
            hvac->getProgramAt(hvac->calculateIndex(day, hour, quarter));
        EXPECT_NE(program.Mode, SUPLA_HVAC_MODE_OFF);
      }
    }
  }

  // check out of range values
  EXPECT_FALSE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, -1, 0, 0));
  EXPECT_FALSE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 24, 0, 0));
  EXPECT_FALSE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, -1, 0));
  EXPECT_FALSE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 4, 0));
  EXPECT_FALSE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, -1));
  EXPECT_FALSE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 5));
  EXPECT_FALSE(hvac->setWeeklySchedule(-1, 5));
  EXPECT_FALSE(hvac->setWeeklySchedule(1000, 5));

  // weekly schedule is still configured to default
  {
    auto program = hvac->getProgramAt(0);
    EXPECT_NE(program.Mode, SUPLA_HVAC_MODE_OFF);
  }
  //  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(Supla::DayOfWeek_Sunday, 0, 0),
  //            1);

  EXPECT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 1));
  EXPECT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 2));
  EXPECT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 3));
  EXPECT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 4));

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            4);

  EXPECT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 0));
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            0);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Tuesday, 2, 3)),
            1);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE),
            0);

  TWeeklyScheduleProgram program = {};
  auto result = hvac->getProgramById(0);
  EXPECT_EQ(memcmp(&result, &program, sizeof(program)), 0);
  result = hvac->getProgramById(1);
  EXPECT_NE(memcmp(&result, &program, sizeof(program)), 0);
  result = hvac->getProgramById(2);
  EXPECT_NE(memcmp(&result, &program, sizeof(program)), 0);
  result = hvac->getProgramById(3);
  EXPECT_NE(memcmp(&result, &program, sizeof(program)), 0);
  result = hvac->getProgramById(4);
  EXPECT_NE(memcmp(&result, &program, sizeof(program)), 0);

  EXPECT_FALSE(hvac->setProgram(-1, SUPLA_HVAC_MODE_COOL, 2000, 0));
  EXPECT_FALSE(hvac->setProgram(0, SUPLA_HVAC_MODE_COOL, 2000, 0));
  EXPECT_FALSE(hvac->setProgram(5, SUPLA_HVAC_MODE_COOL, 2000, 0));

  EXPECT_TRUE(hvac->setProgram(1, SUPLA_HVAC_MODE_HEAT, 2400, 0));
  EXPECT_FALSE(hvac->setProgram(2, SUPLA_HVAC_MODE_COOL, 0, 2300));
  EXPECT_FALSE(hvac->setProgram(3, SUPLA_HVAC_MODE_HEAT_COOL, 1800, 2400));
  EXPECT_TRUE(hvac->setProgram(4, SUPLA_HVAC_MODE_HEAT, 1900, 0));

  TWeeklyScheduleProgram program1 = {SUPLA_HVAC_MODE_HEAT, {2400}, {0}};
  TWeeklyScheduleProgram program4 = {SUPLA_HVAC_MODE_HEAT, {1900}, {0}};
  result = hvac->getProgramById(0);
  EXPECT_EQ(memcmp(&result, &program, sizeof(result)), 0);
  result = hvac->getProgramById(1);
  EXPECT_EQ(memcmp(&result, &program1, sizeof(result)), 0);
  result = hvac->getProgramById(2);
  EXPECT_NE(memcmp(&result, &program, sizeof(result)), 0);
  result = hvac->getProgramById(3);
  EXPECT_NE(memcmp(&result, &program, sizeof(result)), 0);
  result = hvac->getProgramById(4);
  EXPECT_EQ(memcmp(&result, &program4, sizeof(result)), 0);

  EXPECT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 1));
  EXPECT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Monday, 0, 0, 4));
  EXPECT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Tuesday, 0, 0, 1));

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            1);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Monday, 0, 0)),
            4);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Tuesday, 0, 0)),
            1);

  hvac->setAndSaveFunctionForTesting(
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
  EXPECT_TRUE(hvac->setProgram(1, SUPLA_HVAC_MODE_HEAT, 2400, 0));
  EXPECT_TRUE(hvac->setProgram(2, SUPLA_HVAC_MODE_COOL, 0, 2300));
  EXPECT_TRUE(hvac->setProgram(3, SUPLA_HVAC_MODE_HEAT_COOL, 1800, 2400));
  EXPECT_TRUE(hvac->setProgram(4, SUPLA_HVAC_MODE_HEAT, 1900, 0));
  EXPECT_FALSE(hvac->setProgram(3, SUPLA_HVAC_MODE_HEAT_COOL, 1800, 1900));
  EXPECT_FALSE(hvac->setProgram(3, SUPLA_HVAC_MODE_HEAT_COOL, 500, 2600));

  TWeeklyScheduleProgram program3 = {SUPLA_HVAC_MODE_HEAT_COOL, {1800}, {2400}};
  result = hvac->getProgramById(3);
  EXPECT_EQ(memcmp(&result, &program3, sizeof(result)), 0);

  EXPECT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Monday, 0, 0, 3));
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Monday, 0, 0)),
            3);
}

TEST_F(HvacWeeklyScheduleTestsF, ZeroProgramIsResolvedAsHvacOff) {
  ClockStub clock;
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(output, setOutputValueCheck(0)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AnyNumber());
  EXPECT_CALL(cfg, setInt32(_, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, setUInt8(_, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, setBlob(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  hvac->onInit();
  ASSERT_TRUE(hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 0));
  time.advance(1);

  EXPECT_EQ(hvac->getCurrentProgramId(), 0);
  EXPECT_EQ(hvac->getCurrentProgram().Mode, SUPLA_HVAC_MODE_OFF);
}

TEST_F(HvacWeeklyScheduleTestsF,
       ExternalWeeklyScheduleDoesNotExposeNativeConfigOrApi) {
  ASSERT_TRUE(hvac->setWeeklyScheduleController(
      new Supla::Control::ExternalManagedWeeklySchedule()));

  EXPECT_FALSE(hvac->getChannel()->isWeeklyScheduleAvailable());
  EXPECT_FALSE(hvac->setProgram(1, SUPLA_HVAC_MODE_HEAT, 2100, 0));
  EXPECT_FALSE(hvac->setWeeklySchedule(0, 1));

  TChannelConfig_WeeklySchedule schedule = {};
  int size = -1;
  hvac->fillChannelConfig(
      &schedule, &size, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  EXPECT_EQ(size, 0);

  EXPECT_TRUE(hvac->turnOnWeeklySchedlue());
  EXPECT_TRUE(hvac->isWeeklyScheduleEnabled());
  hvac->setWeeklyScheduleEnabled(false);
  EXPECT_FALSE(hvac->isWeeklyScheduleEnabled());
}

TEST_F(HvacWeeklyScheduleTestsF,
       CompleteServerConfigDoesNotTriggerConfigUpload) {
  ProtocolLayerMock proto;
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_weekly"), _,
                           sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_aweekly"), _,
                           sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_weekly"), _,
                           sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_aweekly"), _,
                           sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000)).Times(2);

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->onInit();
  hvac->onRegistered(nullptr);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0)).Times(1);
  EXPECT_FALSE(hvac->iterateConnected());
  receiveCurrentConfigsFromDevice();
  hvac->handleChannelConfigFinished();

  EXPECT_CALL(proto, setChannelConfig(_, _, _, _, _)).Times(0);
  for (int i = 0; i < 5; ++i) {
    hvac->iterateConnected();
  }
}

TEST_F(HvacWeeklyScheduleTestsF,
       UnsupportedServerFunctionDoesNotTriggerConfigUpload) {
  ProtocolLayerMock proto;

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->onInit();
  hvac->onRegistered(nullptr);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0)).Times(1);
  EXPECT_FALSE(hvac->iterateConnected());

  TSD_ChannelConfig serverConfig = {};
  serverConfig.Func = SUPLA_CHANNELFNC_HVAC_FAN;
  serverConfig.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  serverConfig.ConfigSize = sizeof(TChannelConfig_HVAC);
  ASSERT_EQ(hvac->handleChannelConfig(&serverConfig, false),
            SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED);
  hvac->handleChannelConfigFinished();

  EXPECT_CALL(proto, setChannelConfig(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(hvac->iterateConnected());
}

TEST_F(HvacWeeklyScheduleTestsF, ConfigChangeFlagsAreClearedPerConfigType) {
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 12))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 0))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(2);

  hvac->triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
  hvac->triggerSetChannelConfig(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE, true);
  hvac->triggerSetChannelConfig(SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE, true);

  hvac->clearChannelConfigChangedFlag();
  EXPECT_FALSE(hvac->isLocalConfigChangePendingForTesting(
      SUPLA_CONFIG_TYPE_DEFAULT));
  EXPECT_TRUE(hvac->isLocalConfigChangePendingForTesting(
      SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE));
  EXPECT_TRUE(hvac->isLocalConfigChangePendingForTesting(
      SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE));

  hvac->clearWeeklyScheduleChangedFlag();
  EXPECT_FALSE(hvac->isLocalConfigChangePendingForTesting(
      SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE));
  EXPECT_FALSE(hvac->isLocalConfigChangePendingForTesting(
      SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE));
}

TEST_F(HvacWeeklyScheduleTestsF, InitDefaultWeeklyScheduleResetsExistingData) {
  EXPECT_CALL(cfg, getBlob(_, _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setBlob(_, _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AnyNumber());

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->onInit();
  ASSERT_TRUE(hvac->setProgram(1, SUPLA_HVAC_MODE_HEAT, 2500, 0));
  ASSERT_TRUE(hvac->setProgram(1, SUPLA_HVAC_MODE_COOL, 0, 2600, true));
  ASSERT_EQ(hvac->getProgramById(1).SetpointTemperatureHeat, 2500);
  ASSERT_EQ(hvac->getProgramById(1, true).SetpointTemperatureCool, 2600);

  hvac->initDefaultWeeklySchedule();

  EXPECT_EQ(hvac->getProgramById(1).SetpointTemperatureHeat, 1900);
  EXPECT_EQ(hvac->getProgramById(1, true).SetpointTemperatureCool, 2400);
}

TEST_F(HvacWeeklyScheduleTestsF, HvacClassCanDefineItsDefaultWeeklySchedule) {
  EXPECT_CALL(cfg, setBlob(_, _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000));

  hvac->getChannel()->setDefault(
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);
  hvac->useCustomDefaultWeeklySchedule();
  hvac->initDefaultWeeklySchedule();

  EXPECT_EQ(hvac->getProgramById(1).Mode, SUPLA_HVAC_MODE_HEAT);
  EXPECT_EQ(hvac->getProgramById(1).SetpointTemperatureHeat, 2250);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr, 0), 1);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr, 1), 0);
}

TEST_F(HvacWeeklyScheduleTestsF,
       EmptyWeeklyConfigAfterSynchronizationTriggersOnlyWeeklyUpload) {
  ProtocolLayerMock proto;
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_weekly"), _,
                           sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_aweekly"), _,
                           sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_weekly"), _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_aweekly"), _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000)).Times(2);

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->onInit();
  hvac->onRegistered(nullptr);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0)).Times(1);
  EXPECT_FALSE(hvac->iterateConnected());
  receiveCurrentConfigsFromDevice();
  hvac->handleChannelConfigFinished();

  TSD_ChannelConfig emptyWeekly = {};
  emptyWeekly.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  emptyWeekly.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  emptyWeekly.ConfigSize = 0;
  ASSERT_EQ(hvac->handleWeeklySchedule(&emptyWeekly, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                               _,
                               sizeof(TChannelConfig_WeeklySchedule),
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_FALSE(hvac->iterateConnected());
}

TEST_F(HvacWeeklyScheduleTestsF,
       MissingServerConfigsAreUploadedOneByOne) {
  ProtocolLayerMock proto;
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_aweekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, setBlob(_, _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000)).Times(2);

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->onInit();
  hvac->onRegistered(nullptr);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0)).Times(1);
  EXPECT_FALSE(hvac->iterateConnected());
  hvac->handleChannelConfigFinished();

  const uint8_t configTypes[] = {
      SUPLA_CONFIG_TYPE_DEFAULT,
      SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE,
      SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE};
  const int configSizes[] = {sizeof(TChannelConfig_HVAC),
                             sizeof(TChannelConfig_WeeklySchedule),
                             sizeof(TChannelConfig_WeeklySchedule)};
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 configSizes[i],
                                 configTypes[i]))
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_FALSE(hvac->iterateConnected());

    TSDS_SetChannelConfigResult result = {
        .Result = SUPLA_CONFIG_RESULT_TRUE,
        .ConfigType = configTypes[i],
        .ChannelNumber = 0};
    hvac->handleSetChannelConfigResult(&result);
  }

  EXPECT_CALL(proto, setChannelConfig(_, _, _, _, _)).Times(0);
  EXPECT_TRUE(hvac->iterateConnected());
}

TEST_F(HvacWeeklyScheduleTestsF,
       LocalDefaultConfigChangeDoesNotBlockServerWeeklySchedule) {
  ProtocolLayerMock proto;
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_weekly"), _,
                           sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_aweekly"), _,
                           sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_weekly"), _, _))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_aweekly"), _, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000)).Times(AtLeast(1));

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->onInit();
  hvac->onRegistered(nullptr);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0)).Times(1);
  EXPECT_FALSE(hvac->iterateConnected());
  receiveCurrentConfigsFromDevice();
  hvac->handleChannelConfigFinished();

  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 1))
      .Times(1)
      .WillOnce(Return(true));
  hvac->setOutputValueOnError(100);

  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  auto *weeklySchedule = reinterpret_cast<TChannelConfig_WeeklySchedule *>(
      &configFromServer.Config);
  weeklySchedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[0].SetpointTemperatureHeat = 2100;
  weeklySchedule->Quarters[0] = 1;
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac->getProgramById(1).SetpointTemperatureHeat, 2100);

  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                               _,
                               sizeof(TChannelConfig_HVAC),
                               SUPLA_CONFIG_TYPE_DEFAULT))
      .Times(1)
      .WillOnce([](uint8_t, int, void *data, int, uint8_t) {
        auto *config = reinterpret_cast<TChannelConfig_HVAC *>(data);
        EXPECT_EQ(config->OutputValueOnError, 100);
        return true;
      });
  time.advance(5000);
  EXPECT_FALSE(hvac->iterateConnected());

  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 0))
      .Times(1)
      .WillOnce(Return(true));
  TSDS_SetChannelConfigResult result = {
      .Result = SUPLA_CONFIG_RESULT_TRUE,
      .ConfigType = SUPLA_CONFIG_TYPE_DEFAULT,
      .ChannelNumber = 0};
  hvac->handleSetChannelConfigResult(&result);
}

TEST_F(HvacWeeklyScheduleTestsF,
       ServerScheduleRemainsUsableWhenStorageWriteFails) {
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_aweekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, saveWithDelay(5000)).Times(1);

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->onInit();

  TSD_ChannelConfig serverConfig = {};
  serverConfig.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  serverConfig.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  serverConfig.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  auto *schedule = reinterpret_cast<TChannelConfig_WeeklySchedule *>(
      serverConfig.Config);
  schedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  schedule->Program[0].SetpointTemperatureHeat = 2150;
  schedule->Quarters[0] = 1;

  ASSERT_EQ(hvac->handleWeeklySchedule(&serverConfig, false, false),
            SUPLA_CONFIG_RESULT_TRUE);
  auto program = hvac->getProgramAt(0);
  EXPECT_EQ(program.Mode, SUPLA_HVAC_MODE_HEAT);
  EXPECT_EQ(program.SetpointTemperatureHeat, 2150);
}

TEST_F(HvacWeeklyScheduleTestsF,
       FailedStorageWritePreventsScheduleCacheEviction) {
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 4))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(2));

  hvac->getChannel()->setDefault(
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
  hvac->onInit();
  ASSERT_TRUE(hvac->setProgram(1, SUPLA_HVAC_MODE_HEAT, 2250, 0));

  time.advance(16000);
  hvac->iterateAlways();

  auto program = hvac->getProgramById(1);
  EXPECT_EQ(program.Mode, SUPLA_HVAC_MODE_HEAT);
  EXPECT_EQ(program.SetpointTemperatureHeat, 2250);
}

TEST_F(HvacWeeklyScheduleTestsF, MainScheduleUpdateDoesNotTouchAltSchedule) {
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_aweekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 4))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, saveWithDelay(5000)).Times(AtLeast(2));

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->onInit();

  EXPECT_TRUE(hvac->setProgram(1, SUPLA_HVAC_MODE_HEAT, 2250, 0));
}

TEST_F(HvacWeeklyScheduleTestsF,
       CoolingScheduleLoadsWithoutPrimarySchedule) {
  TChannelConfig_WeeklySchedule storedAltSchedule = {};
  storedAltSchedule.Program[0].Mode = SUPLA_HVAC_MODE_COOL;
  storedAltSchedule.Program[0].SetpointTemperatureCool = 2220;
  storedAltSchedule.Quarters[0] = 1;

  EXPECT_CALL(cfg, init());
  EXPECT_CALL(output, setOutputValueCheck(0)).Times(1);
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_weekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_aweekly"),
                      _,
                      sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce([&storedAltSchedule](const char *, char *buf, int size) {
        memcpy(buf, &storedAltSchedule, size);
        return true;
      });

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->setSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);
  hvac->onInit();

  auto program = hvac->getProgramAt(0);
  EXPECT_EQ(program.Mode, SUPLA_HVAC_MODE_COOL);
  EXPECT_EQ(program.SetpointTemperatureCool, 2220);
}

TEST_F(HvacWeeklyScheduleTestsF, handleWeeklyScehduleFromServer) {
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(output, setOutputValueCheck(0)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));
  EXPECT_CALL(cfg, setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(
      cfg,
      setBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([](const char *, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};
        EXPECT_EQ(0, memcmp(buf, &expectedData, size));
        return 1;
      })
      .WillOnce([](const char *, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};

        expectedData.Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
        expectedData.Program[0].SetpointTemperatureHeat = 2100;
        expectedData.Program[1].Mode = SUPLA_HVAC_MODE_HEAT;
        expectedData.Program[1].SetpointTemperatureHeat = 1800;
        expectedData.Program[2].Mode = SUPLA_HVAC_MODE_HEAT;
        expectedData.Program[2].SetpointTemperatureHeat = 2300;
        expectedData.Quarters[0] = (1 | (2 << 4));
        expectedData.Quarters[1] = 3;

        EXPECT_EQ(0, memcmp(buf, &expectedData, size));
        return 1;
      });
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_aweekly"), _,
              sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);

  hvac->onInit();
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  TChannelConfig_WeeklySchedule *weeklySchedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(
          &configFromServer.Config);

  // empty weekly schedule is filled with "off", so it is fine
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  // Program identifiers are encoded in four bits, while only the configured
  // program slots are valid.
  weeklySchedule->Quarters[0] = 0xF;
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  weeklySchedule->Quarters[0] = 0;

  weeklySchedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[0].SetpointTemperatureHeat = 2100;
  weeklySchedule->Program[1].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[1].SetpointTemperatureHeat = 1800;
  // cool is not supported by current hvac
  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_COOL;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 2300;

  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 2300;

  weeklySchedule->Quarters[0] = (1 | (2 << 4));
  weeklySchedule->Quarters[1] = 3;

  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  TWeeklyScheduleProgram program1 = {SUPLA_HVAC_MODE_HEAT, {2100}, {0}};
  TWeeklyScheduleProgram program2 = {SUPLA_HVAC_MODE_HEAT, {1800}, {0}};
  TWeeklyScheduleProgram program3 = {SUPLA_HVAC_MODE_HEAT, {2300}, {0}};
  auto result = hvac->getProgramById(1);
  EXPECT_EQ(memcmp(&result, &program1, sizeof(result)), 0);
  result = hvac->getProgramById(2);
  EXPECT_EQ(memcmp(&result, &program2, sizeof(result)), 0);
  result = hvac->getProgramById(3);
  EXPECT_EQ(memcmp(&result, &program3, sizeof(result)), 0);

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            1);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 1)),
            2);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 2)),
            3);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 3)),
            0);
}

TEST_F(HvacWeeklyScheduleTestsF, startupProcedureWithEmptyConfigForWeekly) {
  EXPECT_CALL(cfg, init());
  // Config storage doesn't contain any data about HVAC channel, so it returns
  // false on each getxxx call. Then function is initialized and saved to
  // storage.
  EXPECT_CALL(output, setOutputValueCheck(0)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));
  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_aweekly"), _,
              sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);
  EXPECT_CALL(cfg, setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), 1))
      .Times(0);
  EXPECT_CALL(
      cfg,
      setBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([](const char *, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};
        EXPECT_NE(0, memcmp(buf, &expectedData, size));
        return 1;
      })
      .WillOnce([](const char *, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};

        EXPECT_EQ(0, memcmp(buf, &expectedData, size));
        return 1;
      });
  EXPECT_CALL(
      cfg,
      setBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillRepeatedly(Return(1));

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  hvac->onRegistered(nullptr);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // default schedule (program 1)
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            1);

  // send config from server
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);

  // empty weekly schedule is filled with "off", so it is fine
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  // check if schedule was properly configured (off)
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            0);
}

TEST_F(HvacWeeklyScheduleTestsF,
       weeklyScheduleCacheReleasesAfterDelayWhenInactive) {
  TChannelConfig_WeeklySchedule storedWeeklySchedule = {};
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(output, setOutputValueCheck(0)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));
  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_aweekly"), _,
              sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);
  EXPECT_CALL(cfg, setUInt8(_, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(
      cfg,
      setBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce([&storedWeeklySchedule](const char *,
                                       const char *buf,
                                       int size) {
        memcpy(&storedWeeklySchedule, buf, size);
        return true;
      });
  EXPECT_CALL(
      cfg,
      setBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  hvac->onRegistered(nullptr);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            1);

  hvac->setWeeklyScheduleEnabled(true);
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
    time.advance(100);
  }

  hvac->setWeeklyScheduleEnabled(false);
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce([&storedWeeklySchedule](const char *, char *buf, int size) {
        memcpy(buf, &storedWeeklySchedule, size);
        return true;
      });

  time.advance(16000);
  hvac->iterateAlways();

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            1);
}

TEST_F(HvacWeeklyScheduleTestsF,
       startupProcedureWithScheduleChangedBeforeConnection) {
  EXPECT_CALL(cfg, init());
  ProtocolLayerMock proto;
  ::testing::Sequence s1;
  // Config storage doesn't contain any data about HVAC channel, so it returns
  // false on each getxxx call. Then function is initialized and saved to
  // storage.
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, _, _)).Times(1);
  EXPECT_CALL(output, setOutputValueCheck(0)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));
  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_aweekly"), _,
              sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(
      cfg,
      setBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(2)
      .InSequence(s1)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(
      cfg,
      setBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(2)
      .WillRepeatedly(Return(1));

  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 4))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 12))
      .Times(1)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(
      cfg,
      setBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .InSequence(s1)
      .WillOnce([](const char *, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};

        EXPECT_EQ(0, memcmp(buf, &expectedData, size));
        return 1;
      });

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();

  hvac->setProgram(1, SUPLA_HVAC_MODE_HEAT, 1800, 0);
  hvac->setProgram(1, SUPLA_HVAC_MODE_COOL, 0, 2400, true);

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // send config from server
  TSD_ChannelConfig defaultConfigFromServer = {};
  defaultConfigFromServer.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  defaultConfigFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  int defaultConfigSize = 0;
  hvac->fillChannelConfig(defaultConfigFromServer.Config,
                          &defaultConfigSize,
                          SUPLA_CONFIG_TYPE_DEFAULT);
  defaultConfigFromServer.ConfigSize = defaultConfigSize;
  EXPECT_EQ(hvac->handleChannelConfig(&defaultConfigFromServer, false),
            SUPLA_CONFIG_RESULT_TRUE);

  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);

  // empty weekly schedule is filled with "off", so it is fine
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  // above set config from server should be ignored
  TWeeklyScheduleProgram program1 = {SUPLA_HVAC_MODE_HEAT, {1800}, {0}};
  auto progResult = hvac->getProgramById(1);
  EXPECT_EQ(memcmp(&progResult, &program1, sizeof(progResult)), 0);

  hvac->handleChannelConfigFinished();

  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                               _,
                               sizeof(TChannelConfig_WeeklySchedule),
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
      .Times(1)
      .WillRepeatedly(Return(true));

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // send reply from server
  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 8))
      .Times(1)
      .WillOnce(Return(true));
  TSDS_SetChannelConfigResult result = {.Result = SUPLA_CONFIG_RESULT_FALSE,
                                        .ConfigType =
                                            SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE,
                                        .ChannelNumber = 0};

  hvac->handleSetChannelConfigResult(&result);

  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                               _,
                               sizeof(TChannelConfig_WeeklySchedule),
                               SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE))
      .Times(1)
      .WillOnce(Return(true));

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  EXPECT_CALL(cfg, setUInt32(StrEq("0_cfg_chng_t"), 0))
      .Times(1)
      .WillOnce(Return(true));
  result.Result = SUPLA_CONFIG_RESULT_TRUE;
  result.ConfigType = SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE;
  hvac->handleSetChannelConfigResult(&result);

  // send anothoer set channel config from server - this time it should be
  // applied to the channel
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_TRUE);
}

TEST_F(HvacWeeklyScheduleTestsF, handleWeeklyScehduleFromServerForDiffMode) {
  EXPECT_CALL(cfg, init());
  EXPECT_CALL(output, setOutputValueCheck(0)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(0);

  EXPECT_CALL(
      cfg,
      setBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([](const char *, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};
        EXPECT_EQ(0, memcmp(buf, &expectedData, size));
        return 1;
      })
      .WillOnce([](const char *, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};

        expectedData.Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
        expectedData.Program[0].SetpointTemperatureHeat = 2100;
        expectedData.Program[1].Mode = SUPLA_HVAC_MODE_HEAT;
        expectedData.Program[1].SetpointTemperatureHeat = -2000;
        expectedData.Program[2].Mode = SUPLA_HVAC_MODE_HEAT;
        expectedData.Program[2].SetpointTemperatureHeat = 4000;
        expectedData.Quarters[0] = (1 | (2 << 4));
        expectedData.Quarters[1] = 3;

        EXPECT_EQ(0, memcmp(buf, &expectedData, size));
        return 1;
      });

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);
  hvac->onLoadConfig(nullptr);
  hvac->onInit();
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  TChannelConfig_WeeklySchedule *weeklySchedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(
          &configFromServer.Config);

  // empty weekly schedule is filled with "off", so it is fine
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  weeklySchedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[0].SetpointTemperatureHeat = 2100;
  weeklySchedule->Program[1].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[1].SetpointTemperatureHeat = -2000;
  // cool is not supported by current hvac
  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_COOL;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 2300;

  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 4000;

  weeklySchedule->Quarters[0] = (1 | (2 << 4));
  weeklySchedule->Quarters[1] = 3;

  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer, false, false),
            SUPLA_CONFIG_RESULT_TRUE);

  TWeeklyScheduleProgram program1 = {SUPLA_HVAC_MODE_HEAT, {2100}, {0}};
  TWeeklyScheduleProgram program2 = {SUPLA_HVAC_MODE_HEAT, {-2000}, {0}};
  TWeeklyScheduleProgram program3 = {SUPLA_HVAC_MODE_HEAT, {4000}, {0}};
  auto result = hvac->getProgramById(1);
  EXPECT_EQ(memcmp(&result, &program1, sizeof(result)), 0);
  result = hvac->getProgramById(2);
  EXPECT_EQ(memcmp(&result, &program2, sizeof(result)), 0);
  result = hvac->getProgramById(3);
  EXPECT_EQ(memcmp(&result, &program3, sizeof(result)), 0);

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            1);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 1)),
            2);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 2)),
            3);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(
                nullptr, hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 3)),
            0);
}
