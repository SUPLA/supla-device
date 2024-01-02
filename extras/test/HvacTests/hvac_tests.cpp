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

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <protocol_layer_mock.h>
#include <simple_time.h>
#include <string.h>
#include <supla/control/hvac_base.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <supla/sensor/thermometer.h>
#include <output_mock.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::StrEq;
using ::testing::Return;
// using ::testing::Args;
// using ::testing::ElementsAre;

class HvacTestsF : public ::testing::Test {
 protected:
  SimpleTime time;
  void SetUp() override {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }
  void TearDown() override {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }
};

class HvacTestWithChannelSetupF : public HvacTestsF {
 protected:
  ConfigMock cfg;
  OutputMock output;

  Supla::Control::HvacBase *hvac = {};
  Supla::Sensor::Thermometer *t1 = {};
  Supla::Sensor::ThermHygroMeter *t2 = {};

  void SetUp() override {
    HvacTestsF::SetUp();

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
    HvacTestsF::TearDown();
    delete hvac;
    delete t1;
    delete t2;
  }
};

TEST_F(HvacTestsF, BasicChannelSetup) {
  OutputMock output;
  Supla::Control::HvacBase hvac(&output);

  auto ch = hvac.getChannel();
  ASSERT_NE(ch, nullptr);

  EXPECT_EQ(ch->getChannelNumber(), 0);
  EXPECT_EQ(ch->getChannelType(), SUPLA_CHANNELTYPE_HVAC);
  // default function is set in onInit based on supported modes
  // or loaded from config
  EXPECT_EQ(ch->getDefaultFunction(), 0);
  EXPECT_TRUE(ch->isWeeklyScheduleAvailable());
  EXPECT_NE(
      ch->getFlags() & SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE, 0);

  EXPECT_TRUE(hvac.isHeatingAndCoolingSupported());
  EXPECT_FALSE(hvac.isHeatCoolSupported());
  EXPECT_FALSE(hvac.isFanSupported());
  EXPECT_FALSE(hvac.isDrySupported());

  EXPECT_EQ(ch->getFuncList(), SUPLA_BIT_FUNC_HVAC_THERMOSTAT);

  // check fan
  hvac.setFanSupported(true);
  EXPECT_FALSE(hvac.isFanSupported());  // fan is not implemented
  EXPECT_EQ(ch->getFuncList(), SUPLA_BIT_FUNC_HVAC_THERMOSTAT);

  // check dry
  hvac.setDrySupported(true);
  EXPECT_FALSE(hvac.isDrySupported());  // dry is not implemented
  EXPECT_EQ(ch->getFuncList(), SUPLA_BIT_FUNC_HVAC_THERMOSTAT);

  // check HeatCool
  hvac.setHeatCoolSupported(true);
  EXPECT_TRUE(hvac.isHeatCoolSupported());
  EXPECT_EQ(ch->getFuncList(),
            SUPLA_BIT_FUNC_HVAC_THERMOSTAT |
                SUPLA_BIT_FUNC_HVAC_THERMOSTAT_HEAT_COOL);

  // check heating&cooling
  // HeatCool is also removed, because it requires both heat and cool support
  hvac.setHeatingAndCoolingSupported(false);
  EXPECT_FALSE(hvac.isHeatingAndCoolingSupported());
  EXPECT_EQ(ch->getFuncList(), 0);

  // check fan
  hvac.setFanSupported(false);
  EXPECT_FALSE(hvac.isFanSupported());
  EXPECT_EQ(ch->getFuncList(), 0);

  // check dry
  hvac.setDrySupported(false);
  EXPECT_FALSE(hvac.isDrySupported());
  EXPECT_EQ(ch->getFuncList(), 0);

  // check HeatCool
  hvac.setHeatCoolSupported(false);
  EXPECT_FALSE(hvac.isHeatCoolSupported());
  EXPECT_EQ(ch->getFuncList(), 0);

  // check heating&cooling
  hvac.setHeatingAndCoolingSupported(true);
  EXPECT_TRUE(hvac.isHeatingAndCoolingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_BIT_FUNC_HVAC_THERMOSTAT);

  // check if set HeatCool will also set cool and heat
  hvac.setHeatingAndCoolingSupported(false);
  EXPECT_FALSE(hvac.isHeatingAndCoolingSupported());
  EXPECT_FALSE(hvac.isHeatCoolSupported());

  hvac.setHeatCoolSupported(true);
  EXPECT_TRUE(hvac.isHeatCoolSupported());
  EXPECT_TRUE(hvac.isHeatingAndCoolingSupported());
  EXPECT_EQ(ch->getFuncList(),
            SUPLA_BIT_FUNC_HVAC_THERMOSTAT |
                SUPLA_BIT_FUNC_HVAC_THERMOSTAT_HEAT_COOL);

  hvac.enableDifferentialFunctionSupport();
  EXPECT_EQ(ch->getFuncList(),
            SUPLA_BIT_FUNC_HVAC_THERMOSTAT |
                SUPLA_BIT_FUNC_HVAC_THERMOSTAT_HEAT_COOL |
                SUPLA_BIT_FUNC_HVAC_THERMOSTAT_DIFFERENTIAL);
}

