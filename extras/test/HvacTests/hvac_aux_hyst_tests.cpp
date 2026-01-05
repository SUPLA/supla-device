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

class HvacAuxTestsF : public ::testing::Test {
 protected:
  ConfigMock cfg;
  StorageMock storage;
  OutputSimulatorWithCheck primaryOutput;
  OutputSimulatorWithCheck secondaryOutput;
  SimpleTime time;
  ProtocolLayerMock proto;
  ClockStub clock;

  Supla::Control::HvacBase *hvac = {};
  Supla::Sensor::VirtualThermometer *t1 = {};
  Supla::Sensor::VirtualThermometer *t2 = {};

  void SetUp() override {
    Supla::Channel::resetToDefaults();
    EXPECT_CALL(cfg, init());

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
    delete hvac;
    delete t1;
    delete t2;
    Supla::Channel::resetToDefaults();
  }

  void moveTime(int times) {
    for (int i = 0; i < times; ++i) {
      hvac->iterateAlways();
      t1->iterateAlways();
      t2->iterateAlways();
      hvac->iterateConnected();
      t1->iterateConnected();
      t2->iterateConnected();
      time.advance(100);
    }
  }
};

TEST_F(HvacAuxTestsF, auxHysteresisMaxCheck) {
  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

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
          [](uint32_t, unsigned char *data, int, bool) {
            THVACValue hvacValue = {};
            memcpy(data, &hvacValue, sizeof(THVACValue));
            return sizeof(THVACValue);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(int16_t), _))
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
            int16_t value = INT16_MIN;
            memcpy(data, &value, sizeof(int16_t));
            return sizeof(int16_t);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
            *data = 0;
            return sizeof(uint8_t);
          });

  // ignore channel value changed from thermometer
  EXPECT_CALL(proto, sendChannelValueChanged(1, _, 0, 0)).Times(AtLeast(1));
  EXPECT_CALL(proto, sendChannelValueChanged(2, _, 0, 0)).Times(AtLeast(1));
  // ignore output changes
  EXPECT_CALL(primaryOutput, setOutputValueCheck(_)).Times(AtLeast(1));

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(15.0);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setAuxThermometerChannelNo(2);
  hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  t2->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  hvac->setTemperatureAuxMaxSetpoint(5000);
  hvac->setAuxMinMaxSetpointEnabled(true);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  // set value 0
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  // target temperature is 21.0, room temperature is 18.0, heating should start
  // aux temperature is still low
  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  t1->setValue(18.0);
  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  // t2 is set to 50.01, so aboce max aux setpoint limit, which works on "at
  // most" mode
  t2->setValue(50.01);
  moveTime(50);

  // nothing
  t2->setValue(49.9);
  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  // target temperature is 21.0, room temperature is 18.0, heating should start
  // aux temperature is below limit
  t2->setValue(49.55);
  moveTime(50);

  hvac->setTemperatureAuxHisteresis(1000);

  // target temperature is 21.0, room temperature is 18.0, heating should start
  t2->setValue(49.55);
  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t2->setValue(51.0);
  moveTime(50);

  for (int i = 50; i > 40; i--) {
    t2->setValue(static_cast<double>(i));
    moveTime(50);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  // it should start heating, because t2 is below max aux limit + hysteresis
  t2->setValue(39.0);
  moveTime(50);

  for (int i = 38; i > -20; i--) {
    t2->setValue(static_cast<double>(i));
    moveTime(50);
  }

  for (int i = -20; i < 50; i++) {
    t2->setValue(static_cast<double>(i));
    moveTime(50);
  }

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  t2->setValue(50.1);
  moveTime(50);

  t2->setValue(45.0);
  moveTime(50);

  t1->setValue(24.0);
  moveTime(50);

  t1->setValue(14.0);
  moveTime(50);

  t1->setValue(-14.0);
  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  t2->setValue(30.0);
  moveTime(50);
}

