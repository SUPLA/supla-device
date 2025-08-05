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

using ::testing::Return;

class CustomIoMock : public Supla::Io::Base {
 public:
  MOCK_METHOD(
      void, customPinMode, (int channelNumber, uint8_t pin, uint8_t mode));
  MOCK_METHOD(int, customDigitalRead, (int channelNumber, uint8_t pin));
  MOCK_METHOD(
      void, customDigitalWrite, (int channelNumber, uint8_t pin, uint8_t val));
  MOCK_METHOD(unsigned int, customPulseIn, (int channelNumber, uint8_t pin,
      uint8_t value, unsigned long timeoutMicro));
  MOCK_METHOD(void,
              customAnalogWrite,
              (int channelNumber, uint8_t pin, int val));
  MOCK_METHOD(int, customAnalogRead, (int channelNumber, uint8_t pin));
  MOCK_METHOD(void,
              customAttachInterrupt,
              (uint8_t pin, void (*func)(void), int mode));
  MOCK_METHOD(void, customDetachInterrupt, (uint8_t pin));
  MOCK_METHOD(uint8_t, customPinToInterrupt, (uint8_t pin));
};

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

TEST(IoTest, OperationsWithCustomIoInteface) {
  DigitalInterfaceMock hwInterfaceMock;
  CustomIoMock ioMock;

  EXPECT_CALL(hwInterfaceMock, pinMode).Times(0);
  EXPECT_CALL(hwInterfaceMock, digitalWrite).Times(0);
  EXPECT_CALL(hwInterfaceMock, digitalRead).Times(0);

  EXPECT_CALL(ioMock, customPinMode(-1, 12, INPUT));
  EXPECT_CALL(ioMock, customDigitalRead(-1, 11))
    .WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, customDigitalWrite(-1, 13, HIGH));

  Supla::Io::pinMode(12, INPUT);
  EXPECT_EQ(Supla::Io::digitalRead(11), HIGH);
  Supla::Io::digitalWrite(13, HIGH);

}
TEST(IoTest, OperationsWithCustomIoIntefaceWithChannel) {
  DigitalInterfaceMock hwInterfaceMock;
  CustomIoMock ioMock;

  // Custom io interface should not call arduino's methods 
  EXPECT_CALL(hwInterfaceMock, pinMode).Times(0);
  EXPECT_CALL(hwInterfaceMock, digitalWrite).Times(0);
  EXPECT_CALL(hwInterfaceMock, digitalRead).Times(0);

  EXPECT_CALL(ioMock, customPinMode(6, 12, INPUT));
  EXPECT_CALL(ioMock, customDigitalRead(6, 11))
    .WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, customDigitalWrite(6, 13, HIGH));

  Supla::Io::pinMode(6, 12, INPUT);
  EXPECT_EQ(Supla::Io::digitalRead(6, 11), HIGH);
  Supla::Io::digitalWrite(6, 13, HIGH);

}
