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
#include <supla/control/simple_button.h>

using ::testing::Return;

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

class TimeInterfaceStub : public TimeInterface {
  public:
    virtual uint32_t millis() override {
      static uint32_t value = 0;
      value += 1000;
      return value;
    }
};

TEST(SimpleButtonTests, NoPullupInit) {
  TimeInterfaceStub time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5)).WillOnce(Return(0));

  EXPECT_CALL(mock1, handleAction).Times(0);

  Supla::Control::SimpleButton button(5, false, false);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);
}

TEST(SimpleButtonTests, PullupInitAndPress) {
  TimeInterfaceMock time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT_PULLUP));
  EXPECT_CALL(ioMock, digitalRead(5))
    .WillOnce(Return(1))
    .WillOnce(Return(0)) // time 1000 - first read
    .WillOnce(Return(0)) // second read, should be ignored
    .WillOnce(Return(0)) // third read, should trigger on_press
    .WillOnce(Return(0)) // time 1090 
    .WillOnce(Return(1)) // time 1100 
    .WillOnce(Return(1)) // time 1110 
    .WillOnce(Return(1)) // time 1150 
    ;

  EXPECT_CALL(time, millis)
    .WillOnce(Return(1000)) // first read
    .WillOnce(Return(1010)) // should be ignored by filtering time
    .WillOnce(Return(1030)) // should trigger on press
    .WillOnce(Return(1040)) // 
    .WillOnce(Return(1050)) // 
    .WillOnce(Return(1060)) // 
    .WillOnce(Return(1090)) // 
    .WillOnce(Return(1100)) // 
    .WillOnce(Return(1110)) // 
    .WillOnce(Return(1150)) // 
    ;


  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(1);

  Supla::Control::SimpleButton button(5, true, true);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);

  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();

}

TEST(SimpleButtonTests, PullupInitAndPressWithoutNoiseFilter) {
  TimeInterfaceMock time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT_PULLUP));
  EXPECT_CALL(ioMock, digitalRead(5))
    .WillOnce(Return(1))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(1))
    ;

  EXPECT_CALL(time, millis)
    .WillOnce(Return(1000))
    .WillOnce(Return(1010))
    .WillOnce(Return(1100))
    .WillOnce(Return(1200))
    .WillOnce(Return(1300))
    ;


  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(1);

  Supla::Control::SimpleButton button(5, true, true);
  button.setSwNoiseFilterDelay(0);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);

  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();

}

TEST(SimpleButtonTests, PullupInitAndPressWithoutDebounce) {
  TimeInterfaceMock time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT_PULLUP));
  EXPECT_CALL(ioMock, digitalRead(5))
    .WillOnce(Return(1))
    
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(0))
    .WillOnce(Return(1))
    .WillOnce(Return(1))
    ;

  EXPECT_CALL(time, millis)
    .WillOnce(Return(1000))
    .WillOnce(Return(1010))
    .WillOnce(Return(1100))
    .WillOnce(Return(1101))
    .WillOnce(Return(1145))
    ;


  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(1);

  Supla::Control::SimpleButton button(5, true, true);
  button.setDebounceDelay(0);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);

  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();
  button.onTimer();

}

