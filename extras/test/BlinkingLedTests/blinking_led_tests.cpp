// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <arduino_mock.h>
#include <supla/control/blinking_led.h>
#include <supla_io_mock.h>

using ::testing::Return;

TEST(BlinkingLedTests, IoPinConstructorUsesSeparateIoAndOutputPolarity) {
  SuplaIoMock io;
  TimeInterfaceMock timeMock;
  TimeInterface::instance = &timeMock;
  EXPECT_CALL(timeMock, millis()).WillRepeatedly(Return(0));

  ::testing::InSequence seq;
  EXPECT_CALL(io, customDigitalWrite(-1, 7, LOW)).Times(1);
  EXPECT_CALL(io, customPinMode(-1, 7, OUTPUT)).Times(1);

  Supla::Control::BlinkingLed led(Supla::Io::IoPin(7, &io));
  led.onInit();
}

TEST(BlinkingLedTests, IoPinConstructorSupportsInputPolarityForLegacyInvert) {
  SuplaIoMock io;
  TimeInterfaceMock timeMock;
  TimeInterface::instance = &timeMock;
  EXPECT_CALL(timeMock, millis()).WillRepeatedly(Return(0));

  ::testing::InSequence seq;
  EXPECT_CALL(io, customDigitalWrite(-1, 7, HIGH)).Times(1);
  EXPECT_CALL(io, customPinMode(-1, 7, OUTPUT)).Times(1);

  Supla::Control::BlinkingLed led(Supla::Io::IoPin(7, &io), true);
  led.onInit();
}
