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

#include <simple_time.h>
#include <supla/sensor/impulse_counter.h>

#include "../doubles/supla_io_mock.h"

TEST(ImpulseCounterTests, IoPinConstructorUsesSeparateIoAndPullup) {
  Supla::Channel::resetToDefaults();
  SuplaIoMock ioMock;
  SimpleTime time;

  int gpioValue = LOW;
  EXPECT_CALL(ioMock, customPinMode(0, 7, INPUT_PULLUP))
      .WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, customDigitalRead(0, 7))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));

  Supla::Io::IoPin impulsePin(7, &ioMock);
  impulsePin.setPullUp(true);
  Supla::Sensor::ImpulseCounter counter(impulsePin, true);

  counter.onInit();

  EXPECT_EQ(counter.getCounter(), 0);

  gpioValue = HIGH;
  time.advance(11);
  counter.onFastTimer();

  EXPECT_EQ(counter.getCounter(), 1);
}