TEST_F(HvacTestsF, checkDefaultFunctionInitizedByOnInit) {
  OutputMock output;
  Supla::Control::HvacBase hvac(&output);

  EXPECT_CALL(output, setOutputValue(0)).Times(1);

  auto *ch = hvac.getChannel();
  EXPECT_EQ(ch->getDefaultFunction(), 0);

  hvac.onInit();
  // check default function
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  EXPECT_TRUE(hvac.isHeatingSubfunction());
  EXPECT_FALSE(hvac.isCoolingSubfunction());
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);

  // check HeatCool
  hvac.setHeatCoolSupported(true);
  // init doesn't change default function when it was previously set
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  EXPECT_TRUE(hvac.isHeatingSubfunction());
  EXPECT_FALSE(hvac.isCoolingSubfunction());
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);


  // clear default function and check HeatCool again
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(),
            SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);

  // check heating & cooling
  hvac.setHeatingAndCoolingSupported(true);
  hvac.setHeatCoolSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  EXPECT_TRUE(hvac.isHeatingSubfunction());
  EXPECT_FALSE(hvac.isCoolingSubfunction());
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);

  // check dry - not implemented yet
  hvac.setHeatingAndCoolingSupported(false);
  hvac.setHeatCoolSupported(false);
  hvac.setDrySupported(true);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), 0);

  // check fan - not implemented yet
  hvac.setHeatingAndCoolingSupported(false);
  hvac.setHeatCoolSupported(false);
  hvac.setDrySupported(false);
  hvac.setFanSupported(true);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), 0);

  // check with all options enabled
  hvac.setHeatingAndCoolingSupported(true);
  hvac.setHeatCoolSupported(true);
  hvac.setDrySupported(true);
  hvac.setFanSupported(true);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(),
            SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);

  // check will all options disabled
  hvac.setHeatingAndCoolingSupported(false);
  hvac.setHeatCoolSupported(false);
  hvac.setDrySupported(false);
  hvac.setFanSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), 0);

}

TEST_F(HvacTestsF, checkDefaultFunctionInitizedByOnInitWithTwoOutputs) {
  OutputMock output1;
  OutputMock output2;
  Supla::Control::HvacBase hvac(&output1, &output2);

  EXPECT_CALL(output1, setOutputValue(0)).Times(1);
  EXPECT_CALL(output2, setOutputValue(0)).Times(1);

  auto *ch = hvac.getChannel();
  EXPECT_EQ(ch->getDefaultFunction(), 0);

  hvac.onInit();
  // check default function
  EXPECT_EQ(ch->getDefaultFunction(),
            SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);

  EXPECT_EQ(ch->getFuncList(),
            SUPLA_BIT_FUNC_HVAC_THERMOSTAT |
                SUPLA_BIT_FUNC_HVAC_THERMOSTAT_HEAT_COOL);

  hvac.enableDifferentialFunctionSupport();
  EXPECT_EQ(ch->getFuncList(), SUPLA_BIT_FUNC_HVAC_THERMOSTAT_HEAT_COOL |
                            SUPLA_BIT_FUNC_HVAC_THERMOSTAT_DIFFERENTIAL |
                            SUPLA_BIT_FUNC_HVAC_THERMOSTAT);
}

TEST_F(HvacTestsF, handleChannelConfigTestsOnEmptyElement) {
  OutputMock output;
  Supla::Control::HvacBase hvac(&output);

  Supla::Sensor::Thermometer t1;
  Supla::Sensor::ThermHygroMeter t2;
  EXPECT_CALL(output, setOutputValue(0)).Times(1);

  ASSERT_EQ(hvac.getChannelNumber(), 0);
  ASSERT_EQ(t1.getChannelNumber(), 1);
  ASSERT_EQ(t2.getChannelNumber(), 2);

  // init min max ranges for tempreatures setting and check again setters
  // for temperatures
  hvac.setTemperatureRoomMin(500);           // 5 degrees
  hvac.setTemperatureRoomMax(5000);          // 50 degrees
  hvac.setTemperatureHisteresisMin(20);      // 0.2 degree
  hvac.setTemperatureHisteresisMax(1000);    // 10 degree
  hvac.setTemperatureHeatCoolOffsetMin(200);     // 2 degrees
  hvac.setTemperatureHeatCoolOffsetMax(1000);    // 10 degrees
  hvac.setTemperatureAuxMin(500);   // 5 degrees
  hvac.setTemperatureAuxMax(7500);  // 75 degrees

  EXPECT_EQ(hvac.handleChannelConfig(nullptr), SUPLA_CONFIG_RESULT_DATA_ERROR);

  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;

  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED);


  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED);

  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // invalid config size
  configFromServer.ConfigSize = sizeof(TChannelConfig_HVAC) - 1;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  // main thermometer is not set, however we accept such config
  configFromServer.ConfigSize = sizeof(TChannelConfig_HVAC);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  TChannelConfig_HVAC *hvacConfig =
      reinterpret_cast<TChannelConfig_HVAC *>(&configFromServer.Config);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET;

  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->MainThermometerChannelNo = 0;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->MainThermometerChannelNo = 2;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // invalid thermometer channel number
  hvacConfig->MainThermometerChannelNo = 3;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  hvacConfig->AuxThermometerChannelNo = 0;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  hvacConfig->AuxThermometerChannelNo = 1;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  hvacConfig->AuxThermometerChannelNo = 3;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  hvacConfig->AuxThermometerChannelNo = 2;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->MainThermometerChannelNo = 2;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  hvacConfig->AuxThermometerChannelNo = 1;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->UsedAlgorithm = 15;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // Check hvacConfig with temperatures configured
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ECO, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ECO, 1600);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_COMFORT, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_COMFORT, 2200);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

