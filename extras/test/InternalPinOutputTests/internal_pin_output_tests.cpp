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

#include <supla/control/internal_pin_output.h>
#include <arduino_mock.h>
#include <supla/events.h>
#include <supla/actions.h>

using ::testing::Return;

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

TEST(InternalPinOutputTests, BasicMethodsTests) {
  const int pin = 5;
  const int pin2 = 6;
  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;

  EXPECT_CALL(timeMock, millis).WillRepeatedly(Return(0));

  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);
  EXPECT_CALL(ioMock, pinMode(pin, OUTPUT)).Times(1);
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  EXPECT_CALL(ioMock, digitalWrite(pin2, HIGH)).Times(1);
  EXPECT_CALL(ioMock, pinMode(pin2, OUTPUT)).Times(1);
  EXPECT_CALL(ioMock, digitalWrite(pin2, HIGH)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(pin)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalRead(pin2)).WillOnce(Return(HIGH));

  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);

  EXPECT_CALL(ioMock, digitalWrite(pin2, LOW)).Times(1);

  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  EXPECT_CALL(ioMock, digitalRead(pin)).WillOnce(Return(LOW));
  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);

  Supla::Control::InternalPinOutput ipo(pin);
  Supla::Control::InternalPinOutput ipo2(pin2, false);


  EXPECT_EQ(ipo.pinOnValue(), HIGH);
  EXPECT_EQ(ipo.pinOffValue(), LOW);

  EXPECT_EQ(ipo2.pinOnValue(), LOW);
  EXPECT_EQ(ipo2.pinOffValue(), HIGH);

  ipo.onInit();
  ipo2.onInit();

  EXPECT_EQ(ipo.isOn(), false);
  EXPECT_EQ(ipo2.isOn(), false);

  ipo.turnOn();
  ipo2.turnOn();

  ipo.turnOff();

  ipo.toggle();
}

TEST(InternalPinOutputTests, DefaultInitialState) {
  const int pin = 5;
  const int pin2 = 6;
  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;

  EXPECT_CALL(timeMock, millis).WillRepeatedly(Return(0));

  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);
  EXPECT_CALL(ioMock, pinMode(pin, OUTPUT)).Times(1);
  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);

  EXPECT_CALL(ioMock, digitalWrite(pin2, HIGH)).Times(1);
  EXPECT_CALL(ioMock, pinMode(pin2, OUTPUT)).Times(1);
  EXPECT_CALL(ioMock, digitalWrite(pin2, HIGH)).Times(1);

  Supla::Control::InternalPinOutput ipo(pin);
  Supla::Control::InternalPinOutput ipo2(pin2, false);

  ipo.setDefaultStateOn();
  ipo2.setDefaultStateOff();

  ipo.onInit();
  ipo2.onInit();

}

TEST(InternalPinOutputTests, TurnOnWithIterations) {
  const int pin = 5;
  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;


  ::testing::InSequence seq;

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);
  EXPECT_CALL(ioMock, pinMode(pin, OUTPUT)).Times(1);
  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);


  Supla::Control::InternalPinOutput ipo(pin);

  ipo.onInit();

  ipo.iterateAlways();
  ipo.iterateAlways();

  ipo.turnOn();
  ipo.iterateAlways();
  ipo.iterateAlways();
}

TEST(InternalPinOutputTests, TurnOnWithDuration) {
  const int pin = 5;
  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;


  ::testing::InSequence seq;

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);
  EXPECT_CALL(ioMock, pinMode(pin, OUTPUT)).Times(1);
  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(100));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(200));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(201));

  EXPECT_CALL(ioMock, digitalRead(pin)).WillOnce(Return(HIGH));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(201));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);


  Supla::Control::InternalPinOutput ipo(pin);

  ipo.onInit();

  ipo.iterateAlways();
  ipo.iterateAlways();

  ipo.turnOn(200);
  ipo.iterateAlways(); // time 0
  ipo.iterateAlways(); // time 100
  ipo.iterateAlways(); // time 200
  ipo.iterateAlways(); // time 201

  ipo.iterateAlways();
  ipo.iterateAlways();
  ipo.iterateAlways();
}

