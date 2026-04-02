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

  ::testing::InSequence seq;
  EXPECT_CALL(io, customDigitalWrite(-1, 7, HIGH)).Times(1);
  EXPECT_CALL(io, customPinMode(-1, 7, OUTPUT)).Times(1);

  Supla::Control::BlinkingLed led(Supla::Io::IoPin(7, &io), true);
  led.onInit();
}