// TEMPERATURE_BOOST
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BOOST, 10000);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BOOST, 4000);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_FREEZE_PROTECTION
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION, 1000);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_HEAT_PROTECTION
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEAT_PROTECTION, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_HISTERESIS
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HISTERESIS, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HISTERESIS, 100);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_BELOW_ALARM
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, 1800);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_ABOVE_ALARM
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_AUX_MAX_SETPOINT
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MAX_SETPOINT, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MAX_SETPOINT, 2000);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_AUX_MIN_SETPOINT
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MIN_SETPOINT, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  // aux min setpoint can be lower than aux max in message from server,
  // it will be corrected by device afterwards
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MIN_SETPOINT, 2000);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MIN_SETPOINT, 1000);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // check if themperatures and other paramters were set on the hvac config
  EXPECT_EQ(hvac.getTemperatureEco(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_ECO));

  EXPECT_EQ(hvac.getTemperatureComfort(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_COMFORT));

  EXPECT_EQ(hvac.getTemperatureBoost(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_BOOST));

  EXPECT_EQ(hvac.getTemperatureFreezeProtection(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION));

  EXPECT_EQ(hvac.getTemperatureHeatProtection(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_HEAT_PROTECTION));

  EXPECT_EQ(hvac.getTemperatureHisteresis(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_HISTERESIS));

  EXPECT_EQ(hvac.getTemperatureBelowAlarm(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM));

  EXPECT_EQ(hvac.getTemperatureAboveAlarm(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM));

  EXPECT_EQ(
      hvac.getTemperatureAuxMaxSetpoint(),
      Supla::Control::HvacBase::getTemperatureFromStruct(
          &hvacConfig->Temperatures, TEMPERATURE_AUX_MAX_SETPOINT));

  EXPECT_EQ(
      hvac.getTemperatureAuxMinSetpoint(),
      Supla::Control::HvacBase::getTemperatureFromStruct(
          &hvacConfig->Temperatures, TEMPERATURE_AUX_MIN_SETPOINT));

  EXPECT_EQ(hvac.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
}

