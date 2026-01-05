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

class HvacRuntimeF : public ::testing::Test {
 protected:
  ConfigMock cfg;
  StorageMock storage;
  OutputSimulator primaryOutput;
  OutputSimulator secondaryOutput;
  SimpleTime time;
  ProtocolLayerMock proto;
  ClockStub clock;

  Supla::Control::HvacBase *hvac = {};
  Supla::Sensor::VirtualThermometer *t1 = {};
  Supla::Sensor::VirtualThermometer *t2 = {};

  void SetUp() override {
    Supla::Channel::resetToDefaults();

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

  void iterateAndMoveTime(int times) {
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

TEST_F(HvacRuntimeF, antifreezeCheck) {
  EXPECT_CALL(cfg, init());
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

  ::testing::Sequence seq1;

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(20.0);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setAuxThermometerChannelNo(2);
  hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
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


  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  t2->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);

  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  hvac->setTemperatureFreezeProtection(500);  // 5 degrees
  hvac->setAuxMinMaxSetpointEnabled(true);
  hvac->setTemperatureAuxMinSetpoint(1500);
  hvac->setTemperatureAuxMaxSetpoint(7500);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  iterateAndMoveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
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
  // check values around target 21.0 (histeresis 0.4 C with "SETPOINT AT MOST")
  hvac->setTargetMode(SUPLA_HVAC_MODE_HEAT);
  t1->setValue(18.0);
  iterateAndMoveTime(50);
  EXPECT_EQ(primaryOutput.getOutputValue(), 1);

  // it shouldn't stop heating
  t2->setValue(55.01);
  iterateAndMoveTime(50);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagHeating());


  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
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


  // it should stop heating
  t2->setValue(80.01);
  iterateAndMoveTime(50);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagHeating());

  // check if antifreeze protection didn't start when aux max is too high
  t1->setValue(1.0);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagHeating());

  // check if antifreeze protection start when aux protection is disabled
  // give it more time, since there is 5s delay after config change
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING |
                  SUPLA_HVAC_VALUE_FLAG_ANTIFREEZE_OVERHEAT_ACTIVE);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  t1->setValue(1.0);
  hvac->setAuxMinMaxSetpointEnabled(false);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 1);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagHeating());

  // check if antifreeze protection stops when aux protection is enabled again.
  // give it more time, since there is 5s delay after config change
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
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
  t1->setValue(1.0);
  hvac->setAuxMinMaxSetpointEnabled(true);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagHeating());

  // check the same with target mode OFF
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2100);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagHeating());

  t1->setValue(20.0);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagHeating());

  // mode if off, so aux min/max shouldn't be used to enable heating
  t2->setValue(10.01);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagHeating());
}

TEST_F(HvacRuntimeF, antifreezeCheckInCoolMode) {
  EXPECT_CALL(cfg, init());
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

  ::testing::Sequence seq1;

  t1->setValue(18.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(20.0);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setAuxThermometerChannelNo(2);
  hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
  hvac->setTemperatureHisteresis(40);  // 0.4 C
  hvac->setDefaultSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);

  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                      SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });


  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  t1->onInit();
  t2->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);

  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  hvac->setTemperatureFreezeProtection(500);  // 5 degrees
  hvac->setAuxMinMaxSetpointEnabled(true);
  hvac->setTemperatureAuxMinSetpoint(1500);
  hvac->setTemperatureAuxMaxSetpoint(7500);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  iterateAndMoveTime(50);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
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
  hvac->setTargetMode(SUPLA_HVAC_MODE_COOL);
  t1->setValue(28.0);
  iterateAndMoveTime(50);
  EXPECT_EQ(primaryOutput.getOutputValue(), -1);

  t2->setValue(55.01);
  iterateAndMoveTime(50);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagCooling());

  t2->setValue(80.01);
  iterateAndMoveTime(50);
  EXPECT_EQ(primaryOutput.getOutputValue(), -1);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagCooling());

  // overheat protection still works...
  t1->setValue(1.0);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), -1);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagCooling());

  // check if antifreeze protection start when aux protection is disabled
  // give it more time, since there is 5s delay after config change
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_COOL);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });
  t1->setValue(1.0);
  hvac->setAuxMinMaxSetpointEnabled(false);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagCooling());

  // check if antifreeze protection stops when aux protection is enabled again.
  // give it more time, since there is 5s delay after config change
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
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
  t1->setValue(1.0);
  hvac->setAuxMinMaxSetpointEnabled(true);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), -1);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagCooling());

  // check the same with target mode OFF
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagCooling());

  t1->setValue(20.0);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagCooling());

  // mode if off, so aux min/max shouldn't be used to enable cooling
  t2->setValue(100.01);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagCooling());

  // set mode to off, check if overheat protection works
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET |
                  SUPLA_HVAC_VALUE_FLAG_COOL |
                  SUPLA_HVAC_VALUE_FLAG_COOLING |
                  SUPLA_HVAC_VALUE_FLAG_ANTIFREEZE_OVERHEAT_ACTIVE);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 0);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 2500);
    });
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  t1->setValue(38.0);
//  t2->setValue(30.01);
  hvac->setTemperatureHeatProtection(3200);
  iterateAndMoveTime(100);
  EXPECT_EQ(primaryOutput.getOutputValue(), -1);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagCooling());
}


