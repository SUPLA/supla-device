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
#include <simple_time.h>
#include <supla/device/register_device.h>

class EMForTest : public Supla::Sensor::ElectricityMeter {
 public:
  TElectricityMeter_ExtendedValue_V2 *getEmValue() {
    return &emValue;
  }
};

TEST(ElectricityMeterTests, SettersAndGetters) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Sensor::ElectricityMeter em;

  int number = em.getChannel()->getChannelNumber();
  EXPECT_EQ(number, 0);

  // common
  EXPECT_EQ(em.getFwdBalancedActEnergy(), 0);
  EXPECT_EQ(em.getRvrBalancedActEnergy(), 0);
  EXPECT_EQ(em.getFreq(), 0);

  // phase 0
  EXPECT_EQ(em.getFwdActEnergy(0), 0);
  EXPECT_EQ(em.getRvrActEnergy(0), 0);
  EXPECT_EQ(em.getFwdReactEnergy(0), 0);
  EXPECT_EQ(em.getRvrReactEnergy(0), 0);
  EXPECT_EQ(em.getVoltage(0), 0);
  EXPECT_EQ(em.getCurrent(0), 0);
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
  EXPECT_EQ(em.getPowerActive(2), 0);
  EXPECT_EQ(em.getPowerReactive(2), 0);
  EXPECT_EQ(em.getPowerApparent(2), 0);
  EXPECT_EQ(em.getPowerFactor(2), 0);
  EXPECT_EQ(em.getPhaseAngle(2), 0);

  em.setFwdBalancedEnergy(101);
  em.setRvrBalancedEnergy(102);

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

  // common
  EXPECT_EQ(em.getFwdBalancedActEnergy(), 101);
  EXPECT_EQ(em.getRvrBalancedActEnergy(), 102);
  EXPECT_EQ(em.getFreq(), 7);

  // phase 0
  EXPECT_EQ(em.getFwdActEnergy(0), 1);
  EXPECT_EQ(em.getRvrActEnergy(0), 2);
  EXPECT_EQ(em.getFwdReactEnergy(0), 3);
  EXPECT_EQ(em.getRvrReactEnergy(0), 4);
  EXPECT_EQ(em.getVoltage(0), 5);
  EXPECT_EQ(em.getCurrent(0), 6);
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
  EXPECT_EQ(em.getPowerActive(2), 32);
  EXPECT_EQ(em.getPowerReactive(2), 33);
  EXPECT_EQ(em.getPowerApparent(2), 34);
  EXPECT_EQ(em.getPowerFactor(2), 35);
  EXPECT_EQ(em.getPhaseAngle(2), 36);
}

TEST(ElectricityMeterTests, ClearMeasurmentsInCaseOfError) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
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
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                   emptyArray,
                   SUPLA_CHANNELVALUE_SIZE));
  EXPECT_FALSE(channel->isUpdateReady());

  em.updateChannelValues();

  TElectricityMeter_Value *emValue =
      reinterpret_cast<TElectricityMeter_Value *>(
          Supla::RegisterDevice::getChannelValuePtr(number));

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

  // measured values shouldn't be cleared
  EXPECT_EQ(emExtValue->measured_values,
            EM_VAR_FREQ | EM_VAR_VOLTAGE | EM_VAR_CURRENT |
                EM_VAR_POWER_ACTIVE | EM_VAR_POWER_REACTIVE |
                EM_VAR_POWER_APPARENT | EM_VAR_POWER_FACTOR |
                EM_VAR_PHASE_ANGLE | EM_VAR_FORWARD_ACTIVE_ENERGY |
                EM_VAR_REVERSE_ACTIVE_ENERGY | EM_VAR_FORWARD_REACTIVE_ENERGY |
                EM_VAR_REVERSE_REACTIVE_ENERGY);
  // m_count is set to 0
  EXPECT_EQ(emExtValue->m_count, 0);
}

TEST(ElectricityMeterTests, ParametersOverBasicRange) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  EMForTest em;

  int number = em.getChannel()->getChannelNumber();
  EXPECT_EQ(number, 0);

  // common
  EXPECT_EQ(em.getFwdBalancedActEnergy(), 0);
  EXPECT_EQ(em.getRvrBalancedActEnergy(), 0);
  EXPECT_EQ(em.getFreq(), 0);

  EXPECT_EQ(em.getCurrent(0), 0);
  EXPECT_EQ(em.getCurrent(1), 0);
  EXPECT_EQ(em.getCurrent(2), 0);

  EXPECT_EQ(em.getPowerActive(0), 0);
  EXPECT_EQ(em.getPowerActive(1), 0);
  EXPECT_EQ(em.getPowerActive(2), 0);

  EXPECT_EQ(em.getPowerReactive(0), 0);
  EXPECT_EQ(em.getPowerReactive(1), 0);
  EXPECT_EQ(em.getPowerReactive(2), 0);

  EXPECT_EQ(em.getPowerApparent(0), 0);
  EXPECT_EQ(em.getPowerApparent(1), 0);
  EXPECT_EQ(em.getPowerApparent(2), 0);

  em.setCurrent(0, 60000);
  em.setCurrent(1, 70000);
  em.setCurrent(2, 655350);

