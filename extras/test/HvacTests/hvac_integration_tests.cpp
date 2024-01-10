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
#include <supla/actions.h>

using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::AtLeast;
using ::testing::StrEq;
using ::testing::Return;
// using ::testing::Args;
// using ::testing::ElementsAre;

class HvacIntegrationF : public ::testing::Test {
 protected:
  ConfigMock cfg;
  StorageMock storage;
  OutputMock primaryOutput;
  OutputMock secondaryOutput;
  SimpleTime time;
  ProtocolLayerMock proto;
  ClockStub clock;

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
    hvac->setDefaultTemperatureRoomMin(
        SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL,
        500);  // 5 degrees
    hvac->setDefaultTemperatureRoomMin(SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                       500);  // 5 degrees
    hvac->setDefaultTemperatureRoomMax(
        SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL,
        5000);  // 50 degrees
    hvac->setDefaultTemperatureRoomMax(SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                       5000);  // 50 degrees
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

TEST_F(HvacIntegrationF, startupWithEmptyConfigHeating) {
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
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
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

  ::testing::Sequence seq1;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags, 0);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
      .InSequence(seq1)
      .WillOnce([](uint8_t channelNumber,
                   char *value,
                   unsigned char offline,
                   uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
      });

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  // set value 0

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(10);  // set value 1
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // very cold
  t1->setValue(-20);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // extreemly cold
  t1->setValue(-200);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // extreemly hot
  t1->setValue(200);  // set value 0
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(30);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(15);  // set value 1
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // check values around target 18.0 (histeresis 0.4 C, so +- 0.2 C)
  t1->setValue(20.9);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(21.1);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(21.2);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // should stop heating
  t1->setValue(21.21);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(21.1);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(20.8);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // start heating
  t1->setValue(20.79);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTemperatureSetpointHeat(2300);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // hvac->setTemperatureSetpointHeat(-500);  // -5
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  hvac->clearTemperatureSetpointHeat();
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, startupWithEmptyConfigCooling) {
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
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
//  EXPECT_CALL(cfg,
//              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL))
//      .Times(1)
//      .WillOnce(Return(true));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
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

  ::testing::Sequence seq1;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL|
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL|
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL|
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL|
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2100);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags, SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2100);
    });
  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL|
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2100);
    });

  // it was cool
  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac->setDefaultSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(10);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // very cold
  t1->setValue(-20);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // extreemly cold
  t1->setValue(-200);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // extreemly hot
  t1->setValue(200);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(30);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_COOL);
  for (int i = 0; i < 70; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // check values around target 24.0 (histeresis 0.4 C, so +- 0.2 C)
  t1->setValue(24.9);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(24.8);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // should stop cooling
  t1->setValue(24.7);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(25);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(25.1);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(25.2);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // start cooling
  t1->setValue(25.21);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTemperatureSetpointCool(2100);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  hvac->clearTemperatureSetpointCool();
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, startupWithEmptyConfigHeatCool) {
  hvac->addSecondaryOutput(&secondaryOutput);

  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

  EXPECT_CALL(primaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));
  EXPECT_CALL(secondaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));

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
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg, setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_aweekly"), _, _))
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

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();

  ::testing::Sequence seqPrimary;
  ::testing::Sequence seqSecondary;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);

  hvac->onInit();
  t1->onInit();
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });


  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(10);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // very cold
  t1->setValue(-20);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // extreemly cold
  t1->setValue(-200);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // extreemly hot
  t1->setValue(200);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(30);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(15);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // check values around target 18.0 (histeresis 0.4 C, so +- 0.2 C)
  t1->setValue(20.9);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(21.1);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(21.2);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // should stop heating
  t1->setValue(21.21);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(21.1);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(20.8);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // start heating
  t1->setValue(20.79);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvac->setTargetMode(SUPLA_HVAC_MODE_COOL);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(1))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(25.3);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(24.8);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(24.7);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });


  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
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

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(1))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(25.3);
  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT_COOL);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
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

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(15);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(21.2);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
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

  t1->setValue(21.3);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }


  t1->setValue(22);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(23);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(24);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(25);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(22);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(1))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(25.3);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
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

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(1))
      .Times(1)
      .InSequence(seqSecondary);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });


  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, startupWithEmptyConfigDifferentialHeat) {
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
//  EXPECT_CALL(cfg,
//              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL))
//      .Times(1)
//      .WillOnce(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
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
  EXPECT_CALL(proto, sendChannelValueChanged(2, _, 0, 0)).Times(AtLeast(1));

  ::testing::Sequence seq1;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(23.5);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setAuxThermometerChannelNo(2);
  hvac->setAuxThermometerType(
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
  hvac->setTemperatureHisteresis(400);  // 4.00 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  hvac->setTemperatureSetpointHeat(-500);  // -5
  t1->onInit();

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  hvac->handleChannelConfigFinished();

  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL,
                               _,
                               sizeof(TChannelConfig_HVAC),
                               SUPLA_CONFIG_TYPE_DEFAULT))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL,
                               _,
                               sizeof(TChannelConfig_WeeklySchedule),
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
      .Times(1)
      .WillRepeatedly(Return(true));

  t1->setValue(10);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  t2->setValue(-20);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  t1->setValue(-200);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  // extreemly hot
  t1->setValue(200);
  t2->setValue(200);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  t1->setValue(30);
  t2->setValue(30);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  for (int i = 0; i < 70; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t1->setValue(20);
  t2->setValue(30);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  t1->setValue(22);
  t2->setValue(28);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  t1->setValue(23);
  t2->setValue(27);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  // diff 24-26 = -2
  // heat below -5 +- 2
  t1->setValue(24);
  t2->setValue(26);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  // -4
  t1->setValue(23);
  t2->setValue(27);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  // -6
  t1->setValue(22);
  t2->setValue(28);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  // -7.5
  t1->setValue(20.5);
  t2->setValue(28);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t1->setValue(50);
  t2->setValue(28);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  t1->setValue(50);
  t2->setValue(50);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }


  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t1->setValue(50);
  t2->setValue(60);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -1500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  hvac->setTemperatureSetpointHeat(-1500);  // -15

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -100);  // -1
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  hvac->setTemperatureSetpointHeat(-100);  // -1

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_THERMOMETER_ERROR);

        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  t1->setValue(-275);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));
  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL,
                               _,
                               sizeof(TChannelConfig_HVAC),
                               SUPLA_CONFIG_TYPE_DEFAULT))
      .Times(1)
      .WillRepeatedly(Return(true));


  hvac->setOutputValueOnError(100);  // change configuration on device
  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING |
                  SUPLA_HVAC_VALUE_FLAG_THERMOMETER_ERROR);

        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, -100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  t1->setValue(-275);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, startupWithEmptyConfigHeatCoolSetpointTempCheck) {
  hvac->addSecondaryOutput(&secondaryOutput);

  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

  EXPECT_CALL(primaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));
  EXPECT_CALL(secondaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));

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

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();

  ::testing::Sequence seqPrimary;
  ::testing::Sequence seqSecondary;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);

  hvac->onInit();
  t1->onInit();
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
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

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT_COOL);
  t1->setValue(10);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // this is invalid temperature setpoint setting, however it is not validated
  // on this method call. All other interfaces (value from server or weekly
  // schedule) are validated. This method should be validated on local device
  // interface instead.
  hvac->setTemperatureSetpointCool(1500);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 1500);
    });

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 1500);
    });
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2300);
    });
  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2300);
    });
  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, runtimeFunctionChange) {
  hvac->addSecondaryOutput(&secondaryOutput);

  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

  EXPECT_CALL(primaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));
  EXPECT_CALL(secondaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));

  EXPECT_CALL(primaryOutput, setOutputValue(_)).Times(AtLeast(1));
  EXPECT_CALL(secondaryOutput, setOutputValue(_)).Times(AtLeast(1));

  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
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

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();

  hvac->onInit();
  t1->onInit();
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    })
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleChannelConfigFinished();

  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL,
                               _,
                               sizeof(TChannelConfig_HVAC),
                               SUPLA_CONFIG_TYPE_DEFAULT))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL,
                               _,
                               sizeof(TChannelConfig_WeeklySchedule),
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
      .Times(1)
      .WillRepeatedly(Return(true));

  t1->setValue(30);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(15);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // function is already set to auto, so this will do nothing
  hvac->changeFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL, true);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags, SUPLA_HVAC_VALUE_FLAG_COOL |
            SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(proto,
              setChannelConfig(0,
                // cool
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                               _,
                               sizeof(TChannelConfig_HVAC),
                               SUPLA_CONFIG_TYPE_DEFAULT))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(proto,
              setChannelConfig(0,
                // cool
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                               _,
                               sizeof(TChannelConfig_WeeklySchedule),
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
      .WillOnce(Return(true));
  EXPECT_CALL(proto,
              setChannelConfig(0,
                // cool
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                               _,
                               sizeof(TChannelConfig_WeeklySchedule),
                               SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE))
      .WillOnce(Return(true));

  // cool
  hvac->changeFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT, true);
  hvac->setSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);  // ASDaSDASDASD
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOL |
                      SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });
  t1->setValue(45);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }


  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                               _,
                               sizeof(TChannelConfig_HVAC),
                               SUPLA_CONFIG_TYPE_DEFAULT))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(proto,
              setChannelConfig(0,
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                               _,
                               sizeof(TChannelConfig_WeeklySchedule),
                               SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t1->setValue(15);
  hvac->changeFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT, true);
  hvac->setSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    })
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  for (int i = 0; i < 60; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, countdownTimerTests) {
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
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
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

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();

  ::testing::Sequence seqPrimary;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  hvac->onInit();
  t1->onInit();
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(30);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });


  t1->setValue(10);
  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  for (int i = 0; i < 70; ++i) {
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

  hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
  hvacValue->SetpointTemperatureHeat = 2350;
  hvacValue->SetpointTemperatureCool = 2700;
  newValue.DurationSec = 20;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
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

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COUNTDOWN_TIMER);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2350);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
  hvacValue->SetpointTemperatureHeat = 2350;
  hvacValue->SetpointTemperatureCool = 2700;
  newValue.DurationSec = 20;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COUNTDOWN_TIMER);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2350);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2700);
    });

  hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
  hvacValue->SetpointTemperatureHeat = 2350;
  hvacValue->SetpointTemperatureCool = 2700;
  newValue.DurationSec = 20;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  // 10 s
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
  hvacValue->SetpointTemperatureHeat = 1900;
  hvacValue->SetpointTemperatureCool = 2500;
  newValue.DurationSec = 0;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
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
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2000);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
  hvacValue->SetpointTemperatureHeat = 2000;
  hvacValue->SetpointTemperatureCool = 2500;
  newValue.DurationSec = 20;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }


  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COUNTDOWN_TIMER);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2000);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvacValue->Mode = SUPLA_HVAC_MODE_OFF;
  hvacValue->SetpointTemperatureHeat = 2000;
  hvacValue->SetpointTemperatureCool = 2500;
  newValue.DurationSec = 20;

  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });
  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
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
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });
  hvac->applyNewRuntimeSettings(SUPLA_HVAC_MODE_HEAT, 2300, INT16_MIN, 20);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  // send timer and then break it with CMD_SWITCH_TO_MANUAL
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
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
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 3000);
    });

  hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
  hvacValue->SetpointTemperatureHeat = 2300;
  hvacValue->SetpointTemperatureCool = 3000;
  newValue.DurationSec = 20;

  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  // time +10 s, so in the middle of countdown timer
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvacValue->Mode = SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL;
  hvacValue->SetpointTemperatureHeat = 0;
  hvacValue->SetpointTemperatureCool = 0;
  hvacValue->Flags = 0;
  newValue.DurationSec = 0;

  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }
}