TEST_F(HvacTestsF, temperatureSettersAndGetters) {
  Supla::Control::HvacBase hvac;
  Supla::Sensor::Thermometer t1;
  Supla::Sensor::ThermHygroMeter t2;

  ASSERT_EQ(hvac.getChannelNumber(), 0);
  ASSERT_EQ(t1.getChannelNumber(), 1);
  ASSERT_EQ(t2.getChannelNumber(), 2);
  // check temperatures - all setters for r/w parameters should fail, because
  // corresponding min/max values are not configured yet
  EXPECT_FALSE(hvac.setTemperatureEco(0));
  EXPECT_FALSE(hvac.setTemperatureEco(-1000));
  EXPECT_FALSE(hvac.setTemperatureEco(1000));

  EXPECT_FALSE(hvac.setTemperatureAboveAlarm(100));
  EXPECT_FALSE(hvac.setTemperatureAboveAlarm(-100));
  EXPECT_FALSE(hvac.setTemperatureAboveAlarm(0));

  EXPECT_FALSE(hvac.setTemperatureBelowAlarm(100));
  EXPECT_FALSE(hvac.setTemperatureBelowAlarm(-100));
  EXPECT_FALSE(hvac.setTemperatureBelowAlarm(0));

  EXPECT_FALSE(hvac.setTemperatureFreezeProtection(100));
  EXPECT_FALSE(hvac.setTemperatureFreezeProtection(-100));
  EXPECT_FALSE(hvac.setTemperatureFreezeProtection(0));

  EXPECT_FALSE(hvac.setTemperatureHeatProtection(100));
  EXPECT_FALSE(hvac.setTemperatureHeatProtection(-100));
  EXPECT_FALSE(hvac.setTemperatureHeatProtection(0));

  EXPECT_FALSE(hvac.setTemperatureComfort(100));
  EXPECT_FALSE(hvac.setTemperatureComfort(-100));
  EXPECT_FALSE(hvac.setTemperatureComfort(0));

  EXPECT_FALSE(hvac.setTemperatureBoost(100));
  EXPECT_FALSE(hvac.setTemperatureBoost(-100));
  EXPECT_FALSE(hvac.setTemperatureBoost(0));

  // there is default histeresis min/max configured
  EXPECT_TRUE(hvac.setTemperatureHisteresis(100));
  EXPECT_FALSE(hvac.setTemperatureHisteresis(-100));
  EXPECT_FALSE(hvac.setTemperatureHisteresis(0));

  EXPECT_FALSE(hvac.setTemperatureAuxMinSetpoint(100));
  EXPECT_FALSE(hvac.setTemperatureAuxMinSetpoint(-100));
  EXPECT_FALSE(hvac.setTemperatureAuxMinSetpoint(0));

  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(100));
  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(-100));
  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(0));

  // init min max ranges for tempreatures setting and check again setters
  // for temperatures
  hvac.setTemperatureRoomMin(500);           // 5 degrees
  hvac.setTemperatureRoomMax(5000);          // 50 degrees
  hvac.setTemperatureHisteresisMin(20);      // 0.2 degree
  hvac.setTemperatureHisteresisMax(1000);    // 10 degree
  hvac.setTemperatureHeatCoolOffsetMin(200);     // 2 degrees
  hvac.setTemperatureHeatCoolOffsetMax(1000);    // 10 degrees
  hvac.setTemperatureAuxMin(500);   // 5 degrees
  hvac.setTemperatureAuxMax(7500);  // 75 degrees

  EXPECT_EQ(hvac.getTemperatureRoomMin(), 500);
  EXPECT_EQ(hvac.getTemperatureRoomMax(), 5000);
  EXPECT_EQ(hvac.getTemperatureHisteresisMin(), 20);
  EXPECT_EQ(hvac.getTemperatureHisteresisMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureHeatCoolOffsetMin(), 200);
  EXPECT_EQ(hvac.getTemperatureHeatCoolOffsetMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureAuxMin(), 500);
  EXPECT_EQ(hvac.getTemperatureAuxMax(), 7500);

  hvac.setTemperatureRoomMin(600);
  EXPECT_EQ(hvac.getTemperatureRoomMin(), 600);
  EXPECT_EQ(hvac.getTemperatureRoomMax(), 5000);
  EXPECT_EQ(hvac.getTemperatureHisteresisMin(), 20);
  EXPECT_EQ(hvac.getTemperatureHisteresisMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureHeatCoolOffsetMin(), 200);
  EXPECT_EQ(hvac.getTemperatureHeatCoolOffsetMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureAuxMin(), 500);
  EXPECT_EQ(hvac.getTemperatureAuxMax(), 7500);

  EXPECT_FALSE(hvac.setTemperatureEco(0));
  EXPECT_FALSE(hvac.setTemperatureEco(-1000));
  EXPECT_TRUE(hvac.setTemperatureEco(1000));
  EXPECT_TRUE(hvac.setTemperatureEco(600));
  EXPECT_TRUE(hvac.setTemperatureEco(5000));
  EXPECT_FALSE(hvac.setTemperatureEco(599));
  EXPECT_FALSE(hvac.setTemperatureEco(5001));
  EXPECT_EQ(hvac.getTemperatureEco(), 5000);

  EXPECT_TRUE(hvac.setTemperatureComfort(2000));
  EXPECT_FALSE(hvac.setTemperatureComfort(-1000));
  EXPECT_TRUE(hvac.setTemperatureComfort(1000));
  EXPECT_TRUE(hvac.setTemperatureComfort(2000));
  EXPECT_TRUE(hvac.setTemperatureComfort(2400));
  EXPECT_FALSE(hvac.setTemperatureComfort(599));
  EXPECT_FALSE(hvac.setTemperatureComfort(5001));
  EXPECT_EQ(hvac.getTemperatureComfort(), 2400);

  EXPECT_TRUE(hvac.setTemperatureBoost(2000));
  EXPECT_FALSE(hvac.setTemperatureBoost(-1000));
  EXPECT_TRUE(hvac.setTemperatureBoost(1000));
  EXPECT_TRUE(hvac.setTemperatureBoost(2000));
  EXPECT_TRUE(hvac.setTemperatureBoost(3000));
  EXPECT_FALSE(hvac.setTemperatureBoost(599));
  EXPECT_FALSE(hvac.setTemperatureBoost(5001));
  EXPECT_EQ(hvac.getTemperatureBoost(), 3000);

  EXPECT_FALSE(hvac.setTemperatureHisteresis(2000));
  EXPECT_FALSE(hvac.setTemperatureHisteresis(-1000));
  EXPECT_FALSE(hvac.setTemperatureHisteresis(0));
  EXPECT_FALSE(hvac.setTemperatureHisteresis(10));
  EXPECT_TRUE(hvac.setTemperatureHisteresis(20));
  EXPECT_TRUE(hvac.setTemperatureHisteresis(1000));
  EXPECT_TRUE(hvac.setTemperatureHisteresis(100));
  EXPECT_FALSE(hvac.setTemperatureHisteresis(10));
  EXPECT_EQ(hvac.getTemperatureHisteresis(), 100);

  EXPECT_TRUE(hvac.setTemperatureAuxMinSetpoint(2000));
  EXPECT_FALSE(hvac.setTemperatureAuxMinSetpoint(-1000));
  EXPECT_FALSE(hvac.setTemperatureAuxMinSetpoint(0));
  EXPECT_FALSE(hvac.setTemperatureAuxMinSetpoint(10));
  EXPECT_TRUE(hvac.setTemperatureAuxMinSetpoint(500));
  EXPECT_TRUE(hvac.setTemperatureAuxMinSetpoint(1000));
  EXPECT_TRUE(hvac.setTemperatureAuxMinSetpoint(5000));
  EXPECT_FALSE(hvac.setTemperatureAuxMinSetpoint(10));
  EXPECT_EQ(hvac.getTemperatureAuxMinSetpoint(), 5000);

  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(2000));
  EXPECT_TRUE(hvac.setTemperatureAuxMaxSetpoint(6000));
  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(-1000));
  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(0));
  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(10));
  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(501));

  // change min setpoint
  EXPECT_TRUE(hvac.setTemperatureAuxMinSetpoint(500));
  // ang check max setpoint (min setpoint offset is not kept)
  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(501));
  EXPECT_TRUE(hvac.setTemperatureAuxMaxSetpoint(1000));
  EXPECT_FALSE(hvac.setTemperatureAuxMaxSetpoint(10));
  EXPECT_EQ(hvac.getTemperatureAuxMaxSetpoint(), 1000);

  // change min setpoint (higher than max setpoint)
  EXPECT_FALSE(hvac.setTemperatureAuxMinSetpoint(1500));

  // check below and above alarm setters
  EXPECT_FALSE(hvac.setTemperatureBelowAlarm(-10));
  EXPECT_TRUE(hvac.setTemperatureAboveAlarm(1000));
  EXPECT_FALSE(hvac.setTemperatureBelowAlarm(500));
  EXPECT_FALSE(hvac.setTemperatureAboveAlarm(500));
  EXPECT_TRUE(hvac.setTemperatureBelowAlarm(5000));
  EXPECT_TRUE(hvac.setTemperatureAboveAlarm(4500));
  EXPECT_TRUE(hvac.setTemperatureBelowAlarm(1000));
  EXPECT_TRUE(hvac.setTemperatureAboveAlarm(1000));
}

