// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <supla/correction.h>
#include <supla/sensor/virtual_therm_hygro_meter.h>
#include <supla/sensor/virtual_thermometer.h>
#include <config_simulator.h>
#include <simple_time.h>
#include "supla/sensor/therm_hygro_meter.h"

using Supla::Correction;

TEST(CorrectionTests, CorrectionGetCheck) {
  Correction::add(5, 2.5);

  EXPECT_EQ(Correction::get(5), 2.5);
  EXPECT_EQ(Correction::get(5, true), 0);
  EXPECT_EQ(Correction::get(1), 0);

  Correction::add(1, 3.14);
  EXPECT_EQ(Correction::get(1), 3.14);
  EXPECT_EQ(Correction::get(5), 2.5);

  Correction::clear();

  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 0);
}

TEST(CorrectionTests, CorrectionGetCheckForSecondary) {
  Correction::add(5, 2.5, true);

  EXPECT_EQ(Correction::get(5), 0);
  EXPECT_EQ(Correction::get(5, true), 2.5);
  EXPECT_EQ(Correction::get(1), 0);

  Correction::add(1, 3.14);
  EXPECT_EQ(Correction::get(1), 3.14);
  EXPECT_EQ(Correction::get(5, true), 2.5);

  Correction::clear();

  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 0);
}

TEST(CorrectionTests, CorrectionChangeTest) {
  Correction::add(5, 2.5);

  EXPECT_EQ(Correction::get(5), 2.5);
  EXPECT_EQ(Correction::get(5, true), 0);
  EXPECT_EQ(Correction::get(1), 0);

  Correction::add(5, 3.14);
  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 3.14);

  Correction::clear();

  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 0);
}

TEST(CorrectionTests, ThermometerCorrectionTest) {
  ConfigSimulator config;
  SimpleTime time;
  Supla::Sensor::VirtualThermometer temp;

  temp.onLoadConfig(nullptr);
  temp.setValue(23.0);

  time.advance(20000);
  temp.iterateAlways();

  EXPECT_EQ(temp.getChannel()->getValueDouble(), 23.0);

  temp.applyCorrectionsAndStoreIt(-50, 0);

  EXPECT_EQ(temp.getChannel()->getValueDouble(), 23.0);

  time.advance(20000);
  temp.iterateAlways();
  EXPECT_EQ(temp.getChannel()->getValueDouble(), 18.0);

  temp.applyCorrectionsAndStoreIt(0, 50);
  time.advance(20000);
  temp.iterateAlways();
  EXPECT_EQ(temp.getChannel()->getValueDouble(), 23.0);

  temp.applyCorrectionsAndStoreIt(-30, 50);
  time.advance(20000);
  temp.iterateAlways();
  EXPECT_EQ(temp.getChannel()->getValueDouble(), 20.0);

  Supla::Correction::clear();  // cleanup
}

TEST(CorrectionTests, ThermHygroMeterCorrectionTest) {
  ConfigSimulator config;
  SimpleTime time;
  Supla::Sensor::VirtualThermHygroMeter th;

  th.onLoadConfig(nullptr);
  th.setTemp(23.0);
  th.setHumi(55.0);

  time.advance(20000);
  th.iterateAlways();

  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 23.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 55.0);

  th.applyCorrectionsAndStoreIt(-50, 0);

  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 23.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 55.0);

  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 18.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 55.0);

  th.applyCorrectionsAndStoreIt(0, 50);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 23.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 60.0);

  th.applyCorrectionsAndStoreIt(-30, 50);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 60.0);

  th.setHumi(100.0);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 100.0);

  th.setHumi(98.0);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 100.0);

  th.applyCorrectionsAndStoreIt(-30, -100);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 88.0);

  th.setHumi(5.0);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 0.0);

  th.setTemp(-271.0);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), -273.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 0.0);

  Supla::Correction::clear();  // cleanup
}
