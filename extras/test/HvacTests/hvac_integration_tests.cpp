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

using ::testing::_;
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
    hvac->setTemperatureAutoOffsetMin(200);     // 2 degrees
    hvac->setTemperatureAutoOffsetMax(1000);    // 10 degrees
    hvac->setTemperatureHeaterCoolerMin(500);   // 5 degrees
    hvac->setTemperatureHeaterCoolerMax(7500);  // 75 degrees
    hvac->addAlgorithmCap(SUPLA_HVAC_ALGORITHM_ON_OFF);
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
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, 64))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TSD_ChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(storage, readState(_, sizeof(THVACValue)))
      .WillOnce([](unsigned char *data, int size) {
        THVACValue hvacValue = {};
        memcpy(data, &hvacValue, sizeof(THVACValue));
        return sizeof(THVACValue);
      });

  EXPECT_CALL(storage, readState(_, 1))
      .WillOnce([](unsigned char *data, int size) {
        uint8_t lastWorkingMode = 0;
        memcpy(data, &lastWorkingMode, 1);
        return 1;
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
    });


  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
    });

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig();
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();

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

  // check values around target 18.0 (histeresis 0.4 C, so +- 0.2 C)
  t1->setValue(17.9);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(18.1);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(18.2);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // should stop heating
  t1->setValue(18.21);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(18.1);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(17.8);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // start heating
  t1->setValue(17.79);
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
}

TEST_F(HvacIntegrationF, startupWithEmptyConfigCooling) {
  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());


  EXPECT_CALL(primaryOutput, isOnOffOnly()).WillRepeatedly(Return(true));

//  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, 64))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TSD_ChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
//  EXPECT_CALL(cfg,
//              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL))
//      .Times(1)
//      .WillOnce(Return(true));

  EXPECT_CALL(storage, readState(_, sizeof(THVACValue)))
      .WillOnce([](unsigned char *data, int size) {
        THVACValue hvacValue = {};
        memcpy(data, &hvacValue, sizeof(THVACValue));
        return sizeof(THVACValue);
      });

  EXPECT_CALL(storage, readState(_, 1))
      .WillOnce([](unsigned char *data, int size) {
        uint8_t lastWorkingMode = 0;
        memcpy(data, &lastWorkingMode, 1);
        return 1;
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  EXPECT_CALL(primaryOutput, setOutputValue(1)).Times(1).InSequence(seq1);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL);

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig();
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();

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
  t1->setValue(23.9);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(23.8);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // should stop cooling
  t1->setValue(23.7);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(24);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(24.1);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(24.2);
  for (int i = 0; i < 50; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // start cooling
  t1->setValue(24.21);
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
}

TEST_F(HvacIntegrationF, startupWithEmptyConfigAuto) {
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
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, 64))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TSD_ChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(storage, readState(_, sizeof(THVACValue)))
      .WillOnce([](unsigned char *data, int size) {
        THVACValue hvacValue = {};
        memcpy(data, &hvacValue, sizeof(THVACValue));
        return sizeof(THVACValue);
      });

  EXPECT_CALL(storage, readState(_, 1))
      .WillOnce([](unsigned char *data, int size) {
        uint8_t lastWorkingMode = 0;
        memcpy(data, &lastWorkingMode, 1);
        return 1;
      });

  // ignore channel value changed from thermometer
  EXPECT_CALL(proto, sendChannelValueChanged(1, _, 0, 0)).Times(AtLeast(1));

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig();
  hvac->onLoadState();

  ::testing::Sequence seqPrimary;
  ::testing::Sequence seqSecondary;
  EXPECT_CALL(primaryOutput, setOutputValue(0)).Times(1).InSequence(seqPrimary);
  EXPECT_CALL(secondaryOutput, setOutputValue(0))
      .Times(1)
      .InSequence(seqSecondary);

  hvac->onInit();
  t1->onInit();

  hvac->onRegistered(nullptr);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });


  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seqPrimary, seqSecondary)
    .WillOnce([](uint8_t channelNumber, char *value, unsigned char offline,
                 uint32_t validityTimeSec) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
  t1->setValue(17.9);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(18.1);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(18.2);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // should stop heating
  t1->setValue(18.21);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(18.1);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // no change
  t1->setValue(17.8);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  // start heating
  t1->setValue(17.79);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  t1->setValue(24.3);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(23.8);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  t1->setValue(23.7);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_AUTO);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_AUTO);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  t1->setValue(24.3);
  hvac->setTargetMode(SUPLA_HVAC_MODE_AUTO);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_AUTO);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                      SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_AUTO);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  t1->setValue(15);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(18.2);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_AUTO);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  t1->setValue(18.3);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }


  t1->setValue(19);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(20);
  for (int i = 0; i < 20; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    hvac->iterateConnected();
    t1->iterateConnected();
    time.advance(100);
  }

  t1->setValue(21);
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

  t1->setValue(19);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_AUTO);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
    });

  t1->setValue(24.3);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_AUTO);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                      SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MAX_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_AUTO);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, 1800);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 2400);
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

  EXPECT_CALL(cfg, getInt32(StrEq("0_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("0_weekly_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("0_hvac_cfg"), _, 64))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(
          StrEq("0_hvac_weekly"), _, sizeof(TSD_ChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
//  EXPECT_CALL(cfg,
//              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL))
//      .Times(1)
//      .WillOnce(Return(true));

  EXPECT_CALL(storage, readState(_, sizeof(THVACValue)))
      .WillOnce([](unsigned char *data, int size) {
        THVACValue hvacValue = {};
        memcpy(data, &hvacValue, sizeof(THVACValue));
        return sizeof(THVACValue);
      });

  EXPECT_CALL(storage, readState(_, 1))
      .WillOnce([](unsigned char *data, int size) {
        uint8_t lastWorkingMode = 0;
        memcpy(data, &lastWorkingMode, 1);
        return 1;
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
    });

  hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(23.5);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setHeaterCoolerThermometerChannelNo(2);
  hvac->setHeaterCoolerThermometerType(
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_GENERIC_HEATER);
  hvac->setTemperatureHisteresis(400);  // 4.00 C

  hvac->onLoadConfig();
  hvac->onLoadState();
  hvac->setTemperatureSetpointMin(-500);  // -5
  hvac->onInit();
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
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
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_MIN_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureMin, -500);
        EXPECT_EQ(hvacValue->SetpointTemperatureMax, 0);
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
}
