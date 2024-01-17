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

#include <arduino_mock.h>
#include <gtest/gtest.h>
#include <supla/sensor/general_purpose_measurement.h>
#include <measurment_driver_mock.h>

using ::testing::Return;

class GpMeasurementTestsFixture : public ::testing::Test {
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


TEST_F(GpMeasurementTestsFixture, initNaNValue) {
  MeasurementDriverMock driver;
  Supla::Sensor::GeneralPurposeMeasurement gp(&driver);

  EXPECT_CALL(driver, initialize()).Times(1);
  EXPECT_CALL(driver, getValue()).Times(1).WillOnce(Return(NAN));

  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 0);

  gp.onInit();
  EXPECT_TRUE(std::isnan(gp.getChannel()->getValueDouble()));
}

TEST_F(GpMeasurementTestsFixture, initValue) {
  MeasurementDriverMock driver;
  Supla::Sensor::GeneralPurposeMeasurement gp(&driver);

  EXPECT_CALL(driver, initialize()).Times(1);
  EXPECT_CALL(driver, getValue()).Times(1).WillOnce(Return(3.1415));

  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 0);

  gp.onInit();
  EXPECT_DOUBLE_EQ(gp.getChannel()->getValueDouble(), 3.1415);
}

