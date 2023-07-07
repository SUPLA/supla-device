/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include <gtest/gtest.h>

#include <supla/channel.h>
#include <gmock/gmock.h>
#include <srpc_mock.h>
#include <supla/sensor/electricity_meter.h>


using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Args;
using ::testing::ElementsAre;

TEST(ElectricityMeterTests, SettersAndGetters) {
  Supla::Sensor::ElectricityMeter em;

  int number = em.getChannel()->getChannelNumber();
  EXPECT_EQ(number, 0);

  // phase 0
  EXPECT_EQ(em.getFwdActEnergy(0), 0);
  EXPECT_EQ(em.getRvrActEnergy(0), 0);
  EXPECT_EQ(em.getFwdReactEnergy(0), 0);
  EXPECT_EQ(em.getRvrReactEnergy(0), 0);
  EXPECT_EQ(em.getVoltage(0), 0);
  EXPECT_EQ(em.getCurrent(0), 0);
  EXPECT_EQ(em.getFreq(), 0);
  EXPECT_EQ(em.getPowerActive(0), 0);
  EXPECT_EQ(em.getPowerReactive(0), 0);
  EXPECT_EQ(em.getPowerApparent(0), 0);
  EXPECT_EQ(em.getPowerFactor(0), 0);
  EXPECT_EQ(em.getPhaseAngle(0), 0);

  // phase 1
  EXPECT_EQ(em.getFwdActEnergy(1), 0);
  EXPECT_EQ(em.getRvrActEnergy(1), 0);
  EXPECT_EQ(em.getFwdReactEnergy(1), 0);
  EXPECT_EQ(em.getRvrReactEnergy(1), 0);
  EXPECT_EQ(em.getVoltage(1), 0);
  EXPECT_EQ(em.getCurrent(1), 0);
  EXPECT_EQ(em.getFreq(), 0);
  EXPECT_EQ(em.getPowerActive(1), 0);
  EXPECT_EQ(em.getPowerReactive(1), 0);
  EXPECT_EQ(em.getPowerApparent(1), 0);
  EXPECT_EQ(em.getPowerFactor(1), 0);
  EXPECT_EQ(em.getPhaseAngle(1), 0);

  // phase 2
  EXPECT_EQ(em.getFwdActEnergy(2), 0);
  EXPECT_EQ(em.getRvrActEnergy(2), 0);
  EXPECT_EQ(em.getFwdReactEnergy(2), 0);
  EXPECT_EQ(em.getRvrReactEnergy(2), 0);
  EXPECT_EQ(em.getVoltage(2), 0);
  EXPECT_EQ(em.getCurrent(2), 0);
  EXPECT_EQ(em.getFreq(), 0);
  EXPECT_EQ(em.getPowerActive(2), 0);
  EXPECT_EQ(em.getPowerReactive(2), 0);
  EXPECT_EQ(em.getPowerApparent(2), 0);
  EXPECT_EQ(em.getPowerFactor(2), 0);
  EXPECT_EQ(em.getPhaseAngle(2), 0);

  em.setFwdActEnergy(0, 1);
  em.setRvrActEnergy(0, 2);
  em.setFwdReactEnergy(0, 3);
  em.setRvrReactEnergy(0, 4);
  em.setVoltage(0, 5);
  em.setCurrent(0, 6);
  em.setFreq(7);
  em.setPowerActive(0, 8);
  em.setPowerReactive(0, 9);
  em.setPowerApparent(0, 10);
  em.setPowerFactor(0, 11);
  em.setPhaseAngle(0, 12);

  em.setFwdActEnergy(1, 13);
  em.setRvrActEnergy(1, 14);
  em.setFwdReactEnergy(1, 15);
  em.setRvrReactEnergy(1, 16);
  em.setVoltage(1, 17);
  em.setCurrent(1, 18);
  em.setPowerActive(1, 20);
  em.setPowerReactive(1, 21);
  em.setPowerApparent(1, 22);
  em.setPowerFactor(1, 23);
  em.setPhaseAngle(1, 24);

  em.setFwdActEnergy(2, 25);
  em.setRvrActEnergy(2, 26);
  em.setFwdReactEnergy(2, 27);
  em.setRvrReactEnergy(2, 28);
  em.setVoltage(2, 29);
  em.setCurrent(2, 30);
  em.setPowerActive(2, 32);
  em.setPowerReactive(2, 33);
  em.setPowerApparent(2, 34);
  em.setPowerFactor(2, 35);
  em.setPhaseAngle(2, 36);

  // phase 0
  EXPECT_EQ(em.getFwdActEnergy(0), 1);
  EXPECT_EQ(em.getRvrActEnergy(0), 2);
  EXPECT_EQ(em.getFwdReactEnergy(0), 3);
  EXPECT_EQ(em.getRvrReactEnergy(0), 4);
  EXPECT_EQ(em.getVoltage(0), 5);
  EXPECT_EQ(em.getCurrent(0), 6);
  EXPECT_EQ(em.getFreq(), 7);
  EXPECT_EQ(em.getPowerActive(0), 8);
  EXPECT_EQ(em.getPowerReactive(0), 9);
  EXPECT_EQ(em.getPowerApparent(0), 10);
  EXPECT_EQ(em.getPowerFactor(0), 11);
  EXPECT_EQ(em.getPhaseAngle(0), 12);

  // phase 1
  EXPECT_EQ(em.getFwdActEnergy(1), 13);
  EXPECT_EQ(em.getRvrActEnergy(1), 14);
  EXPECT_EQ(em.getFwdReactEnergy(1), 15);
  EXPECT_EQ(em.getRvrReactEnergy(1), 16);
  EXPECT_EQ(em.getVoltage(1), 17);
  EXPECT_EQ(em.getCurrent(1), 18);
  EXPECT_EQ(em.getFreq(), 7);
  EXPECT_EQ(em.getPowerActive(1), 20);
  EXPECT_EQ(em.getPowerReactive(1), 21);
  EXPECT_EQ(em.getPowerApparent(1), 22);
  EXPECT_EQ(em.getPowerFactor(1), 23);
  EXPECT_EQ(em.getPhaseAngle(1), 24);

  // phase 2
  EXPECT_EQ(em.getFwdActEnergy(2), 25);
  EXPECT_EQ(em.getRvrActEnergy(2), 26);
  EXPECT_EQ(em.getFwdReactEnergy(2), 27);
  EXPECT_EQ(em.getRvrReactEnergy(2), 28);
  EXPECT_EQ(em.getVoltage(2), 29);
  EXPECT_EQ(em.getCurrent(2), 30);
  EXPECT_EQ(em.getFreq(), 7);
  EXPECT_EQ(em.getPowerActive(2), 32);
  EXPECT_EQ(em.getPowerReactive(2), 33);
  EXPECT_EQ(em.getPowerApparent(2), 34);
  EXPECT_EQ(em.getPowerFactor(2), 35);
  EXPECT_EQ(em.getPhaseAngle(2), 36);


  /*
  char emptyArray[SUPLA_CHANNELVALUE_SIZE] = {};

  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, emptyArray, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_FALSE(channel.isUpdateReady());

  char array[SUPLA_CHANNELVALUE_SIZE] = {0, 1, 2, 3, 4, 5, 6, 7};
  channel.setNewValue(array);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, array, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(channel.isUpdateReady());

  channel.clearUpdateReady();
  EXPECT_FALSE(channel.isUpdateReady());

  channel.setNewValue(array);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, array, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_FALSE(channel.isUpdateReady());

  array[4] = 15;
  channel.setNewValue(array);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, array, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(channel.isUpdateReady());

  ASSERT_EQ(sizeof(double), 8);
  double temp = 3.1415;
  channel.setNewValue(temp);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &temp, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();

  char arrayBool[SUPLA_CHANNELVALUE_SIZE] = {};
  arrayBool[0] = true;
  channel.setNewValue(true);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, arrayBool, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();

  channel.setNewValue(false);
  arrayBool[0] = false;
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, arrayBool, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();

  int value = 1234;
  ASSERT_EQ(sizeof(int), 4);
  channel.setNewValue(value);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &value, sizeof(int)));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();
  
  unsigned _supla_int64_t value64 = 124346;
  ASSERT_EQ(sizeof(value64), 8);
  channel.setNewValue(value64);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &value64, sizeof(value64)));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();
  
  double humi = 95.2234123;
  temp = 23.443322;

  int expectedTemp = temp * 1000;
  int expectedHumi = humi * 1000;

  channel.setNewValue(temp, humi);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &expectedTemp, sizeof(expectedTemp)));
  EXPECT_TRUE(0 == memcmp(&(Supla::Channel::reg_dev.channels[number].value[4]), &expectedHumi, sizeof(expectedHumi)));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();

  // RGBW channel setting
  channel.setNewValue(1, 2, 3, 4, 5);
  char rgbwArray[SUPLA_CHANNELVALUE_SIZE] = {5, 4, 3, 2, 1, 0, 0, 0};
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, rgbwArray, SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();

  TElectricityMeter_ExtendedValue_V2 emVal = {};
  TElectricityMeter_Value expectedValue = {};

  emVal.m_count = 1;
  emVal.measured_values |= EM_VAR_FORWARD_ACTIVE_ENERGY;
  emVal.total_forward_active_energy[0] = 1000;
  emVal.total_forward_active_energy[1] = 2000;
  emVal.total_forward_active_energy[2] = 4000;

  expectedValue.total_forward_active_energy = (1000 + 2000 + 4000) / 1000;

  channel.setNewValue(emVal);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &expectedValue, sizeof(expectedValue)));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();

  emVal.measured_values |= EM_VAR_VOLTAGE;
  emVal.m[0].voltage[0] = 10; 
  emVal.m[0].voltage[1] = 0; 
  emVal.m[0].voltage[2] = 0; 

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE1_ON;

  channel.setNewValue(emVal);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &expectedValue, sizeof(expectedValue)));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();

  emVal.m[0].voltage[0] = 0; 
  emVal.m[0].voltage[1] = 20; 
  emVal.m[0].voltage[2] = 0; 

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE2_ON;

  channel.setNewValue(emVal);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &expectedValue, sizeof(expectedValue)));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();


  emVal.m[0].voltage[0] = 0; 
  emVal.m[0].voltage[1] = 0; 
  emVal.m[0].voltage[2] = 300; 

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE3_ON;

  channel.setNewValue(emVal);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &expectedValue, sizeof(expectedValue)));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();


  emVal.m[0].voltage[0] = 10; 
  emVal.m[0].voltage[1] = 0; 
  emVal.m[0].voltage[2] = 540; 

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE1_ON | EM_VALUE_FLAG_PHASE3_ON;

  channel.setNewValue(emVal);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &expectedValue, sizeof(expectedValue)));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();

  emVal.m[0].voltage[0] = 10; 
  emVal.m[0].voltage[1] = 230; 
  emVal.m[0].voltage[2] = 540; 

  expectedValue.flags = 0;
  expectedValue.flags |= EM_VALUE_FLAG_PHASE1_ON | EM_VALUE_FLAG_PHASE3_ON | EM_VALUE_FLAG_PHASE2_ON;

  channel.setNewValue(emVal);
  EXPECT_TRUE(0 == memcmp(Supla::Channel::reg_dev.channels[number].value, &expectedValue, sizeof(expectedValue)));
  EXPECT_TRUE(channel.isUpdateReady());
  channel.clearUpdateReady();
  */
}