// When stored config is invalid, device should use SW default configuration
// instead
TEST_F(HvacIntegrationF, startupWithInvalidConfigLoadedFromStorage) {
  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

  EXPECT_CALL(primaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));

  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));

  int32_t storedFunction = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(storedFunction), Return(true)));

  uint8_t valueZero = 0;
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(valueZero), Return(true)));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(valueZero), Return(true)));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce([](const char *key, char *value, size_t blobSize) {
         TChannelConfig_HVAC config = {};
         config.UsedAlgorithm = 123;
         // invalid used algorithm
         memcpy(value, &config, blobSize);
         return true;
      });
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
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

  ::testing::Sequence seq1;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(10);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(15);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, startupWithValidConfigLoadedFromStorage) {
  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

  EXPECT_CALL(primaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));

  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));

  int32_t storedFunction = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(storedFunction), Return(true)));

  uint8_t valueZero = 0;
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(valueZero), Return(true)));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(valueZero), Return(true)));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce([](const char *key, char *value, size_t blobSize) {
         TChannelConfig_HVAC config = {};
         config.MainThermometerChannelNo = 2;
         config.AvailableAlgorithms =
             SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
         config.UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
         Supla::Control::HvacBase::setTemperatureInStruct(
             &config.Temperatures, TEMPERATURE_HISTERESIS, 30);
         config.Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT;
         memcpy(value, &config, blobSize);
         return true;
      });
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
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
  EXPECT_CALL(proto, sendChannelValueChanged(2, _, 0, 0)).Times(AtLeast(1));

  ::testing::Sequence seq1;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(23.5);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  t2->onInit();
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  t2->setValue(10);
  t1->setValue(40);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }

  t2->setValue(15);
  t1->setValue(45);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    t2->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, newValuesFromServer) {
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
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
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

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();

  ::testing::Sequence seqPrimary;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  hvac->onInit();
  t1->onInit();

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(30);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });


  t1->setValue(10);
  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  for (int i = 0; i < 70; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  TSD_SuplaChannelNewValue newValue = {};
  THVACValue *hvacValue = reinterpret_cast<THVACValue *>(newValue.value);
  hvacValue->Flags = 0;

  hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
  hvacValue->SetpointTemperatureHeat = 0;
  hvacValue->SetpointTemperatureCool = 0;
  newValue.DurationSec = 0;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  // +20s
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(1000);
  }

  hvacValue->Flags = SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;

  hvacValue->Mode = SUPLA_HVAC_MODE_HEAT;
  hvacValue->SetpointTemperatureHeat = 2300;
  hvacValue->SetpointTemperatureCool = 0;
  newValue.DurationSec = 0;
  EXPECT_EQ(hvac->handleNewValueFromServer(&newValue), 1);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2300);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
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

