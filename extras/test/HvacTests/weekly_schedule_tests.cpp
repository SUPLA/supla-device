/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <gtest/gtest.h>

#include <string.h>
#include <supla/control/hvac_base.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <supla/sensor/thermometer.h>
#include <config_mock.h>
#include <simple_time.h>
#include <protocol_layer_mock.h>
#include "gmock/gmock.h"
#include <output_mock.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::StrEq;
using ::testing::Return;
using ::testing::AnyNumber;

class HvacWeeklyScheduleTestsF : public ::testing::Test {
 protected:
  ConfigMock cfg;
  OutputMock output;
  SimpleTime time;
  Supla::Control::HvacBase *hvac = {};
  Supla::Sensor::Thermometer *t1 = {};
  Supla::Sensor::ThermHygroMeter *t2 = {};

  void SetUp() override {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));

    hvac = new Supla::Control::HvacBase(&output);
    t1 = new Supla::Sensor::Thermometer();
    t2 = new Supla::Sensor::ThermHygroMeter();

    ASSERT_EQ(hvac->getChannelNumber(), 0);
    ASSERT_EQ(t1->getChannelNumber(), 1);
    ASSERT_EQ(t2->getChannelNumber(), 2);

    // init min max ranges for tempreatures setting and check again setters
    // for temperatures
    hvac->setTemperatureRoomMin(500);           // 5 degrees
    hvac->setTemperatureRoomMax(5000);          // 50 degrees
    hvac->setTemperatureHisteresisMin(20);      // 0.2 degree
    hvac->setTemperatureHisteresisMax(1000);    // 10 degree
    hvac->setTemperatureHeatCoolOffsetMin(200);     // 2 degrees
    hvac->setTemperatureHeatCoolOffsetMax(1000);    // 10 degrees
    hvac->setTemperatureAuxMin(500);   // 5 degrees
    hvac->setTemperatureAuxMax(7500);  // 75 degrees
    hvac->addAvailableAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  }

  void TearDown() override {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
    delete hvac;
    delete t1;
    delete t2;
  }
};

TEST_F(HvacWeeklyScheduleTestsF, WeeklyScheduleBasicSetAndGet) {
  EXPECT_CALL(output, setOutputValue(0)).Times(1);
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
  EXPECT_FALSE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, -1, 0, 0));
  EXPECT_FALSE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 24, 0, 0));
  EXPECT_FALSE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, -1, 0));
  EXPECT_FALSE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 4, 0));
  EXPECT_FALSE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, -1));
  EXPECT_FALSE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 5));
  EXPECT_FALSE(
      hvac->setWeeklySchedule(-1, 5));
  EXPECT_FALSE(
      hvac->setWeeklySchedule(1000, 5));

  // weekly schedule is still configured to default
  {
    auto program = hvac->getProgramAt(0);
    EXPECT_NE(program.Mode, SUPLA_HVAC_MODE_OFF);
  }
//  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(Supla::DayOfWeek_Sunday, 0, 0),
//            1);

  EXPECT_TRUE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 1));
  EXPECT_TRUE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 2));
  EXPECT_TRUE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 3));
  EXPECT_TRUE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 4));

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
                hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)), 4);

  EXPECT_TRUE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 0));
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)), 0);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Tuesday, 2, 3)),
            1);

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

  EXPECT_FALSE(
      hvac->setProgram(-1, SUPLA_HVAC_MODE_COOL, 2000, 0));
  EXPECT_FALSE(
      hvac->setProgram(0, SUPLA_HVAC_MODE_COOL, 2000, 0));
  EXPECT_FALSE(
      hvac->setProgram(5, SUPLA_HVAC_MODE_COOL, 2000, 0));

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

  EXPECT_TRUE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Sunday, 0, 0, 1));
  EXPECT_TRUE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Monday, 0, 0, 4));
  EXPECT_TRUE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Tuesday, 0, 0, 1));

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)), 1);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Monday, 0, 0)), 4);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Tuesday, 0, 0)),
            1);

  hvac->setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
  EXPECT_TRUE(hvac->setProgram(1, SUPLA_HVAC_MODE_HEAT, 2400, 0));
  EXPECT_TRUE(hvac->setProgram(2, SUPLA_HVAC_MODE_COOL, 0, 2300));
  EXPECT_TRUE(hvac->setProgram(3, SUPLA_HVAC_MODE_HEAT_COOL, 1800, 2400));
  EXPECT_TRUE(hvac->setProgram(4, SUPLA_HVAC_MODE_HEAT, 1900, 0));
  EXPECT_FALSE(hvac->setProgram(3, SUPLA_HVAC_MODE_HEAT_COOL, 1800, 1900));
  EXPECT_FALSE(hvac->setProgram(3, SUPLA_HVAC_MODE_HEAT_COOL, 500, 2600));

  TWeeklyScheduleProgram program3 = {SUPLA_HVAC_MODE_HEAT_COOL, {1800}, {2400}};
  result = hvac->getProgramById(3);
  EXPECT_EQ(memcmp(&result, &program3, sizeof(result)), 0);

  EXPECT_TRUE(
      hvac->setWeeklySchedule(Supla::DayOfWeek_Monday, 0, 0, 3));
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Monday, 0, 0)), 3);
}

