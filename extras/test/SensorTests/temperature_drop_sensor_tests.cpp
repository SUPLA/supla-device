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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/sensor/temperature_drop_sensor.h>
#include <simple_time.h>

TEST(TemperatureDropSensorTests, ThermometerMissing) {
  SimpleTime time;
  Supla::Sensor::TemperatureDropSensor sensor(nullptr);

  auto elBinary = Supla::Element::getElementByChannelNumber(0);
  ASSERT_NE(elBinary, nullptr);
  auto ch = elBinary->getChannel();
  ASSERT_NE(ch, nullptr);

  sensor.onInit();
  elBinary->onInit();
  sensor.iterateAlways();
  elBinary->iterateAlways();

  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());
}

TEST(TemperatureDropSensorTests, InitialState) {
  SimpleTime time;
  Supla::Sensor::VirtualThermometer thermometer;
  Supla::Sensor::TemperatureDropSensor sensor(&thermometer);

  auto elBinary = Supla::Element::getElementByChannelNumber(1);
  ASSERT_NE(elBinary, nullptr);
  auto ch = elBinary->getChannel();
  ASSERT_NE(ch, nullptr);

  sensor.onInit();
  sensor.iterateAlways();
  elBinary->onInit();
  elBinary->iterateAlways();

  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());
}


TEST(TemperatureDropSensorTests, DropFrom23To22) {
  SimpleTime time;
  Supla::Sensor::VirtualThermometer thermometer;
  Supla::Sensor::TemperatureDropSensor sensor(&thermometer);

  auto elBinary = Supla::Element::getElementByChannelNumber(1);
  ASSERT_NE(elBinary, nullptr);
  auto ch = elBinary->getChannel();
  ASSERT_NE(ch, nullptr);

  thermometer.setValue(23);
  thermometer.onInit();
  elBinary->onInit();
  sensor.onInit();
  elBinary->iterateAlways();

  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  // forward time by 30 min with 10 seconds per step
  for (int i = 0; i < 6*30; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(22);
  // forward time by 30 min with 10 seconds per step
  for (int i = 0; i < 6*30; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());
}


TEST(TemperatureDropSensorTests, DropFrom23To19) {
  SimpleTime time;
  Supla::Sensor::VirtualThermometer thermometer;
  Supla::Sensor::TemperatureDropSensor sensor(&thermometer);

  auto elBinary = Supla::Element::getElementByChannelNumber(1);
  ASSERT_NE(elBinary, nullptr);
  auto ch = elBinary->getChannel();
  ASSERT_NE(ch, nullptr);

  thermometer.setValue(23);
  thermometer.onInit();
  elBinary->onInit();
  sensor.onInit();
  elBinary->iterateAlways();

  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  // forward time by 30 min with 10 seconds per step
  for (int i = 0; i < 6*30; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(19);
  // forward time by 1 min with 10 seconds per step
  for (int i = 0; i < 6; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), false);
  EXPECT_TRUE(sensor.isDropDetected());

  // forward time by 25 min with 10 seconds per step
  for (int i = 0; i < 6*25; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), false);
  EXPECT_TRUE(sensor.isDropDetected());


  // forward time by 6 min with 10 seconds per step
  for (int i = 0; i < 6*6; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());
}

TEST(TemperatureDropSensorTests, DropFrom24To20ThenBackTo23) {
  SimpleTime time;
  Supla::Sensor::VirtualThermometer thermometer;
  Supla::Sensor::TemperatureDropSensor sensor(&thermometer);

  auto elBinary = Supla::Element::getElementByChannelNumber(1);
  ASSERT_NE(elBinary, nullptr);
  auto ch = elBinary->getChannel();
  ASSERT_NE(ch, nullptr);

  thermometer.setValue(24);
  thermometer.onInit();
  elBinary->onInit();
  sensor.onInit();
  elBinary->iterateAlways();

  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  // forward time by 30 min with 10 seconds per step
  for (int i = 0; i < 6*30; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(20);
  // forward time by 1 min with 10 seconds per step
  for (int i = 0; i < 6; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), false);
  EXPECT_TRUE(sensor.isDropDetected());

  // forward time by 15 min with 10 seconds per step
  for (int i = 0; i < 6*15; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), false);
  EXPECT_TRUE(sensor.isDropDetected());

  thermometer.setValue(23.2);
  // forward time by 6 min with 10 seconds per step
  for (int i = 0; i < 6; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());
}

TEST(TemperatureDropSensorTests, TemperatureChanges) {
  SimpleTime time;
  Supla::Sensor::VirtualThermometer thermometer;
  Supla::Sensor::TemperatureDropSensor sensor(&thermometer);

  auto elBinary = Supla::Element::getElementByChannelNumber(1);
  ASSERT_NE(elBinary, nullptr);
  auto ch = elBinary->getChannel();
  ASSERT_NE(ch, nullptr);

  thermometer.setValue(24);
  thermometer.onInit();
  elBinary->onInit();
  sensor.onInit();
  elBinary->iterateAlways();

  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(20);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), false);
  EXPECT_TRUE(sensor.isDropDetected());

  thermometer.setValue(23.2);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(35);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(34);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(33);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(32);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(31);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(310.0);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  thermometer.setValue(-103.0);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), false);
  EXPECT_TRUE(sensor.isDropDetected());

  thermometer.setValue(53.0);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), false);
  EXPECT_TRUE(sensor.isDropDetected());

  thermometer.setValue(90.0);
  // forward time by 2 min with 10 seconds per step
  for (int i = 0; i < 6*2; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());
}





