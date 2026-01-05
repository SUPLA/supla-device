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
#include <supla/sensor/virtual_binary.h>
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

class HvacChannelAssignmentTests : public ::testing::Test {
 protected:
  ConfigMock cfg;
  StorageMock storage;
  OutputSimulatorWithCheck primaryOutput;
  OutputSimulatorWithCheck secondaryOutput;
  SimpleTime time;
  ProtocolLayerMock proto;
  ClockStub clock;

  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }
};

TEST_F(HvacChannelAssignmentTests, binarySensorAsChannelZero) {
  EXPECT_CALL(cfg, init());

  Supla::Sensor::VirtualBinary s1;
  Supla::Control::HvacBase hvac(&primaryOutput);
  Supla::Sensor::VirtualThermometer t1;

  ASSERT_EQ(s1.getChannelNumber(), 0);
  ASSERT_EQ(hvac.getChannelNumber(), 1);
  ASSERT_EQ(t1.getChannelNumber(), 2);

  // init min max ranges for tempreatures setting and check again setters
  // for temperatures
  hvac.setDefaultTemperatureRoomMin(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL,
                                     500);  // 5 degrees
  hvac.setDefaultTemperatureRoomMin(SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                     500);  // 5 degrees
  hvac.setDefaultTemperatureRoomMax(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL,
                                     5000);  // 50 degrees
  hvac.setDefaultTemperatureRoomMax(SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                     5000);     // 50 degrees
  hvac.setTemperatureHisteresisMin(20);        // 0.2 degree
  hvac.setTemperatureHisteresisMax(1000);      // 10 degree
  hvac.setTemperatureHeatCoolOffsetMin(200);   // 2 degrees
  hvac.setTemperatureHeatCoolOffsetMax(1000);  // 10 degrees
  hvac.setTemperatureAuxMin(500);              // 5 degrees
  hvac.setTemperatureAuxMax(7500);             // 75 degrees
  hvac.addAvailableAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);

  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt32(StrEq("1_fnc"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("1_cfg_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getUInt8(StrEq("1_weekly_chng"), _))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg, getBlob(StrEq("1_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(
      cfg,
      getBlob(StrEq("1_hvac_weekly"), _, sizeof(TChannelConfig_WeeklySchedule)))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(cfg,
              setInt32(StrEq("1_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(cfg,
              setBlob(StrEq("1_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("1_hvac_aweekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("1_weekly_chng"), _))
      .WillRepeatedly(Return(true));

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
  EXPECT_CALL(primaryOutput, setOutputValueCheck(0)).Times(1);

  t1.setValue(23.5);
  t1.setRefreshIntervalMs(100);

//  hvac.setMainThermometerChannelNo(1);
  hvac.setTemperatureHisteresis(40);  // 0.4 C

  hvac.onLoadConfig(nullptr);
  hvac.onLoadState();
  hvac.onInit();
  t1.onInit();
  // set value 0

  for (int i = 0; i < 10; ++i) {
    hvac.iterateAlways();
    t1.iterateAlways();
    time.advance(100);
  }

  hvac.onRegistered(nullptr);

  for (int i = 0; i < 50; ++i) {
    hvac.iterateAlways();
    t1.iterateAlways();
    hvac.iterateConnected();
    t1.iterateConnected();
    time.advance(100);
  }

  EXPECT_EQ(hvac.getBinarySensorChannelNo(), -1);
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), -1);
  EXPECT_EQ(hvac.getAuxThermometerChannelNo(), -1);
}

