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

#include <arduino_mock.h>
#include <gtest/gtest.h>
#include <supla/io.h>
#include <supla_io_mock.h>

using ::testing::Return;

TEST(IoTests, PinMode) {
  DigitalInterfaceMock ioMock;
  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, pinMode(3, INPUT));
  EXPECT_CALL(ioMock, pinMode(0, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(3, INPUT_PULLUP));

  Supla::Io::pinMode(3, INPUT);
  Supla::Io::pinMode(0, OUTPUT);
  Supla::Io::pinMode(3, INPUT_PULLUP);
}

TEST(IoTests, PinModeWithChannelNumber) {
  DigitalInterfaceMock ioMock;
  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, pinMode(3, INPUT));
  EXPECT_CALL(ioMock, pinMode(0, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(3, INPUT_PULLUP));

  Supla::Io::pinMode(10, 3, INPUT);
  Supla::Io::pinMode(11, 0, OUTPUT);
  Supla::Io::pinMode(12, 3, INPUT_PULLUP);
}

TEST(IoTests, DigitalWrite) {
  DigitalInterfaceMock ioMock;
  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, digitalWrite(3, HIGH));
  EXPECT_CALL(ioMock, digitalWrite(3, LOW));
  EXPECT_CALL(ioMock, digitalWrite(99, LOW));

  Supla::Io::digitalWrite(3, HIGH);
  Supla::Io::digitalWrite(3, LOW);
  Supla::Io::digitalWrite(99, LOW);
}

TEST(IoTests, DigitalWriteWithChannel) {
  DigitalInterfaceMock ioMock;
  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, digitalWrite(3, HIGH));
  EXPECT_CALL(ioMock, digitalWrite(3, LOW));
  EXPECT_CALL(ioMock, digitalWrite(99, LOW));

  Supla::Io::digitalWrite(3, HIGH);
  Supla::Io::digitalWrite(3, LOW);
  Supla::Io::digitalWrite(99, LOW);
}

TEST(IoTests, DigitalRead) {
  DigitalInterfaceMock ioMock;

  EXPECT_CALL(ioMock, digitalRead(3))
      .WillOnce(Return(LOW))
      .WillOnce(Return(HIGH))
      .WillOnce(Return(LOW));

  EXPECT_EQ(Supla::Io::digitalRead(3), LOW);
  EXPECT_EQ(Supla::Io::digitalRead(3), HIGH);
  EXPECT_EQ(Supla::Io::digitalRead(3), LOW);
}

TEST(IoTests, DigitalReadWithChannel) {
  DigitalInterfaceMock ioMock;

  EXPECT_CALL(ioMock, digitalRead(3))
      .WillOnce(Return(LOW))
      .WillOnce(Return(HIGH))
      .WillOnce(Return(LOW));

  EXPECT_EQ(Supla::Io::digitalRead(100, 3), LOW);
  EXPECT_EQ(Supla::Io::digitalRead(100, 3), HIGH);
  EXPECT_EQ(Supla::Io::digitalRead(-1, 3), LOW);
}

TEST(IoTests, IoPinDefaultsAndFlags) {
  Supla::Io::IoPin pin;

  EXPECT_FALSE(pin.isSet());
  EXPECT_EQ(pin.getPin(), -1);
  EXPECT_FALSE(pin.isPullUp());
  EXPECT_TRUE(pin.isActiveHigh());
  EXPECT_EQ(pin.getMode(), 0);

  pin.setPin(7);
  pin.setPullUp(true);
  pin.setActiveHigh(true);
  pin.setMode(INPUT);

  EXPECT_TRUE(pin.isSet());
  EXPECT_EQ(pin.getPin(), 7);
  EXPECT_TRUE(pin.isPullUp());
  EXPECT_TRUE(pin.isActiveHigh());
  EXPECT_EQ(pin.getMode(), INPUT);

  pin.setPin(-1);

  EXPECT_FALSE(pin.isSet());
  EXPECT_EQ(pin.getPin(), -1);
}

