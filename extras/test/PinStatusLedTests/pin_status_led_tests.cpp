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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/control/pin_status_led.h>
#include <supla_io_mock.h>

using ::testing::Return;

TEST(PinStatusLedTest, IoPinConstructorUsesSourcePinPolarity) {
  SuplaIoMock srcIo;
  SuplaIoMock outIo;

  ::testing::InSequence seq;

  EXPECT_CALL(srcIo, customDigitalRead(-1, 11)).WillOnce(Return(LOW));
  EXPECT_CALL(outIo, customDigitalRead(-1, 12)).WillOnce(Return(LOW));
  EXPECT_CALL(outIo, customDigitalWrite(-1, 12, HIGH)).Times(1);
  EXPECT_CALL(outIo, customPinMode(-1, 12, OUTPUT)).Times(1);

  EXPECT_CALL(srcIo, customDigitalRead(-1, 11)).WillOnce(Return(HIGH));
  EXPECT_CALL(outIo, customDigitalRead(-1, 12)).WillOnce(Return(HIGH));
  EXPECT_CALL(outIo, customDigitalWrite(-1, 12, LOW)).Times(1);

  Supla::Io::IoPin srcPin(11, &srcIo);
  srcPin.setActiveHigh(false);
  Supla::Io::IoPin outPin(12, &outIo);

  Supla::Control::PinStatusLed led(srcPin, outPin);
  led.onInit();
  led.iterateAlways();
}

TEST(PinStatusLedTest, IoPinConstructorUsesSeparateIoForInputAndOutput) {
  SuplaIoMock srcIo;
  SuplaIoMock outIo;

  ::testing::InSequence seq;

  EXPECT_CALL(srcIo, customDigitalRead(-1, 11)).WillOnce(Return(HIGH));
  EXPECT_CALL(outIo, customDigitalRead(-1, 12)).WillOnce(Return(LOW));
  EXPECT_CALL(outIo, customDigitalWrite(-1, 12, HIGH)).Times(1);
  EXPECT_CALL(outIo, customPinMode(-1, 12, OUTPUT)).Times(1);

  EXPECT_CALL(srcIo, customDigitalRead(-1, 11)).WillOnce(Return(LOW));
  EXPECT_CALL(outIo, customDigitalRead(-1, 12)).WillOnce(Return(HIGH));
  EXPECT_CALL(outIo, customDigitalWrite(-1, 12, LOW)).Times(1);

  Supla::Control::PinStatusLed led(Supla::Io::IoPin(11, &srcIo),
                                   Supla::Io::IoPin(12, &outIo));
  led.onInit();
  led.iterateAlways();
}

TEST(PinStatusLedTest, ReplicationTest) {
  EXPECT_EQ(DigitalInterface::instance, nullptr);

  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;

  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, digitalRead(1)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(LOW));
  //  EXPECT_CALL(ioMock, digitalWrite(2, LOW)).Times(1);
  EXPECT_CALL(ioMock, pinMode(2, OUTPUT)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(1)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(LOW));
  //  EXPECT_CALL(ioMock, digitalWrite(2, LOW)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(1)).WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalWrite(2, HIGH)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(1)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, digitalWrite(2, LOW)).Times(1);

  Supla::Control::PinStatusLed led(1, 2);
  led.onInit();
  led.iterateAlways();  // LOW->LOW
  led.iterateAlways();  // HIGH->HIGH
  led.iterateAlways();  // LOW->LOW
}

TEST(PinStatusLedTest, ReplicationTestWithInvertedLogic) {
  EXPECT_EQ(DigitalInterface::instance, nullptr);

  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;

  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, digitalRead(1)).WillOnce(Return(LOW));  // onInit
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalWrite(2, HIGH)).Times(1);
  EXPECT_CALL(ioMock, pinMode(2, OUTPUT)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(1)).WillOnce(Return(LOW));  // iterateAlways
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(HIGH));
  //  EXPECT_CALL(ioMock, digitalWrite(2, HIGH)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(1)).WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, digitalWrite(2, LOW)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(1)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalWrite(2, HIGH)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(1))
      .WillOnce(Return(LOW));  // disable inverted logic
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(HIGH));
  EXPECT_CALL(ioMock, digitalWrite(2, LOW)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(1)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalRead(2)).WillOnce(Return(LOW));
  //  EXPECT_CALL(ioMock, digitalWrite(2, HIGH)).Times(1);

  Supla::Control::PinStatusLed led(1, 2, true);
  led.onInit();
  led.iterateAlways();  // LOW->HIGH
  led.iterateAlways();  // HIGH->LOW
  led.iterateAlways();  // LOW->HIGH

  led.setInvertedLogic(false);  // LOW->LOW
  led.iterateAlways();          // HIGH->HIGH
}
