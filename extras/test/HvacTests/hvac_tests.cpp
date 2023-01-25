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

using ::testing::_;
using ::testing::AtLeast;
using ::testing::StrEq;
// using ::testing::ElementsAreArray;
// using ::testing::Args;
// using ::testing::ElementsAre;

class HvacTestsF : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }
  virtual void TearDown() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }
};

TEST_F(HvacTestsF, BasicChannelSetup) {
  Supla::Control::HvacBase hvac;

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

  EXPECT_TRUE(hvac.isOnOffSupported());
  EXPECT_TRUE(hvac.isHeatingSupported());
  EXPECT_FALSE(hvac.isCoolingSupported());
  EXPECT_FALSE(hvac.isAutoSupported());
  EXPECT_FALSE(hvac.isFanSupported());
  EXPECT_FALSE(hvac.isDrySupported());

  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);

  // check setters for supported modes
  hvac.setCoolingSupported(true);
  EXPECT_TRUE(hvac.isCoolingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);
  // check fan
  hvac.setFanSupported(true);
  EXPECT_TRUE(hvac.isFanSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN);
  // check dry
  hvac.setDrySupported(true);
  EXPECT_TRUE(hvac.isDrySupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY);
  // check auto
  hvac.setAutoSupported(true);
  EXPECT_TRUE(hvac.isAutoSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check onoff
  hvac.setOnOffSupported(false);
  EXPECT_FALSE(hvac.isOnOffSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check heating
  hvac.setHeatingSupported(false);
  EXPECT_FALSE(hvac.isHeatingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check cooling
  hvac.setCoolingSupported(false);
  EXPECT_FALSE(hvac.isCoolingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_FAN |
                                   SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check fan
  hvac.setFanSupported(false);
  EXPECT_FALSE(hvac.isFanSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_DRY |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check dry
  hvac.setDrySupported(false);
  EXPECT_FALSE(hvac.isDrySupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_AUTO);

  // check auto
  hvac.setAutoSupported(false);
  EXPECT_FALSE(hvac.isAutoSupported());
  EXPECT_EQ(ch->getFuncList(), 0);

  // check onoff
  hvac.setOnOffSupported(true);
  EXPECT_TRUE(hvac.isOnOffSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);

  // check heating
  hvac.setHeatingSupported(true);
  EXPECT_TRUE(hvac.isHeatingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);


  // check if set auto will also set cool and heat
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  EXPECT_FALSE(hvac.isCoolingSupported());
  EXPECT_FALSE(hvac.isHeatingSupported());
  EXPECT_FALSE(hvac.isAutoSupported());

  hvac.setAutoSupported(true);
  EXPECT_TRUE(hvac.isAutoSupported());
  EXPECT_TRUE(hvac.isCoolingSupported());
  EXPECT_TRUE(hvac.isHeatingSupported());
  EXPECT_EQ(ch->getFuncList(), SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                                   SUPLA_HVAC_CAP_FLAG_MODE_COOL |
                                   SUPLA_HVAC_CAP_FLAG_MODE_ONOFF |
                                   SUPLA_HVAC_CAP_FLAG_MODE_AUTO);
}

TEST_F(HvacTestsF, checkDefaultFunctionInitizedByOnInit) {
  Supla::Control::HvacBase hvac;
  auto *ch = hvac.getChannel();
  EXPECT_EQ(ch->getDefaultFunction(), 0);

  hvac.onInit();
  // check default function
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);

  // check auto
  hvac.setAutoSupported(true);
  // init doesn't change default function when it was previously set
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);


  // clear default function and check auto again
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO);

  // check cool only
  hvac.setCoolingSupported(true);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL);

  // check heat only
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(true);
  hvac.setAutoSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);

  // check dry
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  hvac.setDrySupported(true);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_DRYER);

  // check fan
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  hvac.setDrySupported(false);
  hvac.setFanSupported(true);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_FAN);

  // check with all options enabled
  hvac.setOnOffSupported(true);
  hvac.setCoolingSupported(true);
  hvac.setHeatingSupported(true);
  hvac.setAutoSupported(true);
  hvac.setDrySupported(true);
  hvac.setFanSupported(true);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO);

  // check will all options disabled
  hvac.setOnOffSupported(false);
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  hvac.setDrySupported(false);
  hvac.setFanSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), 0);

  // only onoff
  hvac.setOnOffSupported(true);
  hvac.setCoolingSupported(false);
  hvac.setHeatingSupported(false);
  hvac.setAutoSupported(false);
  hvac.setDrySupported(false);
  hvac.setFanSupported(false);
  ch->setDefault(0);
  hvac.onInit();
  EXPECT_EQ(ch->getDefaultFunction(), 0);
}