TEST_F(HvacTestsF, otherConfigurationSettersAndGetters) {
  Supla::Control::HvacBase hvac;
  Supla::Sensor::Thermometer t1;
  Supla::Sensor::ThermHygroMeter t2;

  EXPECT_EQ(hvac.getUsedAlgorithm(), SUPLA_HVAC_ALGORITHM_NOT_SET);

  hvac.onInit();

  EXPECT_EQ(hvac.getMinOnTimeS(), 0);
  EXPECT_EQ(hvac.getMinOffTimeS(), 0);
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 0);
  EXPECT_EQ(hvac.getAuxThermometerChannelNo(), 0);
  EXPECT_EQ(hvac.getAuxThermometerType(),
            SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET);
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);

  EXPECT_FALSE(hvac.isAntiFreezeAndHeatProtectionEnabled());

  EXPECT_EQ(hvac.getMinOnTimeS(), 0);
  EXPECT_EQ(hvac.getMinOffTimeS(), 0);

  EXPECT_TRUE(
      hvac.setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE));
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  EXPECT_FALSE(hvac.setUsedAlgorithm(999));
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);

  EXPECT_TRUE(hvac.setMinOnTimeS(10));
  EXPECT_EQ(hvac.getMinOnTimeS(), 10);
  EXPECT_FALSE(hvac.setMinOnTimeS(10000));
  EXPECT_EQ(hvac.getMinOnTimeS(), 10);

  EXPECT_TRUE(hvac.setMinOffTimeS(20));
  EXPECT_EQ(hvac.getMinOffTimeS(), 20);
  EXPECT_FALSE(hvac.setMinOffTimeS(10000));
  EXPECT_EQ(hvac.getMinOffTimeS(), 20);

  hvac.setAntiFreezeAndHeatProtectionEnabled(true);
  EXPECT_FALSE(hvac.isAntiFreezeAndHeatProtectionEnabled());
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  EXPECT_TRUE(hvac.isAntiFreezeAndHeatProtectionEnabled());
  hvac.setAntiFreezeAndHeatProtectionEnabled(false);
  EXPECT_FALSE(hvac.isAntiFreezeAndHeatProtectionEnabled());

  EXPECT_TRUE(hvac.setMainThermometerChannelNo(0));
  EXPECT_FALSE(hvac.setAuxThermometerChannelNo(10));
  EXPECT_EQ(hvac.getAuxThermometerType(),
            SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET);

  EXPECT_TRUE(hvac.setMainThermometerChannelNo(1));
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 1);
  EXPECT_FALSE(hvac.setAuxThermometerChannelNo(1));
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 1);
  EXPECT_EQ(hvac.getAuxThermometerChannelNo(), 0);
  EXPECT_EQ(hvac.getAuxThermometerType(),
            SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET);

  EXPECT_TRUE(hvac.setAuxThermometerChannelNo(2));
  EXPECT_EQ(hvac.getAuxThermometerChannelNo(), 2);
  EXPECT_EQ(hvac.getAuxThermometerType(),
            SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED);
  hvac.setAuxThermometerType(
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_WATER);
  EXPECT_EQ(hvac.getAuxThermometerType(),
            SUPLA_HVAC_AUX_THERMOMETER_TYPE_WATER);

  // setting to the same channel number as hvac channel should remove
  // secondary thermometer
  EXPECT_TRUE(hvac.setAuxThermometerChannelNo(0));
  EXPECT_EQ(hvac.getAuxThermometerType(),
            SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET);

  EXPECT_TRUE(hvac.setOutputValueOnError(0));
  EXPECT_EQ(hvac.getOutputValueOnError(), 0);
  EXPECT_TRUE(hvac.setOutputValueOnError(1));
  EXPECT_EQ(hvac.getOutputValueOnError(), 1);
  EXPECT_TRUE(hvac.setOutputValueOnError(100));
  EXPECT_EQ(hvac.getOutputValueOnError(), 100);
  EXPECT_TRUE(hvac.setOutputValueOnError(101));
  EXPECT_EQ(hvac.getOutputValueOnError(), 100);
  EXPECT_TRUE(hvac.setOutputValueOnError(-1));
  EXPECT_EQ(hvac.getOutputValueOnError(), -1);
  EXPECT_TRUE(hvac.setOutputValueOnError(-100));
  EXPECT_EQ(hvac.getOutputValueOnError(), -100);
  EXPECT_TRUE(hvac.setOutputValueOnError(-101));
  EXPECT_EQ(hvac.getOutputValueOnError(), -100);
}