TEST_F(HvacAuxTestsF, auxHysteresisMinCheck) {
  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

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
          [](uint32_t, unsigned char *data, int, bool) {
            THVACValue hvacValue = {};
            memcpy(data, &hvacValue, sizeof(THVACValue));
            return sizeof(THVACValue);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(int16_t), _))
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
            int16_t value = INT16_MIN;
            memcpy(data, &value, sizeof(int16_t));
            return sizeof(int16_t);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
            *data = 0;
            return sizeof(uint8_t);
          });

  // ignore channel value changed from thermometer
  EXPECT_CALL(proto, sendChannelValueChanged(1, _, 0, 0)).Times(AtLeast(1));
  EXPECT_CALL(proto, sendChannelValueChanged(2, _, 0, 0)).Times(AtLeast(1));
  // ignore output changes
  EXPECT_CALL(primaryOutput, setOutputValueCheck(_)).Times(AtLeast(1));

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(15.0);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setAuxThermometerChannelNo(2);
  hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  t2->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  hvac->setTemperatureAuxMinSetpoint(1500);
  hvac->setAuxMinMaxSetpointEnabled(true);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  // set value 0
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  // target temperature is 21.0, room temperature is 18.0, heating should start
  // aux temperature is still low
  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);      // ON
  t1->setValue(18.0);                             //
  moveTime(50);                                   //
                                                  //
  t2->setValue(60.01);                            //
  moveTime(50);                                   //
                                                  //
  t2->setValue(10.0);                             //
  moveTime(50);                                   //
                                                  //
  t1->setValue(25.0);                             //
  moveTime(50);                                   //

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  t2->setValue(16.0);                             // OFF
  moveTime(50);                                   //
                                                  //
                                                  //
  t2->setValue(15.1);                             //
  moveTime(50);                                   //

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t2->setValue(14.9);                             // ON
  moveTime(50);                                   //
                                                  //
  hvac->setTemperatureAuxHisteresis(500);         //
  t2->setValue(19.1);                             //
  moveTime(50);                                   //

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  t2->setValue(20.1);                             // OFF
  moveTime(100);                                  //
}

TEST_F(HvacAuxTestsF, auxHysteresisMaxCoolinCheck) {
  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

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
          [](uint32_t, unsigned char *data, int, bool) {
            THVACValue hvacValue = {};
            memcpy(data, &hvacValue, sizeof(THVACValue));
            return sizeof(THVACValue);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(int16_t), _))
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
            int16_t value = INT16_MIN;
            memcpy(data, &value, sizeof(int16_t));
            return sizeof(int16_t);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
            *data = 0;
            return sizeof(uint8_t);
          });

  // ignore channel value changed from thermometer
  EXPECT_CALL(proto, sendChannelValueChanged(1, _, 0, 0)).Times(AtLeast(1));
  EXPECT_CALL(proto, sendChannelValueChanged(2, _, 0, 0)).Times(AtLeast(1));
  // ignore output changes
  EXPECT_CALL(primaryOutput, setOutputValueCheck(_)).Times(AtLeast(1));

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(15.0);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setAuxThermometerChannelNo(2);
  hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  t2->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  hvac->setTemperatureAuxMaxSetpoint(5000);
  hvac->setAuxMinMaxSetpointEnabled(true);
  hvac->setSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  // set value 0
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvac->setTargetMode(SUPLA_HVAC_MODE_COOL);
  moveTime(100);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(28.0);
  moveTime(100);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(21.0);
  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t2->setValue(50.1);
  moveTime(50);

  t2->setValue(49.6);
  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t2->setValue(49.5);
  moveTime(50);

  hvac->setTemperatureAuxHisteresis(500);
  moveTime(100);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t2->setValue(50.1);
  moveTime(50);

  t2->setValue(45.6);
  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t2->setValue(44.9);
  moveTime(50);
}

TEST_F(HvacAuxTestsF, auxHysteresisMinCoolinCheck) {
  EXPECT_EQ(hvac->getChannelNumber(), 0);
  EXPECT_EQ(hvac->getChannel()->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  EXPECT_TRUE(hvac->getChannel()->isWeeklyScheduleAvailable());

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
          [](uint32_t, unsigned char *data, int, bool) {
            THVACValue hvacValue = {};
            memcpy(data, &hvacValue, sizeof(THVACValue));
            return sizeof(THVACValue);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(int16_t), _))
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
            int16_t value = INT16_MIN;
            memcpy(data, &value, sizeof(int16_t));
            return sizeof(int16_t);
          });
  EXPECT_CALL(storage, readStorage(_, _, sizeof(uint8_t), _))
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
            *data = 0;
            return sizeof(uint8_t);
          });

  // ignore channel value changed from thermometer
  EXPECT_CALL(proto, sendChannelValueChanged(1, _, 0, 0)).Times(AtLeast(1));
  EXPECT_CALL(proto, sendChannelValueChanged(2, _, 0, 0)).Times(AtLeast(1));
  // ignore output changes
  EXPECT_CALL(primaryOutput, setOutputValueCheck(_)).Times(AtLeast(1));

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(15.0);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setAuxThermometerChannelNo(2);
  hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  t2->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  hvac->setTemperatureAuxMinSetpoint(1000);
  hvac->setAuxMinMaxSetpointEnabled(true);
  hvac->setSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  // set value 0
  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  hvac->setTargetMode(SUPLA_HVAC_MODE_COOL);
  moveTime(100);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t1->setValue(28.0);
  moveTime(100);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t2->setValue(9.9);
  moveTime(50);

  t2->setValue(10.35);
  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t2->setValue(10.41);
  moveTime(100);

  hvac->setTemperatureAuxHisteresis(500);
  moveTime(100);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t2->setValue(9.9);
  moveTime(50);

  t2->setValue(14.35);
  moveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOLING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });

  t2->setValue(15.41);
  moveTime(100);
}