TEST(ElectricityMeterTests, ClearMeasurmentsInCaseOfError) {
  Supla::Sensor::ElectricityMeter em;

  int number = em.getChannel()->getChannelNumber();
  EXPECT_EQ(number, 0);

  // phase 0
  EXPECT_EQ(em.getFwdActEnergy(0), 0);
  EXPECT_EQ(em.getRvrActEnergy(0), 0);
  EXPECT_EQ(em.getFwdReactEnergy(0), 0);
  EXPECT_EQ(em.getRvrReactEnergy(0), 0);
  EXPECT_EQ(em.getVoltage(0), 0);
  EXPECT_EQ(em.getCurrent(0), 0);
  EXPECT_EQ(em.getFreq(), 0);
  EXPECT_EQ(em.getPowerActive(0), 0);
  EXPECT_EQ(em.getPowerReactive(0), 0);
  EXPECT_EQ(em.getPowerApparent(0), 0);
  EXPECT_EQ(em.getPowerFactor(0), 0);
  EXPECT_EQ(em.getPhaseAngle(0), 0);

  em.setFwdActEnergy(0, 2000);
  em.setRvrActEnergy(0, 2);
  em.setFwdReactEnergy(0, 3);
  em.setRvrReactEnergy(0, 4);
  em.setVoltage(0, 5);
  em.setCurrent(0, 6);
  em.setFreq(7);
  em.setPowerActive(0, 8);
  em.setPowerReactive(0, 9);
  em.setPowerApparent(0, 10);
  em.setPowerFactor(0, 11);
  em.setPhaseAngle(0, 12);

  EXPECT_EQ(em.getFwdActEnergy(0), 2000);
  EXPECT_EQ(em.getRvrActEnergy(0), 2);
  EXPECT_EQ(em.getFwdReactEnergy(0), 3);
  EXPECT_EQ(em.getRvrReactEnergy(0), 4);
  EXPECT_EQ(em.getVoltage(0), 5);
  EXPECT_EQ(em.getCurrent(0), 6);
  EXPECT_EQ(em.getFreq(), 7);
  EXPECT_EQ(em.getPowerActive(0), 8);
  EXPECT_EQ(em.getPowerReactive(0), 9);
  EXPECT_EQ(em.getPowerApparent(0), 10);
  EXPECT_EQ(em.getPowerFactor(0), 11);
  EXPECT_EQ(em.getPhaseAngle(0), 12);

  char emptyArray[SUPLA_CHANNELVALUE_SIZE] = {};
  auto channel = em.getChannel();

  EXPECT_EQ(0,
            memcmp(Supla::Channel::reg_dev.channels[number].value,
                   emptyArray,
                   SUPLA_CHANNELVALUE_SIZE));
  EXPECT_FALSE(channel->isUpdateReady());

  em.updateChannelValues();

  TElectricityMeter_Value *emValue =
      reinterpret_cast<TElectricityMeter_Value *>(
          Supla::Channel::reg_dev.channels[number].value);

  EXPECT_EQ(emValue->total_forward_active_energy, 2);
  EXPECT_EQ(emValue->flags, EM_VALUE_FLAG_PHASE1_ON);
  EXPECT_TRUE(channel->isUpdateReady());

  auto extValue = em.getChannel()->getExtValue();
  EXPECT_EQ(extValue->type,
            EV_TYPE_ELECTRICITY_METER_MEASUREMENT_V2);
  // data structre under extValue->value is actually smaller than
  // TElectricityMeter_ExtendedValue_V2, however we'll only read in a limit
  // of valid bytes
  TElectricityMeter_ExtendedValue_V2 *emExtValue =
      reinterpret_cast<TElectricityMeter_ExtendedValue_V2 *>(extValue->value);
  EXPECT_EQ(emExtValue->measured_values,
            EM_VAR_FREQ | EM_VAR_VOLTAGE | EM_VAR_CURRENT |
                EM_VAR_POWER_ACTIVE | EM_VAR_POWER_REACTIVE |
                EM_VAR_POWER_APPARENT | EM_VAR_POWER_FACTOR |
                EM_VAR_PHASE_ANGLE | EM_VAR_FORWARD_ACTIVE_ENERGY |
                EM_VAR_REVERSE_ACTIVE_ENERGY | EM_VAR_FORWARD_REACTIVE_ENERGY |
                EM_VAR_REVERSE_REACTIVE_ENERGY);
  EXPECT_EQ(emExtValue->m_count, 1);

  em.resetReadParameters();

  EXPECT_EQ(em.getFwdActEnergy(0), 2000);
  EXPECT_EQ(em.getRvrActEnergy(0), 2);
  EXPECT_EQ(em.getFwdReactEnergy(0), 3);
  EXPECT_EQ(em.getRvrReactEnergy(0), 4);
  EXPECT_EQ(em.getVoltage(0), 0);
  EXPECT_EQ(em.getCurrent(0), 0);
  EXPECT_EQ(em.getFreq(), 0);
  EXPECT_EQ(em.getPowerActive(0), 0);
  EXPECT_EQ(em.getPowerReactive(0), 0);
  EXPECT_EQ(em.getPowerApparent(0), 0);
  EXPECT_EQ(em.getPowerFactor(0), 0);
  EXPECT_EQ(em.getPhaseAngle(0), 0);

  em.updateChannelValues();

  EXPECT_EQ(extValue->type,
            EV_TYPE_ELECTRICITY_METER_MEASUREMENT_V2);

  // only energy registers are kept
  EXPECT_EQ(emExtValue->measured_values,
                EM_VAR_FORWARD_ACTIVE_ENERGY |
                EM_VAR_REVERSE_ACTIVE_ENERGY | EM_VAR_FORWARD_REACTIVE_ENERGY |
                EM_VAR_REVERSE_REACTIVE_ENERGY);
  EXPECT_EQ(emExtValue->m_count, 1);
}