TEST_F(HvacTestsF, handleChannelConfigTestsOnEmptyElement) {
  Supla::Control::HvacBase hvac;
  Supla::Sensor::Thermometer t1;
  Supla::Sensor::ThermHygroMeter t2;

  ASSERT_EQ(hvac.getChannelNumber(), 0);
  ASSERT_EQ(t1.getChannelNumber(), 1);
  ASSERT_EQ(t2.getChannelNumber(), 2);

  // init min max ranges for tempreatures setting and check again setters
  // for temperatures
  hvac.setTemperatureRoomMin(500);           // 5 degrees
  hvac.setTemperatureRoomMax(5000);          // 50 degrees
  hvac.setTemperatureHisteresisMin(20);      // 0.2 degree
  hvac.setTemperatureHisteresisMax(1000);    // 10 degree
  hvac.setTemperatureAutoOffsetMin(200);     // 2 degrees
  hvac.setTemperatureAutoOffsetMax(1000);    // 10 degrees
  hvac.setTemperatureHeaterCoolerMin(500);   // 5 degrees
  hvac.setTemperatureHeaterCoolerMax(7500);  // 75 degrees

  EXPECT_EQ(hvac.handleChannelConfig(nullptr), SUPLA_CONFIG_RESULT_DATA_ERROR);

  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CHANNEL_CONFIG_TYPE_WEEKLY_SCHEDULE;

  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED);


  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED);

  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  configFromServer.ConfigSize = sizeof(TSD_ChannelConfig_HVAC);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  TSD_ChannelConfig_HVAC *hvacConfig =
      reinterpret_cast<TSD_ChannelConfig_HVAC *>(&configFromServer.Config);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->HeaterCoolerThermometerType =
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->MainThermometerChannelNo = 0;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->MainThermometerChannelNo = 2;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->MainThermometerChannelNo = 3;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->HeaterCoolerThermometerType =
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_FLOOR;
  hvacConfig->HeaterCoolerThermometerChannelNo = 0;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->HeaterCoolerThermometerType =
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_FLOOR;
  hvacConfig->HeaterCoolerThermometerChannelNo = 1;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->HeaterCoolerThermometerType =
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_FLOOR;
  hvacConfig->HeaterCoolerThermometerChannelNo = 3;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->HeaterCoolerThermometerType =
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_FLOOR;
  hvacConfig->HeaterCoolerThermometerChannelNo = 2;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->MainThermometerChannelNo = 2;
  hvacConfig->HeaterCoolerThermometerType =
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_FLOOR;
  hvacConfig->HeaterCoolerThermometerChannelNo = 1;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  // algorithm caps in value received from server is not used
  // It is readonly value stored on device
  hvacConfig->AlgorithmCaps = SUPLA_HVAC_ALGORITHM_ON_OFF;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvac.addAlgorithmCap(SUPLA_HVAC_ALGORITHM_ON_OFF);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  hvacConfig->UsedAlgorithm = 15;
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  hvacConfig->UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF;
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
      &hvacConfig->Temperatures, TEMPERATURE_BOOST, 5000);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_FREEZE_PROTECTION
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION, 500);
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

  // TEMPERATURE_AUTO_OFFSET
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUTO_OFFSET, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUTO_OFFSET, 300);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_BELOW_ALARM
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, 800);
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

  // TEMPERATURE_HEATER_COOLER_MAX_SETPOINT
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEATER_COOLER_MAX_SETPOINT, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEATER_COOLER_MAX_SETPOINT, 2000);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_TRUE);

  // TEMPERATURE_HEATER_COOLER_MIN_SETPOINT
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEATER_COOLER_MIN_SETPOINT, 0);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEATER_COOLER_MIN_SETPOINT, 2000);
  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
            SUPLA_CONFIG_RESULT_DATA_ERROR);

  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEATER_COOLER_MIN_SETPOINT, 1000);
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

  EXPECT_EQ(hvac.getTemperatureAutoOffset(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_AUTO_OFFSET));

  EXPECT_EQ(hvac.getTemperatureBelowAlarm(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM));

  EXPECT_EQ(hvac.getTemperatureAboveAlarm(),
            Supla::Control::HvacBase::getTemperatureFromStruct(
                &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM));

  EXPECT_EQ(
      hvac.getTemperatureHeaterCoolerMaxSetpoint(),
      Supla::Control::HvacBase::getTemperatureFromStruct(
          &hvacConfig->Temperatures, TEMPERATURE_HEATER_COOLER_MAX_SETPOINT));

  EXPECT_EQ(
      hvac.getTemperatureHeaterCoolerMinSetpoint(),
      Supla::Control::HvacBase::getTemperatureFromStruct(
          &hvacConfig->Temperatures, TEMPERATURE_HEATER_COOLER_MIN_SETPOINT));

  EXPECT_EQ(hvac.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);
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

  EXPECT_FALSE(hvac.setTemperatureHisteresis(100));
  EXPECT_FALSE(hvac.setTemperatureHisteresis(-100));
  EXPECT_FALSE(hvac.setTemperatureHisteresis(0));

  EXPECT_FALSE(hvac.setTemperatureAutoOffset(100));
  EXPECT_FALSE(hvac.setTemperatureAutoOffset(-100));
  EXPECT_FALSE(hvac.setTemperatureAutoOffset(0));

  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMinSetpoint(100));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMinSetpoint(-100));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMinSetpoint(0));

  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMaxSetpoint(100));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMaxSetpoint(-100));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMaxSetpoint(0));

  // init min max ranges for tempreatures setting and check again setters
  // for temperatures
  hvac.setTemperatureRoomMin(500);           // 5 degrees
  hvac.setTemperatureRoomMax(5000);          // 50 degrees
  hvac.setTemperatureHisteresisMin(20);      // 0.2 degree
  hvac.setTemperatureHisteresisMax(1000);    // 10 degree
  hvac.setTemperatureAutoOffsetMin(200);     // 2 degrees
  hvac.setTemperatureAutoOffsetMax(1000);    // 10 degrees
  hvac.setTemperatureHeaterCoolerMin(500);   // 5 degrees
  hvac.setTemperatureHeaterCoolerMax(7500);  // 75 degrees

  EXPECT_EQ(hvac.getTemperatureRoomMin(), 500);
  EXPECT_EQ(hvac.getTemperatureRoomMax(), 5000);
  EXPECT_EQ(hvac.getTemperatureHisteresisMin(), 20);
  EXPECT_EQ(hvac.getTemperatureHisteresisMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureAutoOffsetMin(), 200);
  EXPECT_EQ(hvac.getTemperatureAutoOffsetMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureHeaterCoolerMin(), 500);
  EXPECT_EQ(hvac.getTemperatureHeaterCoolerMax(), 7500);

  hvac.setTemperatureRoomMin(600);
  EXPECT_EQ(hvac.getTemperatureRoomMin(), 600);
  EXPECT_EQ(hvac.getTemperatureRoomMax(), 5000);
  EXPECT_EQ(hvac.getTemperatureHisteresisMin(), 20);
  EXPECT_EQ(hvac.getTemperatureHisteresisMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureAutoOffsetMin(), 200);
  EXPECT_EQ(hvac.getTemperatureAutoOffsetMax(), 1000);
  EXPECT_EQ(hvac.getTemperatureHeaterCoolerMin(), 500);
  EXPECT_EQ(hvac.getTemperatureHeaterCoolerMax(), 7500);

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

  EXPECT_FALSE(hvac.setTemperatureAutoOffset(2000));
  EXPECT_FALSE(hvac.setTemperatureAutoOffset(-1000));
  EXPECT_FALSE(hvac.setTemperatureAutoOffset(0));
  EXPECT_FALSE(hvac.setTemperatureAutoOffset(10));
  EXPECT_TRUE(hvac.setTemperatureAutoOffset(200));
  EXPECT_TRUE(hvac.setTemperatureAutoOffset(1000));
  EXPECT_TRUE(hvac.setTemperatureAutoOffset(500));
  EXPECT_FALSE(hvac.setTemperatureAutoOffset(10));
  EXPECT_EQ(hvac.getTemperatureAutoOffset(), 500);

  EXPECT_TRUE(hvac.setTemperatureHeaterCoolerMinSetpoint(2000));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMinSetpoint(-1000));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMinSetpoint(0));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMinSetpoint(10));
  EXPECT_TRUE(hvac.setTemperatureHeaterCoolerMinSetpoint(500));
  EXPECT_TRUE(hvac.setTemperatureHeaterCoolerMinSetpoint(1000));
  EXPECT_TRUE(hvac.setTemperatureHeaterCoolerMinSetpoint(5000));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMinSetpoint(10));
  EXPECT_EQ(hvac.getTemperatureHeaterCoolerMinSetpoint(), 5000);

  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMaxSetpoint(2000));
  EXPECT_TRUE(hvac.setTemperatureHeaterCoolerMaxSetpoint(6000));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMaxSetpoint(-1000));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMaxSetpoint(0));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMaxSetpoint(10));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMaxSetpoint(501));

  // change min setpoint
  EXPECT_TRUE(hvac.setTemperatureHeaterCoolerMinSetpoint(500));
  // ang check max setpoint
  EXPECT_TRUE(hvac.setTemperatureHeaterCoolerMaxSetpoint(501));
  EXPECT_TRUE(hvac.setTemperatureHeaterCoolerMaxSetpoint(1000));
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMaxSetpoint(10));
  EXPECT_EQ(hvac.getTemperatureHeaterCoolerMaxSetpoint(), 1000);

  // change min setpoint (higher than max setpoint)
  EXPECT_FALSE(hvac.setTemperatureHeaterCoolerMinSetpoint(1500));

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

  EXPECT_EQ(hvac.getMinOnTimeS(), 0);
  EXPECT_EQ(hvac.getMinOffTimeS(), 0);
  EXPECT_EQ(hvac.getUsedAlgorithm(), SUPLA_HVAC_ALGORITHM_NOT_SET);
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 0);
  EXPECT_EQ(hvac.getHeaterCoolerThermometerChannelNo(), 0);
  EXPECT_EQ(hvac.getHeaterCoolerThermometerType(),
            SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET);

  EXPECT_FALSE(hvac.isAntiFreezeAndHeatProtectionEnabled());

  EXPECT_EQ(hvac.getMinOnTimeS(), 0);
  EXPECT_EQ(hvac.getMinOffTimeS(), 0);

  EXPECT_FALSE(hvac.setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF));
  EXPECT_EQ(hvac.getUsedAlgorithm(), SUPLA_HVAC_ALGORITHM_NOT_SET);
  hvac.addAlgorithmCap(SUPLA_HVAC_ALGORITHM_ON_OFF);
  EXPECT_TRUE(hvac.setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF));
  EXPECT_EQ(hvac.getUsedAlgorithm(), SUPLA_HVAC_ALGORITHM_ON_OFF);
  EXPECT_FALSE(hvac.setUsedAlgorithm(999));
  EXPECT_EQ(hvac.getUsedAlgorithm(), SUPLA_HVAC_ALGORITHM_ON_OFF);

  EXPECT_TRUE(hvac.setMinOnTimeS(10));
  EXPECT_EQ(hvac.getMinOnTimeS(), 10);
  EXPECT_FALSE(hvac.setMinOnTimeS(10000));
  EXPECT_EQ(hvac.getMinOnTimeS(), 10);

  EXPECT_TRUE(hvac.setMinOffTimeS(20));
  EXPECT_EQ(hvac.getMinOffTimeS(), 20);
  EXPECT_FALSE(hvac.setMinOffTimeS(10000));
  EXPECT_EQ(hvac.getMinOffTimeS(), 20);

  hvac.setAntiFreezeAndHeatProtectionEnabled(true);
  EXPECT_TRUE(hvac.isAntiFreezeAndHeatProtectionEnabled());
  hvac.setAntiFreezeAndHeatProtectionEnabled(false);
  EXPECT_FALSE(hvac.isAntiFreezeAndHeatProtectionEnabled());

  EXPECT_FALSE(hvac.setMainThermometerChannelNo(0));
  EXPECT_FALSE(hvac.setHeaterCoolerThermometerChannelNo(10));
  EXPECT_EQ(hvac.getHeaterCoolerThermometerType(),
            SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET);

  EXPECT_TRUE(hvac.setMainThermometerChannelNo(1));
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 1);
  EXPECT_FALSE(hvac.setHeaterCoolerThermometerChannelNo(1));
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 1);
  EXPECT_EQ(hvac.getHeaterCoolerThermometerChannelNo(), 0);
  EXPECT_EQ(hvac.getHeaterCoolerThermometerType(),
            SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET);

  EXPECT_TRUE(hvac.setHeaterCoolerThermometerChannelNo(2));
  EXPECT_EQ(hvac.getHeaterCoolerThermometerChannelNo(), 2);
  EXPECT_EQ(hvac.getHeaterCoolerThermometerType(),
            SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_DISALBED);
  hvac.setHeaterCoolerThermometerType(
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_WATER);
  EXPECT_EQ(hvac.getHeaterCoolerThermometerType(),
            SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_WATER);

  // setting to the same channel number as hvac channel should remove
  // secondary thermometer
  EXPECT_TRUE(hvac.setHeaterCoolerThermometerChannelNo(0));
  EXPECT_EQ(hvac.getHeaterCoolerThermometerType(),
            SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET);
}

