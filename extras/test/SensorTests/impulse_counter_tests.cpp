// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
