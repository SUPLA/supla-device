// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

TEST(TemperatureDropSensorTests, DropFrom23To15WithDelay) {
  SimpleTime time;
  Supla::Sensor::VirtualThermometer thermometer;
  Supla::Sensor::TemperatureDropSensor sensor(&thermometer);

  auto elBinary = Supla::Element::getElementByChannelNumber(1);
  ASSERT_NE(elBinary, nullptr);
  auto ch = elBinary->getChannel();
  ASSERT_NE(ch, nullptr);

  sensor.setDropDetectionDelayMs(60000);
  sensor.setTemperatureDropThreshold(-500);

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

  thermometer.setValue(15);
  // Delay active
  // forward time by 1 min with 10 seconds per step
  for (int i = 0; i < 6; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());

  // time after delay
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

  // forward time by 20 min with 10 seconds per step
  // after 25+5 min it should change to "drop not detected" and stay this way
  for (int i = 0; i < 20*6; i++) {
    thermometer.iterateAlways();
    sensor.iterateAlways();
    elBinary->iterateAlways();
    time.advance(10000);
  }
  EXPECT_EQ(ch->getValueBool(), true);
  EXPECT_FALSE(sensor.isDropDetected());
}