TEST_F(HvacIntegrationF, histeresisHeatingCheck) {
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
              setUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), _))
      .WillRepeatedly(Return(false));

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

  ::testing::Sequence seq1;

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);
  // set value 0

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // check values around target 21.0 (histeresis 0.4 C with "SETPOINT AT MOST")
  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  t1->setValue(18.0);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // it should stop heating
  t1->setValue(21.01);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, histeresisCoolingCheck) {
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
      cfg,
      getBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), _))
      .WillRepeatedly(Return(false));

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

  ::testing::Sequence seq1;

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL|
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C
  // cool
  hvac->getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);

  hvac->onLoadConfig(nullptr);
  hvac->setSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);
  // set value 0

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // check values around target 21.0 (histeresis 0.4 C with "SETPOINT AT MOST")
  hvac->setTargetMode(SUPLA_HVAC_MODE_COOL);
  t1->setValue(26.0);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // it should stop heating
  t1->setValue(24.99);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, histeresisHeatCoolCheck) {
  hvac->addSecondaryOutput(&secondaryOutput);

  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

  EXPECT_CALL(primaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));
  EXPECT_CALL(secondaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));

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
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), _))
      .WillRepeatedly(Return(false));

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

  ::testing::Sequence seq1;
  ::testing::Sequence seq2;

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(secondaryOutput, setOutputValue(0)).Times(1).InSequence(seq2);
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
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(secondaryOutput, setOutputValue(0)).Times(1).InSequence(seq2);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(secondaryOutput, setOutputValue(0)).Times(1).InSequence(seq2);
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
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(secondaryOutput, setOutputValue(1)).Times(1).InSequence(seq2);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(secondaryOutput, setOutputValue(0)).Times(1).InSequence(seq2);
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
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C
  hvac->getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);
  // set value 0

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // check values around target 21.0 (histeresis 0.4 C with "SETPOINT AT MOST")
  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT_COOL);
  t1->setValue(20.0);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // it should stop heating
  t1->setValue(23.0);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // nothing
  t1->setValue(25.39);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // start cooling
  t1->setValue(25.41);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // stop cooling
  t1->setValue(24.99);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}