TEST_F(HvacRuntimeF, antifreezeCheckWithProgramAndOffMode) {
  EXPECT_CALL(cfg, init());
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

  ::testing::Sequence seq1;

  t1->setValue(23.5);
  t1->setRefreshIntervalMs(100);
  t2->setValue(20.0);
  t2->setRefreshIntervalMs(100);

  hvac->setMainThermometerChannelNo(1);
  hvac->setAuxThermometerChannelNo(2);
  hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
  hvac->setTemperatureHisteresis(40);  // 0.4 C

  EXPECT_EQ(primaryOutput.getOutputValue(), 0);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });


  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  hvac->setTemperatureSetpointChangeSwitchesToManualMode(false);
  t1->onInit();
  t2->onInit();
  hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);

  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  hvac->setTemperatureFreezeProtection(500);  // 5 degrees
  hvac->setAuxMinMaxSetpointEnabled(true);
  hvac->setTemperatureAuxMinSetpoint(1500);
  hvac->setTemperatureAuxMaxSetpoint(7500);
  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    t1->iterateAlways();
    t2->iterateAlways();
    time.advance(100);
  }

  hvac->onRegistered(nullptr);

  iterateAndMoveTime(50);
  EXPECT_EQ(primaryOutput.getOutputValue(), 0);

  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  // check values around target 19.0 (histeresis 0.4 C with "SETPOINT AT MOST")
  t1->setValue(17.0);
  iterateAndMoveTime(50);

  // set mode "off"
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF, false);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  iterateAndMoveTime(100);

  t1->setValue(1.0);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING |
                  SUPLA_HVAC_VALUE_FLAG_ANTIFREEZE_OVERHEAT_ACTIVE);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  iterateAndMoveTime(100);

  t1->setValue(15.0);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  iterateAndMoveTime(100);

  hvac->applyNewRuntimeSettings(SUPLA_HVAC_MODE_HEAT, 2500, INT16_MIN);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  iterateAndMoveTime(100);

  hvac->setTargetMode(SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 1900);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  iterateAndMoveTime(100);

  // manual override
  hvac->applyNewRuntimeSettings(SUPLA_HVAC_MODE_NOT_SET, 2500, INT16_MIN);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE |
                  SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE_TEMPORAL_OVERRIDE);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_HEAT);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  iterateAndMoveTime(100);

  // set mode "off"
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF, false);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  iterateAndMoveTime(100);

  t1->setValue(1.0);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
                  SUPLA_HVAC_VALUE_FLAG_HEATING |
                  SUPLA_HVAC_VALUE_FLAG_ANTIFREEZE_OVERHEAT_ACTIVE);
        EXPECT_EQ(hvacValue->IsOn, 1);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });
  iterateAndMoveTime(100);

  t1->setValue(15.0);
  EXPECT_CALL(proto, sendChannelValueChanged(0, _, 0, 0))
    .InSequence(seq1)
    .WillOnce([](uint8_t, int8_t *value, unsigned char,
                 uint32_t) {
        auto hvacValue = reinterpret_cast<THVACValue *>(value);

        EXPECT_EQ(hvacValue->Flags,
                  SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET);
        EXPECT_EQ(hvacValue->IsOn, 0);
        EXPECT_EQ(hvacValue->Mode, SUPLA_HVAC_MODE_OFF);
        EXPECT_EQ(hvacValue->SetpointTemperatureHeat, 2500);
        EXPECT_EQ(hvacValue->SetpointTemperatureCool, 0);
    });

  iterateAndMoveTime(100);
}