TEST_F(HvacTestsF, handleChannelConfigWithConfigStorage) {
  ConfigMock cfg;
  Supla::Control::HvacBase hvac;
  Supla::Sensor::Thermometer t1;
  Supla::Sensor::ThermHygroMeter t2;

  ASSERT_EQ(hvac.getChannelNumber(), 0);
  ASSERT_EQ(t1.getChannelNumber(), 1);
  ASSERT_EQ(t2.getChannelNumber(), 2);

  // init min max ranges for tempreatures setting and check again setters
  // for temperatures
  hvac.setTemperatureRoomMin(500);           // 5 degrees
  hvac.setTemperatureRoomMax(5000);          // 50 degrees
  hvac.setTemperatureHisteresisMin(20);      // 0.2 degree
  hvac.setTemperatureHisteresisMax(1000);    // 10 degree
  hvac.setTemperatureAutoOffsetMin(200);     // 2 degrees
  hvac.setTemperatureAutoOffsetMax(1000);    // 10 degrees
  hvac.setTemperatureHeaterCoolerMin(500);   // 5 degrees
  hvac.setTemperatureHeaterCoolerMax(7500);  // 75 degrees
  hvac.addAlgorithmCap(SUPLA_HVAC_ALGORITHM_ON_OFF);

  TSD_ChannelConfig configFromServer = {};
  configFromServer.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  configFromServer.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT;
  configFromServer.ConfigSize = sizeof(TSD_ChannelConfig_HVAC);
  TSD_ChannelConfig_HVAC *hvacConfig =
      reinterpret_cast<TSD_ChannelConfig_HVAC *>(&configFromServer.Config);
  hvacConfig->MainThermometerChannelNo = 1;
  hvacConfig->HeaterCoolerThermometerType =
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_FLOOR;
  hvacConfig->HeaterCoolerThermometerChannelNo = 2;
  hvacConfig->EnableAntiFreezeAndOverheatProtection = 1;
  hvacConfig->MinOnTimeS = 10;
  hvacConfig->MinOffTimeS = 20;
  hvacConfig->UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF;
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ECO, 1600);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_COMFORT, 2200);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BOOST, 2500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_FREEZE_PROTECTION, 500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HISTERESIS, 100);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_AUTO_OFFSET, 300);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, 800);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEATER_COOLER_MAX_SETPOINT, 3000);
  Supla::Control::HvacBase::setTemperatureInStruct(
      &hvacConfig->Temperatures, TEMPERATURE_HEATER_COOLER_MIN_SETPOINT, 2000);

  EXPECT_CALL(cfg, saveWithDelay(_)).Times(AtLeast(1));
  EXPECT_CALL(cfg,
              setInt32(StrEq("0_fnc"), SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT))
      .Times(1);

  EXPECT_CALL(cfg,
              setBlob(StrEq("0_hvac_cfg"), _, sizeof(TSD_ChannelConfig_HVAC)))
      .WillOnce([](const char *key, const char *buf, int size) {
        TSD_ChannelConfig_HVAC expectedData = {
            .MainThermometerChannelNo = 1,
            .HeaterCoolerThermometerChannelNo = 2,
            .HeaterCoolerThermometerType =
                SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_FLOOR,
            .EnableAntiFreezeAndOverheatProtection = 1,
            .AlgorithmCaps = SUPLA_HVAC_ALGORITHM_ON_OFF,
            .UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF,
            .MinOnTimeS = 10,
            .MinOffTimeS = 20,
            .Temperatures = {}};

            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ECO, 1600);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_COMFORT, 2200);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_BOOST, 2500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_FREEZE_PROTECTION, 500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HEAT_PROTECTION, 3400);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS, 100);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_AUTO_OFFSET, 300);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_BELOW_ALARM, 800);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ABOVE_ALARM, 3500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_HEATER_COOLER_MAX_SETPOINT,
                3000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_HEATER_COOLER_MIN_SETPOINT,
                2000);

            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ROOM_MIN, 500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_ROOM_MAX, 5000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MIN, 20);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HISTERESIS_MAX, 1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_AUTO_OFFSET_MIN, 200);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_AUTO_OFFSET_MAX, 1000);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures, TEMPERATURE_HEATER_COOLER_MIN, 500);
            Supla::Control::HvacBase::setTemperatureInStruct(
                &expectedData.Temperatures,
                TEMPERATURE_HEATER_COOLER_MAX,
                7500);

            EXPECT_EQ(0, memcmp(buf, &expectedData, size));
            return 1;
      });

  EXPECT_EQ(hvac.handleChannelConfig(&configFromServer),
      SUPLA_CONFIG_RESULT_TRUE);


}