TEST_F(HvacTestWithChannelSetupF, handleChannelConfigWithConfigStorage) {
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_HVAC);
  TChannelConfig_HVAC *hvacConfig =
      reinterpret_cast<TChannelConfig_HVAC *>(&configFromServer.Config);
  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  hvacConfig->AuxThermometerChannelNo = 2;
  hvacConfig->AntiFreezeAndOverheatProtectionEnabled = 1;
  hvacConfig->MinOnTimeS = 10;
  hvacConfig->MinOffTimeS = 20;
  hvacConfig->OutputValueOnError = 100;
  hvacConfig->Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT;
  hvacConfig->UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ECO, 1600);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_COMFORT, 2200);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BOOST, 2500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION, 1000);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HISTERESIS, 100);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, 1800);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MAX_SETPOINT, 3000);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MIN_SETPOINT, 2000);

  EXPECT_CALL(output, setOutputValue(0)).Times(1);
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
  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT))
      .Times(1).WillOnce(Return(true));

  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_cfg_chng"), 0))
      .Times(1).WillOnce(Return(true));

  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillOnce(
          [](const char *key, const char *buf, int size) {
            TChannelConfig_HVAC expectedData = {
                .MainThermometerChannelNo = 1,
                .AuxThermometerChannelNo = 2,
                .AuxThermometerType = SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR,
                .AntiFreezeAndOverheatProtectionEnabled = 1,
                .AvailableAlgorithms =
                    SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE |
                    SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST,
                .UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE,
                .MinOnTimeS = 10,
                .MinOffTimeS = 20,
                .OutputValueOnError = 100,
                .Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT,
                .Temperatures = {}};

            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ECO, 1600);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_COMFORT, 2200);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_BOOST, 2500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_FREEZE_PROTECTION,
                1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS, 100);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_BELOW_ALARM, 1800);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_AUX_MAX_SETPOINT,
                3000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_AUX_MIN_SETPOINT,
                2000);

            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ROOM_MIN, 500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ROOM_MAX, 4000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MIN, 20);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MAX, 1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_HEAT_COOL_OFFSET_MIN,
                200);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_HEAT_COOL_OFFSET_MAX,
                1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_AUX_MIN, 500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_AUX_MAX,
                7500);

            EXPECT_EQ(0, memcmp(buf, &expectedData, size));
            return 1;
          });
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_weekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_aweekly"), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(cfg,
              setUInt8(StrEq("0_weekly_chng"), _))
      .WillRepeatedly(Return(true));

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  hvac->onRegistered(nullptr);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }
  EXPECT_EQ(hvac->handleChannelConfig(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);
}

TEST_F(HvacTestWithChannelSetupF, startupProcedureWithEmptyConfig) {
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
  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), 0))
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

  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .WillOnce(
          [](const char *key, const char *buf, int size) {
            TChannelConfig_HVAC expectedData = {
                .MainThermometerChannelNo = 1,
                .AuxThermometerChannelNo = 2,
                .AuxThermometerType = SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR,
                .AntiFreezeAndOverheatProtectionEnabled = 1,
                .AvailableAlgorithms =
                    SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE |
                    SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST,
                .UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE,
                .MinOnTimeS = 10,
                .MinOffTimeS = 20,
                .Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT,
                .Temperatures = {}};

            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ECO, 1600);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_COMFORT, 2200);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_BOOST, 2500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_FREEZE_PROTECTION,
                1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS, 100);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_BELOW_ALARM, 1800);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_AUX_MAX_SETPOINT,
                3000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_AUX_MIN_SETPOINT,
                2000);

            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ROOM_MIN, 500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ROOM_MAX, 4000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MIN, 20);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MAX, 1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_HEAT_COOL_OFFSET_MIN,
                200);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_HEAT_COOL_OFFSET_MAX,
                1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_AUX_MIN, 500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_AUX_MAX,
                7500);

            EXPECT_EQ(0, memcmp(buf, &expectedData, size));
            return 1;
          });

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();
  hvac->onRegistered(nullptr);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // send config from server
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_HVAC);
  TChannelConfig_HVAC *hvacConfig =
      reinterpret_cast<TChannelConfig_HVAC *>(&configFromServer.Config);
  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  hvacConfig->AuxThermometerChannelNo = 2;
  hvacConfig->AntiFreezeAndOverheatProtectionEnabled = 1;
  hvacConfig->MinOnTimeS = 10;
  hvacConfig->MinOffTimeS = 20;
  hvacConfig->Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT;
  hvacConfig->UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ECO, 1600);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_COMFORT, 2200);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BOOST, 2500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION, 1000);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HISTERESIS, 100);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, 1800);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MAX_SETPOINT, 3000);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MIN_SETPOINT, 2000);

  EXPECT_EQ(hvac->handleChannelConfig(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);

  EXPECT_EQ(hvac->getMainThermometerChannelNo(), 1);
}

