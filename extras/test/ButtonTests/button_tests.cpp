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
#include <simple_time.h>
#include <supla/control/button.h>
#include "supla/events.h"

using ::testing::Return;

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

TEST(ButtonTests, OnPressAndOnRelease) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 second read, should be ignored
      .WillOnce(Return(1))  // #4 third read, should trigger on_press
      .WillOnce(Return(1))  // #5 time 90
      .WillOnce(Return(0))  // #6 time 100
      .WillOnce(Return(0))  // #7 time 110
      .WillOnce(Return(0));  // #8 time 150

  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(1);

  // Conditional events are generated on each press/release beacuse button
  // is not configured to detect hold nor multiclick
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_CHANGE, 5)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 4)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 6)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.onInit();  // #1
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);
  button.addAction(4, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(5, mock1, Supla::CONDITIONAL_ON_CHANGE);
  button.addAction(6, mock1, Supla::CONDITIONAL_ON_RELEASE);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(10);
  button.onTimer();  // #3 within filtering time - nothing should happen
  time.advance(20);
  button.onTimer();  // #4 ON_PRESS
  time.advance(60);
  button.onTimer();  // #5
  time.advance(10);
  button.onTimer();  // #6 new state candidate
  time.advance(10);
  button.onTimer();  // #7
  time.advance(20);
  button.onTimer();  // #8 on release
  time.advance(30);
  button.onTimer();  // #
  time.advance(10);
  button.onTimer();
  time.advance(10);
  button.onTimer();
}

TEST(ButtonTests, OnHold) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3
      .WillOnce(Return(1))  // #4
      .WillOnce(Return(1))  // #5
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(0))  // #7
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0))  // #9
      // second round
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3
      .WillOnce(Return(1))  // #4
      .WillOnce(Return(1))  // #5
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(0))  // #7
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0));  // #9

  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(4);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_LONG_CLICK_0, 5)).Times(2);

  // Conditional on_press and on_change is executed twice, becuase we simulate
  // two independent button presses
  // Conditional on_release is not executed because on_hold was send
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_CHANGE, 10)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 9)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 11)).Times(0);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(500);
  button.setMulticlickTime(300);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(5, mock1, Supla::ON_LONG_CLICK_0);
  button.addAction(6, mock1, Supla::ON_LONG_CLICK_1);
  button.addAction(7, mock1, Supla::ON_LONG_CLICK_2);
  button.addAction(8, mock1, Supla::ON_LONG_CLICK_3);
  button.addAction(9, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(10, mock1, Supla::CONDITIONAL_ON_CHANGE);
  button.addAction(11, mock1, Supla::CONDITIONAL_ON_RELEASE);

  for (int i = 0; i < 2; i++) {
    time.advance(1000);
    button.onTimer();  // #2
    time.advance(30);
    button.onTimer();  // #3 ON_PRESS
    time.advance(60);
    button.onTimer();  // #4
    time.advance(450);
    button.onTimer();  // #5 ON_HOLD
    time.advance(10000);
    button.onTimer();  // #6
    time.advance(10);
    button.onTimer();  // #7 new state candidate
    time.advance(100);
    button.onTimer();  // #8 ON_RELEASE
    time.advance(500);
    button.onTimer();  // #9
  }
}

TEST(ButtonTests, OnHoldRepeated) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3
      .WillOnce(Return(1))  // #4
      .WillOnce(Return(1))  // #5
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7
      .WillOnce(Return(1))  // #8
      .WillOnce(Return(0))  // #9
      .WillOnce(Return(0));  // #10

  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(3);
  EXPECT_CALL(mock1, handleAction(Supla::ON_LONG_CLICK_0, 5)).Times(0);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(500);
  button.repeatOnHoldEvery(100);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(5, mock1, Supla::ON_LONG_CLICK_0);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);
  button.onTimer();  // #3 ON_PRESS
  time.advance(60);
  button.onTimer();  // #4
  time.advance(450);
  button.onTimer();  // #5 ON_HOLD
  time.advance(110);
  button.onTimer();  // #6 ON_HOLD
  time.advance(10);
  button.onTimer();  // #7
  time.advance(100);
  button.onTimer();  // #8 ON_HOLD
  time.advance(10);
  button.onTimer();  // #9 new state candidate
  time.advance(30);
  button.onTimer();  // #10 ON_RELEASE
}

TEST(ButtonTests, OnHoldNotRepeated) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3
      .WillOnce(Return(1))  // #4
      .WillOnce(Return(1))  // #5
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7
      .WillOnce(Return(1))  // #8
      .WillOnce(Return(0))  // #9
      .WillOnce(Return(0));  // #10

  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_LONG_CLICK_0, 5)).Times(0);

  Supla::Control::Button button(5, false, false);

  button.setHoldTime(500);
//  button.repeatOnHoldEvery(100);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(5, mock1, Supla::ON_LONG_CLICK_0);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);
  button.onTimer();  // #3 ON_PRESS
  time.advance(60);
  button.onTimer();  // #4
  time.advance(450);
  button.onTimer();  // #5 ON_HOLD
  time.advance(110);
  button.onTimer();  // #6 ON_HOLD
  time.advance(10);
  button.onTimer();  // #7
  time.advance(100);
  button.onTimer();  // #8 ON_HOLD
  time.advance(10);
  button.onTimer();  // #9 new state candidate
  time.advance(30);
  button.onTimer();  // #10 ON_RELEASE
}

