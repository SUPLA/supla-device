/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.
*/

#include <gtest/gtest.h>
#include <output_mock.h>
#include <simple_time.h>
#include <clock_stub.h>
#include <supla/control/hvac_base.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/virtual_thermometer.h>

class HvacControlFlowF : public ::testing::Test {
 protected:
  OutputSimulator output;
  SimpleTime time;
  ClockStub clock;
  Supla::Control::HvacBase *hvac = nullptr;
  Supla::Sensor::VirtualThermometer *mainTemperature = nullptr;
  Supla::Sensor::VirtualThermometer *auxTemperature = nullptr;
  Supla::Sensor::VirtualBinary *forcedOffSensor = nullptr;

  void SetUp() override {
    Supla::Channel::resetToDefaults();

    hvac = new Supla::Control::HvacBase(&output);
    mainTemperature = new Supla::Sensor::VirtualThermometer();
    auxTemperature = new Supla::Sensor::VirtualThermometer();
    forcedOffSensor = new Supla::Sensor::VirtualBinary();

    hvac->getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
    hvac->initDefaultConfig();
    hvac->setSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
    hvac->setMainThermometerChannelNo(mainTemperature->getChannelNumber());
    hvac->setAuxThermometerChannelNo(auxTemperature->getChannelNumber());
    hvac->setAuxThermometerType(
        SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
    hvac->setBinarySensorChannelNo(forcedOffSensor->getChannelNumber());
    hvac->setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);
    hvac->setTemperatureHisteresis(40);
    hvac->setTemperatureFreezeProtection(500);
    hvac->setTemperatureHeatProtection(3200);
    hvac->setTemperatureAuxMinSetpoint(1500);
    hvac->setTemperatureAuxMaxSetpoint(7500);
    hvac->setAuxMinMaxSetpointEnabled(true);
    hvac->setMinOnTimeS(5);
    hvac->setMinOffTimeS(3);
    hvac->getChannel()->setHvacSetpointTemperatureHeat(2100);
    hvac->getChannel()->setHvacSetpointTemperatureCool(2500);

    mainTemperature->setValue(18.0);
    mainTemperature->setRefreshIntervalMs(1);
    auxTemperature->setValue(20.0);
    auxTemperature->setRefreshIntervalMs(1);
    // A true binary input means that the user-level forced-off condition is
    // inactive. Clearing it later activates forced-off.
    forcedOffSensor->set();
    mainTemperature->onInit();
    auxTemperature->onInit();
    forcedOffSensor->onInit();
    forcedOffSensor->getChannel()->setStateOnline();
    hvac->onInit();
  }

  void TearDown() override {
    delete hvac;
    delete mainTemperature;
    delete auxTemperature;
    delete forcedOffSensor;
    Supla::Channel::resetToDefaults();
  }

  void iterateOnce(uint32_t advanceMs = 1001) {
    time.advance(advanceMs);
    mainTemperature->iterateAlways();
    auxTemperature->iterateAlways();
    forcedOffSensor->iterateAlways();
    hvac->iterateAlways();
  }

  void settleMode(int mode) {
    hvac->setTargetMode(mode);
    for (int i = 0; i < 6; ++i) {
      iterateOnce();
    }
  }

  void useCooling() {
    hvac->setSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);
    mainTemperature->setValue(30.0);
    // Consume the runtime subfunction transition before selecting the mode;
    // the production state machine clears the output when this changes.
    iterateOnce();
  }
};

TEST_F(HvacControlFlowF, forcedOffAfterAuxMinStillRetriesHeatShutdown) {
  hvac->setMinOnTimeS(10);
  settleMode(SUPLA_HVAC_MODE_HEAT);
  ASSERT_EQ(output.getOutputValue(), 1);

  auxTemperature->setValue(10.0);
  iterateOnce();
  forcedOffSensor->clear();
  iterateOnce();

  EXPECT_TRUE(hvac->isHvacFlagForcedOffBySensor());
  EXPECT_EQ(output.getOutputValue(), 1);

  for (int i = 0; i < 10; ++i) {
    iterateOnce();
  }

  EXPECT_TRUE(hvac->isHvacFlagForcedOffBySensor());
  EXPECT_EQ(output.getOutputValue(), 0);
}

TEST_F(HvacControlFlowF, forcedOffAfterAuxMaxStillRetriesCoolShutdown) {
  useCooling();
  hvac->setMinOnTimeS(10);
  settleMode(SUPLA_HVAC_MODE_COOL);
  ASSERT_EQ(output.getOutputValue(), -1);

  auxTemperature->setValue(80.0);
  iterateOnce();
  forcedOffSensor->clear();
  iterateOnce();

  EXPECT_TRUE(hvac->isHvacFlagForcedOffBySensor());
  EXPECT_EQ(output.getOutputValue(), -1);

  for (int i = 0; i < 10; ++i) {
    iterateOnce();
  }

  EXPECT_TRUE(hvac->isHvacFlagForcedOffBySensor());
  EXPECT_EQ(output.getOutputValue(), 0);
}

TEST_F(HvacControlFlowF, offRetriesWithAuxMinAfterMinOnTime) {
  hvac->setMinOnTimeS(20);
  settleMode(SUPLA_HVAC_MODE_HEAT);
  ASSERT_EQ(output.getOutputValue(), 1);

  auxTemperature->setValue(10.0);
  iterateOnce();
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  ASSERT_EQ(output.getOutputValue(), 1);

  for (int i = 0; i < 13; ++i) {
    iterateOnce();
  }
  EXPECT_EQ(output.getOutputValue(), 1);

  for (int i = 0; i < 10; ++i) {
    iterateOnce();
  }
  EXPECT_EQ(output.getOutputValue(), 0);
}