TEST_F(HvacIntegrationF, buttonIntegrationCheck) {
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
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              getBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_aweekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
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

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();

  ::testing::Sequence seqPrimary;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  hvac->onInit();
  t1->onInit();

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::INCREASE_TEMPERATURE);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2150);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });


  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::DECREASE_TEMPERATURE);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });


  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  for (int i = 0; i < 20; i++) {
    hvac->handleAction(0, Supla::DECREASE_TEMPERATURE);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // check minimum supported temperature
  for (int i = 0; i < 50; i++) {
    hvac->handleAction(0, Supla::DECREASE_TEMPERATURE);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TURN_OFF);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // temperature setpoint change should enable thermostat
  hvac->handleAction(0, Supla::INCREASE_TEMPERATURE);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::INCREASE_HEATING_TEMPERATURE);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 600);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::INCREASE_COOLING_TEMPERATURE);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::DECREASE_HEATING_TEMPERATURE);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::DECREASE_COOLING_TEMPERATURE);

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::SWITCH_TO_WEEKLY_SCHEDULE_MODE);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::SWITCH_TO_MANUAL_MODE);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::SWITCH_TO_MANUAL_MODE_COOL);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::SWITCH_TO_MANUAL_MODE_HEAT_COOL);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TURN_OFF);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TOGGLE_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TOGGLE_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TOGGLE_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TOGGLE_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TOGGLE_OFF_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TOGGLE_OFF_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }


  hvac->handleAction(0, Supla::TOGGLE_OFF_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TOGGLE_OFF_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  hvac->handleAction(0, Supla::TOGGLE_OFF_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 550);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }


  hvac->handleAction(0, Supla::TOGGLE_OFF_MANUAL_WEEKLY_SCHEDULE_MODES);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }
}