TEST(ButtonTests, Multiclick) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 click 1
      .WillOnce(Return(0))  // #4
      .WillOnce(Return(0))  // #5 release
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7 click 2
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0))  // #9 release
      .WillOnce(Return(0));  // #10 some time -> ON_CLICK_2

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_2, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(0);

  // Conditional on_press and on_change are send only on first button press.
  // Conditional on_release is send, because there was multiclick detected.
  // Conditional on_change is send twice (on_press + on_release)
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_CHANGE, 10)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 9)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 11)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);
  button.setMulticlickTime(300);
  button.onInit();
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(1, mock1, Supla::ON_CLICK_3);
  button.addAction(1, mock1, Supla::ON_CLICK_4);
  button.addAction(1, mock1, Supla::ON_CLICK_5);
  button.addAction(1, mock1, Supla::ON_CLICK_6);
  button.addAction(1, mock1, Supla::ON_CLICK_7);
  button.addAction(9, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(10, mock1, Supla::CONDITIONAL_ON_CHANGE);
  button.addAction(11, mock1, Supla::CONDITIONAL_ON_RELEASE);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);  // filtering
  button.onTimer();  // #3 click 1
  time.advance(60);  // debounce
  button.onTimer();  // #4
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(500);
  button.onTimer();  // #10 ON_CLICK_2
}

TEST(ButtonTests, OnHoldSendsLongMulticlick) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 click 1
      .WillOnce(Return(1))  // #4 ON_HOLD
      .WillOnce(Return(0))  // #5
      .WillOnce(Return(0))  // #6 release
      .WillOnce(Return(1))  // #7
      .WillOnce(Return(1))  // #8 click 1
      .WillOnce(Return(0))  // #9
      .WillOnce(Return(0))  // #10 release
      .WillOnce(Return(1))  // #11
      .WillOnce(Return(1))  // #12 click 2
      .WillOnce(Return(0))  // #13
      .WillOnce(Return(0))  // #14 release
      .WillOnce(Return(0));  // #15 some time -> ON_LONG_CLICK_2

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_2, 1)).Times(0);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_LONG_CLICK_2, 5)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);
  button.setMulticlickTime(300);
  button.onInit();  // #1
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(1, mock1, Supla::ON_CLICK_3);
  button.addAction(1, mock1, Supla::ON_CLICK_4);
  button.addAction(1, mock1, Supla::ON_CLICK_5);
  button.addAction(1, mock1, Supla::ON_CLICK_6);
  button.addAction(1, mock1, Supla::ON_CLICK_7);
  button.addAction(6, mock1, Supla::ON_LONG_CLICK_1);
  button.addAction(5, mock1, Supla::ON_LONG_CLICK_2);
  button.addAction(7, mock1, Supla::ON_LONG_CLICK_3);

  time.advance(1000);
  button.onTimer();   // #2
  time.advance(30);   // filtering
  button.onTimer();   // #3 click 1
  time.advance(800);  //
  button.onTimer();   // #4 ON_HOLD
  time.advance(60);
  button.onTimer();
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(60);
  button.onTimer();  // #10
  time.advance(30);  // filtering
  button.onTimer();  // #11 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #12
  time.advance(30);  // filtering
  button.onTimer();  // #13 release
  time.advance(500);
  button.onTimer();  // #14 ON_LONG_CLICK_2
}

TEST(ButtonTests, BistableMulticlick) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 click 1
      .WillOnce(Return(0))  // #4
      .WillOnce(Return(0))  // #5 release
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7 click 2
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0))  // #9 release
      .WillOnce(Return(0));  // #10 some time -> ON_CLICK_2

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_4, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(0);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);  // should be ignored
  button.setMulticlickTime(300, true);

  button.onInit();
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(1, mock1, Supla::ON_CLICK_3);
  button.addAction(1, mock1, Supla::ON_CLICK_4);
  button.addAction(1, mock1, Supla::ON_CLICK_5);
  button.addAction(1, mock1, Supla::ON_CLICK_6);
  button.addAction(1, mock1, Supla::ON_CLICK_7);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);  // filtering
  button.onTimer();  // #3 click 1
  time.advance(60);  // debounce
  button.onTimer();  // #4
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(500);
  button.onTimer();  // #10 ON_CLICK_4
}

