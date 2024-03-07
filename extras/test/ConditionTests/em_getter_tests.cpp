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
#include <gmock/gmock.h>

#include <supla/condition_getter.h>
#include <supla/element.h>
#include <supla/channel_element.h>
#include <supla/sensor/electricity_meter.h>
#include <simple_time.h>

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Args;
using ::testing::ElementsAre;

TEST(EmGetterTests, VoltageGetter) {
  SimpleTime time;
  auto getter = EmVoltage();

  bool isValid = true;
  EXPECT_EQ(getter->getValue(nullptr, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Element element;
  isValid = true;
  EXPECT_EQ(getter->getValue(&element, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::ChannelElement chElement;
  isValid = true;
  EXPECT_EQ(getter->getValue(&chElement, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Sensor::ElectricityMeter em;
  isValid = true;
  EXPECT_EQ(getter->getValue(&em, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  em.setVoltage(0, 220.5 * 100);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 220.5, 0.05);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setVoltage(0, 120 * 100);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 120, 0.05);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setVoltage(1, 320 * 100); // set voltage on another phase
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 120, 0.05);
  EXPECT_TRUE(isValid);

  auto getterPhase2 = EmVoltage(1);

  EXPECT_NEAR(getterPhase2->getValue(&em, &isValid), 320, 0.05);
  EXPECT_TRUE(isValid);

  delete getter;
  delete getterPhase2;
}

TEST(EmGetterTests, CurrentGetter) {
  SimpleTime time;
  auto getter = EmCurrent();

  bool isValid = true;
  EXPECT_EQ(getter->getValue(nullptr, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Element element;
  isValid = true;
  EXPECT_EQ(getter->getValue(&element, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::ChannelElement chElement;
  isValid = true;
  EXPECT_EQ(getter->getValue(&chElement, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Sensor::ElectricityMeter em;
  isValid = true;
  EXPECT_EQ(getter->getValue(&em, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  em.setCurrent(0, 12 * 1000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 12.0, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setCurrent(0, 8.52 * 1000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setCurrent(1, 66 * 1000); // set voltage on another phase
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  auto getterPhase2 = EmCurrent(1);

  EXPECT_NEAR(getterPhase2->getValue(&em, &isValid), 66, 0.05);
  EXPECT_TRUE(isValid);

  delete getter;
  delete getterPhase2;
}

TEST(EmGetterTests, TotalCurrentGetter) {
  SimpleTime time;
  auto getter = EmTotalCurrent();

  bool isValid = true;
  EXPECT_EQ(getter->getValue(nullptr, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Element element;
  isValid = true;
  EXPECT_EQ(getter->getValue(&element, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::ChannelElement chElement;
  isValid = true;
  EXPECT_EQ(getter->getValue(&chElement, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Sensor::ElectricityMeter em;
  isValid = true;
  EXPECT_EQ(getter->getValue(&em, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  em.setCurrent(0, 12 * 1000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 12.0, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setCurrent(0, 8.52 * 1000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setCurrent(1, 66 * 1000); // set voltage on another phase
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52 + 66, 0.005);
  EXPECT_TRUE(isValid);

  delete getter;
}

TEST(EmGetterTests, PowerActiveWGetter) {
  SimpleTime time;
  auto getter = EmPowerActiveW();

  bool isValid = true;
  EXPECT_EQ(getter->getValue(nullptr, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Element element;
  isValid = true;
  EXPECT_EQ(getter->getValue(&element, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::ChannelElement chElement;
  isValid = true;
  EXPECT_EQ(getter->getValue(&chElement, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Sensor::ElectricityMeter em;
  // current is set, so EM considers that we have measurements
  em.setCurrent(0, 12 * 1000);
  isValid = true;
  EXPECT_EQ(getter->getValue(&em, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  em.setPowerActive(0, 300 * 100000);  // 300 W
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 300.0, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerActive(0, 8.52 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerActive(1, 66 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  auto getterPhase2 = EmPowerActiveW(1);

  EXPECT_NEAR(getterPhase2->getValue(&em, &isValid), 66, 0.05);
  EXPECT_TRUE(isValid);

  delete getter;
  delete getterPhase2;
}

TEST(EmGetterTests, TotalPowerActiveWGetter) {
  SimpleTime time;
  auto getter = EmTotalPowerActiveW();

  bool isValid = true;
  EXPECT_EQ(getter->getValue(nullptr, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Element element;
  isValid = true;
  EXPECT_EQ(getter->getValue(&element, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::ChannelElement chElement;
  isValid = true;
  EXPECT_EQ(getter->getValue(&chElement, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Sensor::ElectricityMeter em;
  // current is set, so EM considers that we have measurements
  em.setCurrent(0, 12 * 1000);
  isValid = true;
  EXPECT_EQ(getter->getValue(&em, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  em.setPowerActive(0, 300 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 300.0, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerActive(0, 8.52 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerActive(1, 66 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52 + 66, 0.005);
  EXPECT_TRUE(isValid);

  delete getter;
}

TEST(EmGetterTests, PowerApparentGetter) {
  SimpleTime time;
  auto getter = EmPowerApparentVA();

  bool isValid = true;
  EXPECT_EQ(getter->getValue(nullptr, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Element element;
  isValid = true;
  EXPECT_EQ(getter->getValue(&element, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::ChannelElement chElement;
  isValid = true;
  EXPECT_EQ(getter->getValue(&chElement, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Sensor::ElectricityMeter em;
  // current is set, so EM considers that we have measurements
  em.setCurrent(0, 12 * 1000);
  isValid = true;
  EXPECT_EQ(getter->getValue(&em, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  em.setPowerApparent(0, 300 * 100000); // 300
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 300.0, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerApparent(0, 8.52 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerApparent(1, 66 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  auto getterPhase2 = EmPowerApparentVA(1);

  EXPECT_NEAR(getterPhase2->getValue(&em, &isValid), 66, 0.05);
  EXPECT_TRUE(isValid);

  delete getter;
  delete getterPhase2;
}

TEST(EmGetterTests, TotalPowerApparentGetter) {
  SimpleTime time;
  auto getter = EmTotalPowerApparentVA();

  bool isValid = true;
  EXPECT_EQ(getter->getValue(nullptr, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Element element;
  isValid = true;
  EXPECT_EQ(getter->getValue(&element, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::ChannelElement chElement;
  isValid = true;
  EXPECT_EQ(getter->getValue(&chElement, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Sensor::ElectricityMeter em;
  // current is set, so EM considers that we have measurements
  em.setCurrent(0, 12 * 1000);
  isValid = true;
  EXPECT_EQ(getter->getValue(&em, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  em.setPowerApparent(0, 300 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 300.0, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerApparent(0, 8.52 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerApparent(1, 66 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52 + 66, 0.005);
  EXPECT_TRUE(isValid);

  delete getter;
}

TEST(EmGetterTests, PowerReactiveGetter) {
  SimpleTime time;
  auto getter = EmPowerReactiveVar();

  bool isValid = true;
  EXPECT_EQ(getter->getValue(nullptr, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Element element;
  isValid = true;
  EXPECT_EQ(getter->getValue(&element, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::ChannelElement chElement;
  isValid = true;
  EXPECT_EQ(getter->getValue(&chElement, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Sensor::ElectricityMeter em;
  // current is set, so EM considers that we have measurements
  em.setCurrent(0, 12 * 1000);
  isValid = true;
  EXPECT_EQ(getter->getValue(&em, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  em.setPowerReactive(0, 300 * 100000); // 300 W
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 300.0, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerReactive(0, 8.52 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerReactive(1, 66 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  auto getterPhase2 = EmPowerReactiveVar(1);

  EXPECT_NEAR(getterPhase2->getValue(&em, &isValid), 66, 0.05);
  EXPECT_TRUE(isValid);

  delete getter;
  delete getterPhase2;
}

TEST(EmGetterTests, TotalPowerReactiveGetter) {
  SimpleTime time;
  auto getter = EmTotalPowerReactiveVar();

  bool isValid = true;
  EXPECT_EQ(getter->getValue(nullptr, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Element element;
  isValid = true;
  EXPECT_EQ(getter->getValue(&element, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::ChannelElement chElement;
  isValid = true;
  EXPECT_EQ(getter->getValue(&chElement, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  Supla::Sensor::ElectricityMeter em;
  // current is set, so EM considers that we have measurements
  em.setCurrent(0, 12 * 1000);
  isValid = true;
  EXPECT_EQ(getter->getValue(&em, &isValid), 0.0);
  EXPECT_FALSE(isValid);

  em.setPowerReactive(0, 300 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 300.0, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerReactive(0, 8.52 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52, 0.005);
  EXPECT_TRUE(isValid);

  isValid = false;
  em.setPowerReactive(1, 66 * 100000);
  em.updateChannelValues();
  EXPECT_NEAR(getter->getValue(&em, &isValid), 8.52 + 66, 0.005);
  EXPECT_TRUE(isValid);

  delete getter;
}