TEST_F(HvacTestWithChannelSetupF,
       startupProcedureWithConfigChangedBeforeConnection) {
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

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .Times(1)
      .InSequence(s1)
      .WillOnce(Return(true));

  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), 1))
      .Times(1)
      .InSequence(s2)
      .WillOnce(Return(true));

  EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), 0))
      .Times(2)
      .InSequence(s2)
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

  EXPECT_CALL(cfg, setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
      .InSequence(s1)
      .WillOnce(
          [](const char *key, const char *buf, int size) {
            TChannelConfig_HVAC expectedData = {
                .MainThermometerChannelNo = 1,
                .AuxThermometerChannelNo = 2,
                .AuxThermometerType = SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR,
                .AntiFreezeAndOverheatProtectionEnabled = 1,
                .AvailableAlgorithms =
                    SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE |
                    SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST,
                .UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE,
                .MinOnTimeS = 10,
                .MinOffTimeS = 20,
                .Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT,
                .Temperatures = {}};

            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ECO, 1600);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_COMFORT, 2200);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_BOOST, 2500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_FREEZE_PROTECTION,
                1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS, 100);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_BELOW_ALARM, 1800);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_AUX_MAX_SETPOINT,
                3000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_AUX_MIN_SETPOINT,
                2000);

            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ROOM_MIN, 500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ROOM_MAX, 4000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MIN, 20);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MAX, 1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_HEAT_COOL_OFFSET_MIN,
                200);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_HEAT_COOL_OFFSET_MAX,
                1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_AUX_MIN, 500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_AUX_MAX,
                7500);

            EXPECT_EQ(0, memcmp(buf, &expectedData, size));
            return 1;
          });

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();

  hvac->setTemperatureEco(1600);

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // send config from server
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_HVAC);
  TChannelConfig_HVAC *hvacConfig =
      reinterpret_cast<TChannelConfig_HVAC *>(&configFromServer.Config);
  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  hvacConfig->AuxThermometerChannelNo = 2;
  hvacConfig->AntiFreezeAndOverheatProtectionEnabled = 1;
  hvacConfig->MinOnTimeS = 10;
  hvacConfig->MinOffTimeS = 20;
  hvacConfig->Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT;
  hvacConfig->UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ECO, 1600);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_COMFORT, 2200);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BOOST, 2500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION, 1000);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HISTERESIS, 100);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, 1800);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MAX_SETPOINT, 3000);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MIN_SETPOINT, 2000);

  EXPECT_EQ(hvac->handleChannelConfig(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);

  // above set config from server should be ignored
  EXPECT_EQ(hvac->getMainThermometerChannelNo(), 0);

  hvac->handleChannelConfigFinished();

  {
    ::testing::InSequence seq;

    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_HVAC),
                                 SUPLA_CONFIG_TYPE_DEFAULT))
        .Times(1)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
        .Times(1)
        .WillRepeatedly(Return(false));

    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_HVAC),
                                 SUPLA_CONFIG_TYPE_DEFAULT))
        .Times(1)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
        .Times(1)
        .WillRepeatedly(Return(false));

    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_HVAC),
                                 SUPLA_CONFIG_TYPE_DEFAULT))
        .Times(1)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
        .Times(1)
        .WillRepeatedly(Return(false));

    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_HVAC),
                                 SUPLA_CONFIG_TYPE_DEFAULT))
        .WillOnce([](uint8_t channelNumber,
                     _supla_int_t channelFunction,
                     void *buf,
                     int size,
                     uint8_t configType = SUPLA_CONFIG_TYPE_DEFAULT) {
          TChannelConfig_HVAC expectedData = {
              .MainThermometerChannelNo = 0,
              .AuxThermometerChannelNo = 0,
              .AuxThermometerType = SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET,
              .AntiFreezeAndOverheatProtectionEnabled = 0,
              .AvailableAlgorithms =
                  SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE |
                  SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST,
              .UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE,
              .MinOnTimeS = 0,
              .MinOffTimeS = 0,
              .Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT,
              .TemperatureSetpointChangeSwitchesToManualMode = 1,
              .Temperatures = {}};

          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_ECO, 1600);

          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_ROOM_MIN, 500);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_ROOM_MAX, 4000);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MIN, 20);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MAX, 1000);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures,
              TEMPERATURE_HEAT_COOL_OFFSET_MIN,
              200);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures,
              TEMPERATURE_HEAT_COOL_OFFSET_MAX,
              1000);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_AUX_MIN, 500);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_AUX_MAX, 7500);

          EXPECT_EQ(0, memcmp(buf, &expectedData, size));
          return true;
        });
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
        .Times(1)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE))
        .Times(1)
        .WillRepeatedly(Return(true));
  }

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // send reply from server
  TSDS_SetChannelConfigResult result = {
      .Result = SUPLA_CONFIG_RESULT_FALSE,
      .ConfigType = SUPLA_CONFIG_TYPE_DEFAULT,
      .ChannelNumber = 0
  };

  hvac->handleSetChannelConfigResult(&result);

  // send anothoer set channel config from server - this time it should be
  // applied to the channel
  EXPECT_EQ(hvac->handleChannelConfig(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);
}

TEST_F(HvacTestsF, checkTemperatureConfigCopy) {
  Supla::Control::HvacBase hvac;
  // init min max ranges for tempreatures setting and check again setters
  // for temperatures
  hvac.setDefaultTemperatureRoomMin(0,
                                    500);  // 5 degrees
  hvac.setDefaultTemperatureRoomMax(0,
                                    5000);   // 50 degrees
  hvac.setTemperatureHisteresisMin(20);      // 0.2 degree
  hvac.setTemperatureHisteresisMax(1000);    // 10 degree
  hvac.setTemperatureHeatCoolOffsetMin(200);     // 2 degrees
  hvac.setTemperatureHeatCoolOffsetMax(1000);    // 10 degrees
  hvac.setTemperatureAuxMin(500);   // 5 degrees
  hvac.setTemperatureAuxMax(7500);  // 75 degrees
  hvac.addAvailableAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  hvac.onInit();

  EXPECT_EQ(hvac.getTemperatureRoomMin(), 500);
  EXPECT_EQ(hvac.getTemperatureRoomMax(), 5000);
  EXPECT_EQ(hvac.getTemperatureHisteresisMin(), 20);
  EXPECT_EQ(hvac.getTemperatureHisteresisMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureHeatCoolOffsetMin(), 200);
  EXPECT_EQ(hvac.getTemperatureHeatCoolOffsetMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureAuxMin(), 500);
  EXPECT_EQ(hvac.getTemperatureAuxMax(), 7500);
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);

  Supla::Control::HvacBase hvac2;
  hvac.copyFixedChannelConfigTo(&hvac2);
  hvac2.setDefaultTemperatureRoomMin(0,
                                    500);  // 5 degrees
  hvac2.setDefaultTemperatureRoomMax(0,
                                    5000);   // 50 degrees
  hvac2.onInit();

  EXPECT_EQ(hvac2.getTemperatureRoomMin(), 500);
  EXPECT_EQ(hvac2.getTemperatureRoomMax(), 5000);
  EXPECT_EQ(hvac2.getTemperatureHisteresisMin(), 20);
  EXPECT_EQ(hvac2.getTemperatureHisteresisMax(), 1000);
  EXPECT_EQ(hvac2.getTemperatureHeatCoolOffsetMin(), 200);
  EXPECT_EQ(hvac2.getTemperatureHeatCoolOffsetMax(), 1000);
  EXPECT_EQ(hvac2.getTemperatureAuxMin(), 500);
  EXPECT_EQ(hvac2.getTemperatureAuxMax(), 7500);
  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
}