TEST_F(HvacWeeklyScheduleTestsF, handleWeeklyScehduleFromServer) {
  ::testing::Sequence s1;
  EXPECT_CALL(output, setOutputValue(0)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT))
      .Times(1).WillOnce(Return(true));

  EXPECT_CALL(
      cfg,
      setBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([](const char *key, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};
        EXPECT_NE(0, memcmp(buf, &expectedData, size));
        return 1;
      })
      .WillOnce([](const char *key, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};

        EXPECT_EQ(0, memcmp(buf, &expectedData, size));
        return 1;
      })
      .WillOnce([](const char *key, const char *buf, int size) {
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
      setBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillRepeatedly([](const char *key, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};
        EXPECT_NE(0, memcmp(buf, &expectedData, size));
        return 1;
      });

  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_chng"), 0))
      .Times(1).InSequence(s1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_chng"), 0))
      .Times(1).InSequence(s1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_chng"), 0))
      .Times(1).InSequence(s1)
      .WillOnce(Return(true));

  hvac->onInit();
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  TChannelConfig_WeeklySchedule *weeklySchedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(
          &configFromServer.Config);

  // empty weekly schedule is filled with "off", so it is fine
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);

  weeklySchedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[0].SetpointTemperatureHeat = 2100;
  weeklySchedule->Program[1].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[1].SetpointTemperatureHeat = 1800;
  // cool is not supported by current hvac
  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_COOL;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 2300;

  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
      SUPLA_CONFIG_RESULT_DATA_ERROR);

  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 2300;

  weeklySchedule->Quarters[0] = (1 | (2 << 4));
  weeklySchedule->Quarters[1] = 3;

  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
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

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)), 1);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 1)), 2);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 2)), 3);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 3)), 0);
}

TEST_F(HvacWeeklyScheduleTestsF, startupProcedureWithEmptyConfigForWeekly) {
  // Config storage doesn't contain any data about HVAC channel, so it returns
  // false on each getxxx call. Then function is initialized and saved to
  // storage.
  EXPECT_CALL(output, setOutputValue(0)).Times(1);
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
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_weekly_chng"), 0))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(
      cfg,
      setBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce(
          [](const char *key, const char *buf, int size) {
            TChannelConfig_WeeklySchedule expectedData = {};
            EXPECT_NE(0, memcmp(buf, &expectedData, size));
            return 1;
          })
      .WillOnce(
          [](const char *key, const char *buf, int size) {
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
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            1);

  // send config from server
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);

  // empty weekly schedule is filled with "off", so it is fine
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);

  // check if schedule was properly configured (off)
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)),
            0);
}