TEST_F(HvacControlFlowF, offRetriesWithAuxMaxAfterMinOnTime) {
  useCooling();
  hvac->setMinOnTimeS(20);
  settleMode(SUPLA_HVAC_MODE_COOL);
  ASSERT_EQ(output.getOutputValue(), -1);

  auxTemperature->setValue(80.0);
  iterateOnce();
  hvac->setTargetMode(SUPLA_HVAC_MODE_OFF);
  ASSERT_EQ(output.getOutputValue(), -1);

  for (int i = 0; i < 13; ++i) {
    iterateOnce();
  }
  EXPECT_EQ(output.getOutputValue(), -1);

  for (int i = 0; i < 10; ++i) {
    iterateOnce();
  }
  EXPECT_EQ(output.getOutputValue(), 0);
}

TEST_F(HvacControlFlowF, antifreezeCanHeatInOffModeDespiteForcedOffSensor) {
  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  mainTemperature->setValue(0.0);
  forcedOffSensor->clear();
  settleMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_EQ(output.getOutputValue(), 1);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagAntifreezeOverheatActive());
  EXPECT_FALSE(hvac->isHvacFlagForcedOffBySensor());
}

TEST_F(HvacControlFlowF, overheatCanCoolInOffModeDespiteForcedOffSensor) {
  useCooling();
  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  mainTemperature->setValue(38.0);
  forcedOffSensor->clear();
  settleMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_EQ(output.getOutputValue(), -1);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagAntifreezeOverheatActive());
  EXPECT_FALSE(hvac->isHvacFlagForcedOffBySensor());
}

TEST_F(HvacControlFlowF, auxMaxConstrainsAntifreezeHeatingInOffMode) {
  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  mainTemperature->setValue(0.0);
  auxTemperature->setValue(80.0);
  settleMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_EQ(output.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagAntifreezeOverheatActive());
}

TEST_F(HvacControlFlowF,
       auxMaxAtMostConstrainsAntifreezeJustAboveSetpoint) {
  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  mainTemperature->setValue(0.0);
  auxTemperature->setValue(75.01);
  settleMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_EQ(output.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagAntifreezeOverheatActive());
}

TEST_F(HvacControlFlowF,
       auxMaxAtMostConstrainsAntifreezeAtSetpointPlusHysteresis) {
  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  mainTemperature->setValue(0.0);
  auxTemperature->setValue(75.4);
  settleMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_EQ(output.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagAntifreezeOverheatActive());
}

TEST_F(HvacControlFlowF, auxMinConstrainsOverheatCoolingInOffMode) {
  useCooling();
  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  mainTemperature->setValue(38.0);
  auxTemperature->setValue(10.0);
  settleMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_EQ(output.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagAntifreezeOverheatActive());
}

TEST_F(HvacControlFlowF,
       auxMinAtMostConstrainsOverheatJustBelowSetpoint) {
  useCooling();
  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  mainTemperature->setValue(38.0);
  auxTemperature->setValue(14.99);
  settleMode(SUPLA_HVAC_MODE_OFF);

  EXPECT_EQ(output.getOutputValue(), 0);
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagAntifreezeOverheatActive());
}

TEST_F(HvacControlFlowF, antifreezeActivationRespectsMinimumOffTime) {
  hvac->setMinOnTimeS(0);
  hvac->setMinOffTimeS(10);
  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  settleMode(SUPLA_HVAC_MODE_HEAT);
  ASSERT_EQ(output.getOutputValue(), 1);

  mainTemperature->setValue(25.0);
  iterateOnce();
  ASSERT_EQ(output.getOutputValue(), 0);

  mainTemperature->setValue(0.0);
  iterateOnce();
  EXPECT_EQ(output.getOutputValue(), 0);

  for (int i = 0; i < 8; ++i) {
    iterateOnce();
  }
  EXPECT_EQ(output.getOutputValue(), 0);

  iterateOnce();
  EXPECT_EQ(output.getOutputValue(), 1);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagAntifreezeOverheatActive());
}

TEST_F(HvacControlFlowF, overheatActivationRespectsMinimumOffTime) {
  useCooling();
  hvac->setMinOnTimeS(0);
  hvac->setMinOffTimeS(10);
  hvac->setAntiFreezeAndHeatProtectionEnabled(true);
  settleMode(SUPLA_HVAC_MODE_COOL);
  ASSERT_EQ(output.getOutputValue(), -1);

  mainTemperature->setValue(20.0);
  iterateOnce();
  ASSERT_EQ(output.getOutputValue(), 0);

  mainTemperature->setValue(38.0);
  iterateOnce();
  EXPECT_EQ(output.getOutputValue(), 0);

  for (int i = 0; i < 8; ++i) {
    iterateOnce();
  }
  EXPECT_EQ(output.getOutputValue(), 0);

  iterateOnce();
  EXPECT_EQ(output.getOutputValue(), -1);
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagAntifreezeOverheatActive());
}

TEST_F(HvacControlFlowF, normalRegulationStillHonorsMinimumOffTime) {
  hvac->setMinOffTimeS(10);
  settleMode(SUPLA_HVAC_MODE_HEAT);
  ASSERT_EQ(output.getOutputValue(), 1);

  mainTemperature->setValue(25.0);
  for (int i = 0; i < 6; ++i) {
    iterateOnce();
  }
  ASSERT_EQ(output.getOutputValue(), 0);

  mainTemperature->setValue(18.0);
  iterateOnce();
  EXPECT_EQ(output.getOutputValue(), 0);

  for (int i = 0; i < 10; ++i) {
    iterateOnce();
  }
  EXPECT_EQ(output.getOutputValue(), 1);
}