TEST(ButtonTests, OnHoldDoesntWorkOnBistableButton) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))   // #1 onInit
      .WillOnce(Return(1))   // #2 time 0 - first read
      .WillOnce(Return(1))   // #3 click 1
      .WillOnce(Return(1))   // #4 ON_HOLD shouldn't happen -
                             // there will be ON_CLICK_1 instead
      .WillOnce(Return(0))   // #5
      .WillOnce(Return(0))   // #6
      .WillOnce(Return(1))   // #7
      .WillOnce(Return(1))   // #8
      .WillOnce(Return(0))   // #9
      .WillOnce(Return(0))   // #10 release
      .WillOnce(Return(0));  // #11 some time -> no ON_CLICK_3

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_1, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_3, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(0);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);  // should be ignored
  button.setMulticlickTime(300, true);
  button.onInit();
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(1, mock1, Supla::ON_CLICK_3);
  button.addAction(1, mock1, Supla::ON_CLICK_4);
  button.addAction(1, mock1, Supla::ON_CLICK_5);
  button.addAction(1, mock1, Supla::ON_CLICK_6);
  button.addAction(1, mock1, Supla::ON_CLICK_7);

  time.advance(1000);
  button.onTimer();   // #2
  time.advance(30);   // filtering
  button.onTimer();   // #3 click 1
  time.advance(800);  //
  button.onTimer();   // #4 ON_HOLD
  time.advance(60);
  button.onTimer();
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(500);
  button.onTimer();  // #10 ON_CLICK_2
}

TEST(ButtonTests, MulticlickShouldSendEventAsap) {
  // we have configured on_click_2, so it should send that event on second
  // detected click and should not wait longer. Following clicks should be
  // ignored.
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 click 1 (ignored), conditional_on_press send
      .WillOnce(Return(0))  // #4
      .WillOnce(Return(0))  // #5 release, contional_on_release send
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7 click 2 (send)
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0))  // #9 release
      .WillOnce(Return(1))  // #10
      .WillOnce(Return(1))  // #11 click 3 (ignored)
      .WillOnce(Return(0))  // #12
      .WillOnce(Return(0))  // #13 release
      .WillOnce(Return(1))  // #14
      .WillOnce(Return(1))  // #15
      .WillOnce(Return(0))  // #16
      .WillOnce(Return(0))  // #17 release
      .WillOnce(Return(0));  // #18 some time -> ON_CLICK_2

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_2, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(0);

  // Conditional on_press and on_change are send only on first button press.
  // Conditional on_release is send, because there was multiclick detected.
  // Conditional on_change is send twice (on_press + on_release)
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_CHANGE, 10)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 9)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 11)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);
  button.setMulticlickTime(300);
  button.onInit();
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(9, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(10, mock1, Supla::CONDITIONAL_ON_CHANGE);
  button.addAction(11, mock1, Supla::CONDITIONAL_ON_RELEASE);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);  // filtering
  button.onTimer();  // #3 click 1
  time.advance(60);  // debounce
  button.onTimer();  // #4
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(100);
  button.onTimer();  // #10 ON_CLICK_2
  time.advance(100);
  button.onTimer();  // #11
  time.advance(100);
  button.onTimer();  // #12
  time.advance(100);
  button.onTimer();  // #13
  time.advance(100);
  button.onTimer();  // #14
  time.advance(100);
  button.onTimer();  // #15
  time.advance(100);
  button.onTimer();  // #16
  time.advance(100);
  button.onTimer();  // #17
  time.advance(100);
  button.onTimer();  // #18
}

TEST(ButtonTests, MotionSensorClicks) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 on press
      .WillOnce(Return(0))  // #4
      .WillOnce(Return(0))  // #5 release
      .WillOnce(Return(0))  // #6
      .WillOnce(Return(0))  // #7
      .WillOnce(Return(1))  // #8
      .WillOnce(Return(1))  // #9 on press
      .WillOnce(Return(0))  // #10
      .WillOnce(Return(0))  // #11 release
      .WillOnce(Return(1))  // #12
      .WillOnce(Return(1))  // #13 on press
      .WillOnce(Return(0))  // #14
      .WillOnce(Return(0))  // #15 release
      .WillOnce(Return(0));  // #16 some time -> ON_CLICK_4 - motion sensor
                             // counts in the same way as bistable input

  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 1)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 1)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_4, 1)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.setButtonType(Supla::Control::Button::ButtonType::MOTION_SENSOR);
  button.setHoldTime(700);  // should be ignored
  button.setMulticlickTime(300);

  button.onInit();
  button.addAction(1, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(1, mock1, Supla::CONDITIONAL_ON_RELEASE);
  button.addAction(1, mock1, Supla::ON_CLICK_4);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);  // filtering
  button.onTimer();  // #3 on press
  time.advance(60);  // debounce
  button.onTimer();  // #4
  time.advance(30);  // filtering
  button.onTimer();  // #5 on release
  time.advance(60);  // debounce
  button.onTimer();  // #6

  time.advance(1000);
  button.onTimer();  // #7
  time.advance(30);  //
  button.onTimer();  // #8
  time.advance(60);  // filtering
  button.onTimer();  // #9 on press
  time.advance(60);  // debounce
  button.onTimer();  // #10 release
  time.advance(60);  //
  button.onTimer();  // #11
  time.advance(60);  // filtering
  button.onTimer();  // #12   !!!!
  time.advance(60);  // debounce
  button.onTimer();  // #13   !!!!
  time.advance(60);  //
  button.onTimer();  // #14
  time.advance(60);  // filtering
  button.onTimer();  // #15
  time.advance(500);  // debounce
  button.onTimer();  // #16 on click 4
}