TEST_F(HvacWeeklyScheduleTestsF,
       startupProcedureWithScheduleChangedBeforeConnection) {
  ProtocolLayerMock proto;
  ::testing::Sequence s1, s2;
  // Config storage doesn't contain any data about HVAC channel, so it returns
  // false on each getxxx call. Then function is initialized and saved to
  // storage.
  EXPECT_CALL(output, setOutputValue(0)).Times(1);
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
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(
      cfg,
      setBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(2)
      .InSequence(s1)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(
      cfg,
      setBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillRepeatedly(Return(1));

  EXPECT_CALL(cfg, setUInt8(StrEq("0_weekly_chng"), 0))
      .Times(1)
      .InSequence(s2)
      .WillOnce(Return(true));

  EXPECT_CALL(cfg, setUInt8(StrEq("0_weekly_chng"), 1))
      .Times(1)
      .InSequence(s2)
      .WillOnce(Return(true));

  EXPECT_CALL(cfg, setUInt8(StrEq("0_weekly_chng"), 0))
      .Times(1)
      .InSequence(s2)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(cfg, setUInt8(StrEq("0_weekly_chng"), 0))
      .Times(1)
      .InSequence(s2)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(
      cfg,
      setBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .InSequence(s1)
      .WillOnce(
          [](const char *key, const char *buf, int size) {
            TChannelConfig_WeeklySchedule expectedData = {};

            EXPECT_EQ(0, memcmp(buf, &expectedData, size));
            return 1;
          });

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();

  hvac->setProgram(1, SUPLA_HVAC_MODE_HEAT, 1800, 0);

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // send config from server
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);

  // empty weekly schedule is filled with "off", so it is fine
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);

  // above set config from server should be ignored
  TWeeklyScheduleProgram program1 = {SUPLA_HVAC_MODE_HEAT, {1800}, {0}};
  auto progResult = hvac->getProgramById(1);
  EXPECT_EQ(memcmp(&progResult, &program1, sizeof(progResult)), 0);

  hvac->handleChannelConfigFinished();

  {
    ::testing::InSequence seq;

    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_HVAC),
                                 SUPLA_CONFIG_TYPE_DEFAULT))
        .WillOnce(Return(false));
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
        .WillOnce(Return(false));

    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_HVAC),
                                 SUPLA_CONFIG_TYPE_DEFAULT))
        .WillOnce(Return(true));
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
        .WillOnce([](uint8_t channelNumber,
                     _supla_int_t channelFunction,
                     void *buf,
                     int size,
                     uint8_t configType) {
          TChannelConfig_WeeklySchedule expectedData = {};
          expectedData.Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
          expectedData.Program[0].SetpointTemperatureHeat = 1800;
          EXPECT_NE(0, memcmp(buf, &expectedData, size));
          return true;
        });
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE))
        .WillOnce(Return(true));
  }

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // send reply from server
  TSDS_SetChannelConfigResult result = {
      .Result = SUPLA_CONFIG_RESULT_FALSE,
      .ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE,
      .ChannelNumber = 0
  };

  hvac->handleSetChannelConfigResult(&result);

  // send anothoer set channel config from server - this time it should be
  // applied to the channel
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);
}

TEST_F(HvacWeeklyScheduleTestsF, handleWeeklyScehduleFromServerForDiffMode) {
  EXPECT_CALL(output, setOutputValue(0)).Times(1);
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));

  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_chng"), 0))
      .Times(AtLeast(1)).WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(
      cfg,
      setBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillOnce([](const char *key, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};

        EXPECT_NE(0, memcmp(buf, &expectedData, size));
        return 1;
      })
      .WillOnce([](const char *key, const char *buf, int size) {
        TChannelConfig_WeeklySchedule expectedData = {};

        EXPECT_EQ(0, memcmp(buf, &expectedData, size));
        return 1;
      })
      .WillOnce([](const char *key, const char *buf, int size) {
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
  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);

  weeklySchedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[0].SetpointTemperatureHeat = 2100;
  weeklySchedule->Program[1].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[1].SetpointTemperatureHeat = -2000;
  // cool is not supported by current hvac
  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_COOL;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 2300;

  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
      SUPLA_CONFIG_RESULT_DATA_ERROR);

  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 4000;

  weeklySchedule->Quarters[0] = (1 | (2 << 4));
  weeklySchedule->Quarters[1] = 3;

  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
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

  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 0)), 1);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 1)), 2);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 2)), 3);
  EXPECT_EQ(hvac->getWeeklyScheduleProgramId(nullptr,
        hvac->calculateIndex(Supla::DayOfWeek_Sunday, 0, 3)), 0);
}