TEST(IoTests, IoPinDelegatesToCustomIo) {
  SuplaIoMock ioMock(true);
  ::testing::InSequence seq;
  Supla::Io::IoPin pin(12, &ioMock);

  pin.setPullUp(true);
  pin.setActiveHigh(true);
  pin.setMode(INPUT);

  EXPECT_CALL(ioMock, customPinMode(7, 12, INPUT_PULLUP));
  EXPECT_CALL(ioMock, customDigitalRead(7, 12))
      .WillOnce(Return(HIGH))
      .WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, customDigitalWrite(7, 12, HIGH));
  EXPECT_CALL(ioMock, customDigitalWrite(7, 12, LOW));
  EXPECT_CALL(ioMock, customAnalogWrite(7, 12, 123));

  pin.pinMode(7);
  EXPECT_EQ(pin.digitalRead(7), HIGH);
  EXPECT_TRUE(pin.readActive(7));
  pin.writeActive(7);
  pin.writeInactive(7);
  pin.analogWrite(123, 7);
}

TEST(IoTests, IoPinConfigureAnalogOutputDelegatesToCustomIo) {
  SuplaIoMock ioMock(true);
  Supla::Io::IoPin pin(15, &ioMock);

  EXPECT_CALL(ioMock, customConfigureAnalogOutput(5, 15, false));

  pin.configureAnalogOutput(5);
}

TEST(IoTests, IoPinConfigureAnalogOutputPassesInvertFlag) {
  SuplaIoMock ioMock(true);
  Supla::Io::IoPin pin(15, &ioMock);
  pin.setActiveHigh(false);

  EXPECT_CALL(ioMock, customConfigureAnalogOutput(5, 15, true));

  pin.configureAnalogOutput(5);
}

TEST(IoTests, IoPinActiveLowInvertsReadAndWriteLevels) {
  SuplaIoMock ioMock(true);
  ::testing::InSequence seq;
  Supla::Io::IoPin pin(13, &ioMock);

  pin.setActiveHigh(false);

  EXPECT_CALL(ioMock, customDigitalRead(9, 13))
      .WillOnce(Return(LOW))
      .WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, customDigitalWrite(9, 13, LOW));
  EXPECT_CALL(ioMock, customDigitalWrite(9, 13, HIGH));

  EXPECT_TRUE(pin.readActive(9));
  EXPECT_FALSE(pin.readActive(9));
  pin.writeActive(9);
  pin.writeInactive(9);
}

TEST(IoTests, OperationsWithCustomIoInteface) {
  DigitalInterfaceMock hwInterfaceMock;
  SuplaIoMock ioMock(true);

  EXPECT_CALL(hwInterfaceMock, pinMode).Times(0);
  EXPECT_CALL(hwInterfaceMock, digitalWrite).Times(0);
  EXPECT_CALL(hwInterfaceMock, digitalRead).Times(0);

  EXPECT_CALL(ioMock, customPinMode(-1, 12, INPUT));
  EXPECT_CALL(ioMock, customDigitalRead(-1, 11)).WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, customDigitalWrite(-1, 13, HIGH));

  Supla::Io::pinMode(12, INPUT);
  EXPECT_EQ(Supla::Io::digitalRead(11), HIGH);
  Supla::Io::digitalWrite(13, HIGH);
}

TEST(IoTests, OperationsWithCustomIoIntefaceWithChannel) {
  DigitalInterfaceMock hwInterfaceMock;
  SuplaIoMock ioMock(true);

  // Custom io interface should not call arduino's methods
  EXPECT_CALL(hwInterfaceMock, pinMode).Times(0);
  EXPECT_CALL(hwInterfaceMock, digitalWrite).Times(0);
  EXPECT_CALL(hwInterfaceMock, digitalRead).Times(0);

  EXPECT_CALL(ioMock, customPinMode(6, 12, INPUT));
  EXPECT_CALL(ioMock, customDigitalRead(6, 11)).WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, customDigitalWrite(6, 13, HIGH));

  Supla::Io::pinMode(6, 12, INPUT);
  EXPECT_EQ(Supla::Io::digitalRead(6, 11), HIGH);
  Supla::Io::digitalWrite(6, 13, HIGH);
}