TEST(InternalPinOutputTests, TurnOffWithDuration) {
  const int pin = 5;
  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;


  ::testing::InSequence seq;

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);
  EXPECT_CALL(ioMock, pinMode(pin, OUTPUT)).Times(1);
  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(100));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(200));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(201));

  EXPECT_CALL(ioMock, digitalRead(pin)).WillOnce(Return(LOW));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(201));
  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);

  Supla::Control::InternalPinOutput ipo(pin);

  ipo.onInit();

  ipo.iterateAlways();
  ipo.iterateAlways();

  ipo.turnOff(200);
  ipo.iterateAlways(); // time 0
  ipo.iterateAlways(); // time 100
  ipo.iterateAlways(); // time 200
  ipo.iterateAlways(); // time 201

  ipo.iterateAlways();
  ipo.iterateAlways();
  ipo.iterateAlways();
}


TEST(InternalPinOutputTests, TurnOnWithStoredDuration) {
  const int pin = 5;
  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;


  ::testing::InSequence seq;

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);
  EXPECT_CALL(ioMock, pinMode(pin, OUTPUT)).Times(1);
  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(100));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(200));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(201));

  EXPECT_CALL(ioMock, digitalRead(pin)).WillOnce(Return(HIGH));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(201));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  EXPECT_CALL(timeMock, millis).WillOnce(Return(300));
  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);
  EXPECT_CALL(timeMock, millis).WillOnce(Return(400));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(600));
  EXPECT_CALL(ioMock, digitalRead(pin)).WillOnce(Return(HIGH));
  EXPECT_CALL(timeMock, millis).WillOnce(Return(600));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  Supla::Control::InternalPinOutput ipo(pin);

  ipo.onInit();

  ipo.iterateAlways();
  ipo.iterateAlways();

  ipo.setDurationMs(200);

  ipo.turnOn();
  ipo.iterateAlways(); // time 0
  ipo.iterateAlways(); // time 100
  ipo.iterateAlways(); // time 200
  ipo.iterateAlways(); // time 201

  ipo.iterateAlways();
  ipo.iterateAlways();
  ipo.iterateAlways();

  ipo.turnOn();
  ipo.iterateAlways();
  ipo.iterateAlways();
  ipo.iterateAlways();
}

TEST(InternalPinOutputTests, HandleActionTests) {
  const int pin = 5;
  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;

  EXPECT_CALL(timeMock, millis).WillRepeatedly(Return(0));

  ::testing::InSequence seq;

  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1); // onInit
  EXPECT_CALL(ioMock, pinMode(pin, OUTPUT)).Times(1);
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1); // onInit

  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1); // Turn on

  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1); // Turn off

  EXPECT_CALL(ioMock, digitalRead(pin)).WillOnce(Return(LOW)); // Toggle
  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);

  Supla::Control::InternalPinOutput ipo(pin);

  ipo.onInit();

  ipo.iterateAlways();
  ipo.iterateAlways();

  ipo.handleAction(0, Supla::TURN_ON);

  ipo.iterateAlways();
  ipo.handleAction(0, Supla::TURN_OFF);

  ipo.iterateAlways();
  ipo.handleAction(0, Supla::TOGGLE);
  ipo.iterateAlways();
}

TEST(InternalPinOutputTests, TurnOnWithAction) {
  const int pin = 5;
  DigitalInterfaceMock ioMock;
  TimeInterfaceMock timeMock;
  ActionHandlerMock ahMock;


  ::testing::InSequence seq;

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ahMock, handleAction(Supla::ON_TURN_OFF, Supla::TURN_OFF));
  EXPECT_CALL(ahMock, handleAction(Supla::ON_CHANGE, 33));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);
  EXPECT_CALL(ioMock, pinMode(pin, OUTPUT)).Times(1);
  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ahMock, handleAction(Supla::ON_TURN_OFF, Supla::TURN_OFF));
  EXPECT_CALL(ahMock, handleAction(Supla::ON_CHANGE, 33));
  EXPECT_CALL(ioMock, digitalWrite(pin, LOW)).Times(1);

  EXPECT_CALL(timeMock, millis).WillOnce(Return(0));
  EXPECT_CALL(ahMock, handleAction(Supla::ON_TURN_ON, Supla::TURN_ON));
  EXPECT_CALL(ahMock, handleAction(Supla::ON_CHANGE, 33));
  EXPECT_CALL(ioMock, digitalWrite(pin, HIGH)).Times(1);


  Supla::Control::InternalPinOutput ipo(pin);
  ipo.addAction(Supla::TURN_ON, ahMock, Supla::ON_TURN_ON);
  ipo.addAction(Supla::TURN_OFF, ahMock, Supla::ON_TURN_OFF);
  ipo.addAction(33, ahMock, Supla::ON_CHANGE);

  ipo.onInit();

  ipo.iterateAlways();
  ipo.iterateAlways();

  ipo.turnOn();
  ipo.iterateAlways();
  ipo.iterateAlways();
}