// int 32 power_active[3];  * 0.00001W (0.01kW WHEN EM_VAR_POWER_ACTIVE_KW)
  em.setPowerReactive(0, 1000000);
  em.setPowerReactive(1, 2000000);
  em.setPowerReactive(2, 30000000000);

  em.setPowerApparent(0, 40000000000);
  em.setPowerApparent(1, 5000000);
  em.setPowerApparent(2, 6000000);

  em.setPowerActive(0, 7000000);
  em.setPowerActive(1, 8000000);
  em.setPowerActive(2, 9000000);

  EXPECT_EQ(em.getEmValue()->m[0].current[0], 0);
  EXPECT_EQ(em.getEmValue()->m[0].current[1], 0);
  EXPECT_EQ(em.getEmValue()->m[0].current[2], 0);

  em.updateChannelValues();

  EXPECT_EQ(em.getCurrent(0), 60000);
  EXPECT_EQ(em.getCurrent(1), 70000);
  EXPECT_EQ(em.getCurrent(2), 655350);

  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_CURRENT, 0);
  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_CURRENT_OVER_65A,
            EM_VAR_CURRENT_OVER_65A);
  EXPECT_EQ(em.getEmValue()->m[0].current[0], 6000);
  EXPECT_EQ(em.getEmValue()->m[0].current[1], 7000);
  EXPECT_EQ(em.getEmValue()->m[0].current[2], 65535);

  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_ACTIVE,
            EM_VAR_POWER_ACTIVE);
  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_ACTIVE_KW, 0);
  EXPECT_EQ(em.getEmValue()->m[0].power_active[0], 7000000);
  EXPECT_EQ(em.getEmValue()->m[0].power_active[1], 8000000);
  EXPECT_EQ(em.getEmValue()->m[0].power_active[2], 9000000);

  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_REACTIVE, 0);
  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_REACTIVE_KVAR,
            EM_VAR_POWER_REACTIVE_KVAR);
  EXPECT_EQ(em.getEmValue()->m[0].power_reactive[0], 1000);
  EXPECT_EQ(em.getEmValue()->m[0].power_reactive[1], 2000);
  EXPECT_EQ(em.getEmValue()->m[0].power_reactive[2], 30000000);

  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_APPARENT, 0);
  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_APPARENT_KVA,
            EM_VAR_POWER_APPARENT_KVA);

  EXPECT_EQ(em.getEmValue()->m[0].power_apparent[0], 40000000);
  EXPECT_EQ(em.getEmValue()->m[0].power_apparent[1], 5000);
  EXPECT_EQ(em.getEmValue()->m[0].power_apparent[2], 6000);

  em.setCurrent(1, 1000);
  em.setCurrent(2, 2000);
  em.setPowerActive(1, 5000000000);
  em.setPowerReactive(2, 3000000);
  em.setPowerApparent(0, 4000000);

  em.updateChannelValues();

  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_CURRENT, EM_VAR_CURRENT);
  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_CURRENT_OVER_65A, 0);
  EXPECT_EQ(em.getEmValue()->m[0].current[0], 60000);
  EXPECT_EQ(em.getEmValue()->m[0].current[1], 1000);
  EXPECT_EQ(em.getEmValue()->m[0].current[2], 2000);

  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_ACTIVE, 0);
  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_ACTIVE_KW,
            EM_VAR_POWER_ACTIVE_KW);
  EXPECT_EQ(em.getEmValue()->m[0].power_active[0], 7000);
  EXPECT_EQ(em.getEmValue()->m[0].power_active[1], 5000000);
  EXPECT_EQ(em.getEmValue()->m[0].power_active[2], 9000);

  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_REACTIVE,
            EM_VAR_POWER_REACTIVE);
  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_REACTIVE_KVAR,
            0);

  EXPECT_EQ(em.getEmValue()->m[0].power_reactive[0], 1000000);
  EXPECT_EQ(em.getEmValue()->m[0].power_reactive[1], 2000000);
  EXPECT_EQ(em.getEmValue()->m[0].power_reactive[2], 3000000);

  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_APPARENT,
            EM_VAR_POWER_APPARENT);
  EXPECT_EQ(em.getEmValue()->measured_values & EM_VAR_POWER_APPARENT_KVA,
            0);

  EXPECT_EQ(em.getEmValue()->m[0].power_apparent[0], 4000000);
  EXPECT_EQ(em.getEmValue()->m[0].power_apparent[1], 5000000);
  EXPECT_EQ(em.getEmValue()->m[0].power_apparent[2], 6000000);
}
