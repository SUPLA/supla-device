/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <protocol_layer_mock.h>
#include <simple_time.h>
#include <string.h>
#include <supla/control/hvac_base.h>
#include <supla/sensor/virtual_thermometer.h>
#include <output_mock.h>
#include <storage_mock.h>
#include <clock_stub.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::StrEq;
using ::testing::Return;
// using ::testing::Args;
// using ::testing::ElementsAre;

class HvacIntegrationScheduleF : public ::testing::Test {
 protected:
  ConfigMock cfg;
  StorageMock storage;
  OutputMock primaryOutput;
  OutputMock secondaryOutput;
  SimpleTime time;
  ProtocolLayerMock proto;

  Supla::Control::HvacBase *hvac = {};
  Supla::Sensor::VirtualThermometer *t1 = {};
  Supla::Sensor::VirtualThermometer *t2 = {};

  void SetUp() override {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));

    hvac = new Supla::Control::HvacBase(&primaryOutput);
    t1 = new Supla::Sensor::VirtualThermometer();
    t2 = new Supla::Sensor::VirtualThermometer();

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

TEST_F(HvacIntegrationScheduleF, startupWithEmptyConfigHeating) {
  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

  EXPECT_CALL(primaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));

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

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_ignr"), _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(storage, readStorage(_, _, sizeof(THVACValue), _))
      .WillRepeatedly(
          [](uint32_t address, unsigned char *data, int size, bool) {
            THVACValue hvacValue = {};
            memcpy(data, &hvacValue, sizeof(THVACValue));
            return sizeof(THVACValue);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(int16_t), _))
      .WillRepeatedly(
          [](uint32_t address, unsigned char *data, int size, bool) {
            int16_t value = INT16_MIN;
            memcpy(data, &value, sizeof(int16_t));
            return sizeof(int16_t);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillRepeatedly(
          [](uint32_t address, unsigned char *data, int size, bool) {
            *data = 0;
            return sizeof(uint8_t);
          });

  // ignore channel value changed from thermometer
  EXPECT_CALL(proto, sendChannelValueChanged(1, _, 0, 0)).Times(AtLeast(1));

  t1->setValue(21.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();

  ::testing::Sequence seq1;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);

  hvac->onInit();
  t1->onInit();


  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  TSD_SuplaChannelNewValue newValue = {};
  THVACValue *hvacValue = reinterpret_cast<THVACValue *>(newValue.value);
  hvacValue->Flags = SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
    SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
  hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
  hvacValue->SetpointTemperatureHeat = 2050;
  hvacValue->SetpointTemperatureCool = 2700;


  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2050);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2050);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });
  hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);


  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  TChannelConfig_WeeklySchedule *weeklySchedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(
          &configFromServer.Config);

  weeklySchedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[0].SetpointTemperatureHeat = 1500;
  weeklySchedule->Program[1].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[1].SetpointTemperatureHeat = 1900;
  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 2300;

  weeklySchedule->Quarters[0] = (1 | (2 << 4));
  weeklySchedule->Quarters[1] = 3;

  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvacValue->Mode = SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE;
  hvacValue->SetpointTemperatureHeat = 2350;
  hvacValue->SetpointTemperatureCool = 2700;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE |
                  SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        // in case of clock error, we work with program[0]
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  ClockStub clock;

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  for (int i = 0; i < 15; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(60*1000);  // +1 minute
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }


  for (int i = 0; i < 15; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(60*1000);  // +1 minute
  }

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  for (int i = 0; i < 15; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(60*1000);  // +1 minute
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
  hvacValue->SetpointTemperatureHeat = 2350;
  hvacValue->SetpointTemperatureCool = 2700;
  newValue.DurationSec = 20;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING |
                  SUPLA_HVAC_VALUE_FLAG_COUNTDOWN_TIMER);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2350);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  // +20s
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2350);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  // +20s
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }
}