TEST_F(HvacTestWithChannelSetupF,
       startupProcedureWithInvalidConfigFromServerAfterRegister) {
  ProtocolLayerMock proto;
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
      .WillRepeatedly(Return(false));
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
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TChannelConfig_HVAC)))
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

  hvac->onLoadConfig(nullptr);
  hvac->onLoadState();
  hvac->onInit();

  hvac->onRegistered(nullptr);

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // send config from server
  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  configFromServer.ConfigSize = sizeof(TChannelConfig_HVAC);
  TChannelConfig_HVAC *hvacConfig =
      reinterpret_cast<TChannelConfig_HVAC *>(&configFromServer.Config);
  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->AuxThermometerType =
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  hvacConfig->AuxThermometerChannelNo =
      1;  // error, aux has to be different from main
  hvacConfig->AntiFreezeAndOverheatProtectionEnabled = 1;
  hvacConfig->MinOnTimeS = 10;
  hvacConfig->MinOffTimeS = 20;
  hvacConfig->UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ECO, 1600);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_COMFORT, 2200);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BOOST, 2500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION, 1000);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HISTERESIS, 100);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, 1800);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MAX_SETPOINT, 3000);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUX_MIN_SETPOINT, 2000);

  EXPECT_EQ(hvac->handleChannelConfig(&configFromServer),
      SUPLA_CONFIG_RESULT_DATA_ERROR);

  // above set config from server should be ignored beacuse of error in config
  EXPECT_EQ(hvac->getMainThermometerChannelNo(), 0);

  hvac->handleChannelConfigFinished();

  {
    ::testing::InSequence seq;

    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_HVAC),
                                 SUPLA_CONFIG_TYPE_DEFAULT))
        .Times(1)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
        .Times(1)
        .WillRepeatedly(Return(false));

    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_HVAC),
                                 SUPLA_CONFIG_TYPE_DEFAULT))
        .WillOnce([](uint8_t channelNumber,
                     _supla_int_t channelFunction,
                     void *buf,
                     int size,
                     uint8_t configType = SUPLA_CONFIG_TYPE_DEFAULT) {
          TChannelConfig_HVAC expectedData = {
              .MainThermometerChannelNo = 0,
              .AuxThermometerChannelNo = 0,
              .AuxThermometerType =
                  SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET,
              .AntiFreezeAndOverheatProtectionEnabled = 0,
              .AvailableAlgorithms =
                  SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE |
                  SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST,
              .UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE,
              .MinOnTimeS = 0,
              .MinOffTimeS = 0,
              .Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT,
              .TemperatureSetpointChangeSwitchesToManualMode = 1,
              .Temperatures = {}};

          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_ROOM_MIN, 500);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_ROOM_MAX, 4000);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MIN, 20);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MAX, 1000);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures,
              TEMPERATURE_HEAT_COOL_OFFSET_MIN,
              200);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures,
              TEMPERATURE_HEAT_COOL_OFFSET_MAX,
              1000);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_AUX_MIN, 500);
          Supla::Control::HvacBase::setTemperatureInStruct(
              &expectedData.Temperatures, TEMPERATURE_AUX_MAX, 7500);

          EXPECT_EQ(0, memcmp(buf, &expectedData, size));
          return true;
        });
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE))
        .Times(1)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(proto,
                setChannelConfig(0,
                                 SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                                 _,
                                 sizeof(TChannelConfig_WeeklySchedule),
                                 SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE))
        .Times(1)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(cfg, setUInt8(StrEq("0_cfg_chng"), 0))
      .Times(2).WillRepeatedly(Return(true));
  }

  for (int i = 0; i < 10; ++i) {
    hvac->iterateAlways();
    hvac->iterateConnected();
  }

  // send reply from server
  TSDS_SetChannelConfigResult result = {
    .Result = SUPLA_CONFIG_RESULT_FALSE,
    .ConfigType = SUPLA_CONFIG_TYPE_DEFAULT,
    .ChannelNumber = 0
  };

  hvac->handleSetChannelConfigResult(&result);

  // send anothoer set channel config from server - this time it should be
  // applied to the channel
  hvacConfig->AuxThermometerChannelNo = 2;
  EXPECT_EQ(hvac->handleChannelConfig(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);
}

TEST_F(HvacTestsF, checkInitizationForDHW) {
  OutputMock output;
  Supla::Control::HvacBase hvac(&output);

  EXPECT_CALL(output, setOutputValue(0)).Times(1);

  auto *ch = hvac.getChannel();
  EXPECT_EQ(ch->getDefaultFunction(), 0);
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER);

  hvac.onInit();
  // check default function
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER);

  EXPECT_EQ(hvac.getUsedAlgorithm(),
            SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);
}