TEST_F(HvacIntegrationScheduleF, mixedCommandsCheck) {
  hvac->addSecondaryOutput(&secondaryOutput);
  ClockStub clock;

  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

  EXPECT_CALL(primaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));

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
  EXPECT_CALL(
      cfg, setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_ignr"), _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(storage, readStorage(_, _, sizeof(THVACValue), _))
      .WillRepeatedly(
          [](uint32_t address, unsigned char *data, int size, bool) {
            THVACValue hvacValue = {};
            memcpy(data, &hvacValue, sizeof(THVACValue));
            return sizeof(THVACValue);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(int16_t), _))
      .WillRepeatedly(
          [](uint32_t address, unsigned char *data, int size, bool) {
            int16_t value = INT16_MIN;
            memcpy(data, &value, sizeof(int16_t));
            return sizeof(int16_t);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillRepeatedly(
          [](uint32_t address, unsigned char *data, int size, bool) {
            *data = 0;
            return sizeof(uint8_t);
          });

  // ignore channel value changed from thermometer
  EXPECT_CALL(proto, sendChannelValueChanged(1, _, 0, 0)).Times(AtLeast(1));

  t1->setValue(21.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();

  ::testing::Sequence seq1;
  ::testing::Sequence seq2;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(secondaryOutput, setOutputValue(0)).Times(1).InSequence(seq2);

  hvac->onInit();
  t1->onInit();


  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1, seq2)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });


  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  TSD_SuplaChannelNewValue newValue = {};
  THVACValue *hvacValue = reinterpret_cast<THVACValue *>(newValue.value);
  hvacValue->Flags = SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
    SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
  hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
  hvacValue->SetpointTemperatureHeat = 2050;
  hvacValue->SetpointTemperatureCool = 2700;


  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1, seq2)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2050);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_WeeklySchedule);
  TChannelConfig_WeeklySchedule *weeklySchedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(
          &configFromServer.Config);

  weeklySchedule->Program[0].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[0].SetpointTemperatureHeat = 1500;
  weeklySchedule->Program[1].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[1].SetpointTemperatureHeat = 1900;
  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 2300;

  weeklySchedule->Quarters[0] = (1 | (2 << 4));
  weeklySchedule->Quarters[1] = 3;

  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvacValue->Flags = SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
    SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
  hvacValue->Mode = SUPLA_HVAC_MODE_CMD_TURN_ON;
  hvacValue->SetpointTemperatureHeat = 2050;
  hvacValue->SetpointTemperatureCool = 2700;


  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2050);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });


  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvacValue->Flags = 0;
  hvacValue->Mode = SUPLA_HVAC_MODE_COOL;

  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2050);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });


  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2050);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });


  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvacValue->Mode = SUPLA_HVAC_MODE_CMD_TURN_ON;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2050);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });


  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2050);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });


  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvacValue->Mode = SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });


  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  weeklySchedule->Program[0].Mode = SUPLA_HVAC_MODE_COOL;
  weeklySchedule->Program[0].SetpointTemperatureCool = 2600;
  weeklySchedule->Program[1].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[1].SetpointTemperatureHeat = 1900;
  weeklySchedule->Program[2].Mode = SUPLA_HVAC_MODE_HEAT;
  weeklySchedule->Program[2].SetpointTemperatureHeat = 2300;

  weeklySchedule->Quarters[0] = (1 | (2 << 4));
  weeklySchedule->Quarters[1] = 3;

  EXPECT_EQ(hvac->handleWeeklySchedule(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  // this turn on do nothing, because thermostat is already turned on

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  // this turn on do nothing, because thermostat is already turned on

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 70; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  auto quarter = Supla::Clock::GetQuarter();
  (void)(quarter);
  t1->setValue(24.5);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 70; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(60 * 1000);  // +1 minute
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  // this turn on do nothing, because thermostat is already turned on

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2600);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvacValue->Flags = 0;
  hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
  hvacValue->SetpointTemperatureHeat = INT16_MIN;
  hvacValue->SetpointTemperatureCool = INT16_MIN;

  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2050);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}
