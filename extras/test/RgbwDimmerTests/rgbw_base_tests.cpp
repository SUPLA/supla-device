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
#include <supla/control/rgbw_base.h>
#include <supla/actions.h>
#include <supla/storage/storage.h>

using ::testing::Return;

class RgbwBaseForTest : public Supla::Control::RGBWBase {
  public:
    MOCK_METHOD(void, setRGBWValueOnDevice, (uint32_t, uint32_t, uint32_t, uint32_t, uint32_t), (override));
};

class TimeInterfaceStub : public TimeInterface {
  public:
    virtual unsigned long millis() override {
      static unsigned long value = 0;
      value += 1000;
      return value;
    }
};

TEST(RgbwDimmerTests, InitializationWithDefaultValues) {
  TimeInterfaceMock time;

  EXPECT_CALL(time, millis)
    .WillOnce(Return(1))
    .WillOnce(Return(500));

  RgbwBaseForTest rgb;

  ASSERT_NE(rgb.getChannel(), nullptr);

  auto ch = rgb.getChannel();

  EXPECT_EQ(ch->getChannelType(), SUPLA_CHANNELTYPE_DIMMERANDRGBLED);
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.onInit();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

}


TEST(RgbwDimmerTests, FrequentSetCommandShouldNotChangeChannelValue) {
  TimeInterfaceMock time;

  EXPECT_CALL(time, millis)
    .WillOnce(Return(100))    // #1 setRgb (oninit)
    .WillOnce(Return(200))  // #1 iterateAlways
    .WillOnce(Return(200))  // #2 manual setRgb
    .WillOnce(Return(300))  // #2 iterateAlways
    .WillOnce(Return(300))  // #3 manual setRgb
    .WillOnce(Return(400))  // #3 iterateAlways
    .WillOnce(Return(400))  // #4 manual setRgb
    .WillOnce(Return(500))  // #4 iterateAlways
    .WillOnce(Return(500))  // #5 manual setRgb - should change channel value
    .WillOnce(Return(1000))  // #5 iterateAlways 
//    .WillOnce(Return(1100))  // #6 manual setRgb with toggle - it don't call millis
    .WillOnce(Return(1200))  // #6 iterateAlways 
    ;
  RgbwBaseForTest rgb;

  ASSERT_NE(rgb.getChannel(), nullptr);

  auto ch = rgb.getChannel();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  // #1
  rgb.onInit();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
  
  // #2
  rgb.setRGBW(1, 2, 3, 4, 5, false);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  // #3
  rgb.setRGBW(11, 12, 13, 14, 15, false);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  // #4
  rgb.setRGBW(21, 22, 23, 24, 25, false);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  // #5
  rgb.setRGBW(31, 32, 33, 34, 35, false);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 31);
  EXPECT_EQ(ch->getValueGreen(), 32);
  EXPECT_EQ(ch->getValueBlue(), 33);
  EXPECT_EQ(ch->getValueColorBrightness(), 34);
  EXPECT_EQ(ch->getValueBrightness(), 35);

  // #6 - with toggle
  rgb.setRGBW(41, 42, 43, 44, 45, true);

  EXPECT_EQ(ch->getValueRed(), 31);
  EXPECT_EQ(ch->getValueGreen(), 32);
  EXPECT_EQ(ch->getValueBlue(), 33);
  EXPECT_EQ(ch->getValueColorBrightness(), 34);
  EXPECT_EQ(ch->getValueBrightness(), 35);

  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 41);
  EXPECT_EQ(ch->getValueGreen(), 42);
  EXPECT_EQ(ch->getValueBlue(), 43);
  EXPECT_EQ(ch->getValueColorBrightness(), 44);
  EXPECT_EQ(ch->getValueBrightness(), 45);
}

TEST(RgbwDimmerTests, SetValueFromServer) {
  TimeInterfaceMock time;

  EXPECT_CALL(time, millis)
    .WillOnce(Return(1))
    .WillOnce(Return(500))
    .WillOnce(Return(500))
    .WillOnce(Return(1000))
    .WillOnce(Return(2000))
    .WillOnce(Return(3000))
    .WillOnce(Return(4000))
    .WillOnce(Return(5000))
    ;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  rgb.onInit();
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  TSD_SuplaChannelNewValue msg = {};
  msg.value[5] = 0; // turn on/off
  msg.value[4] = 1; // red
  msg.value[3] = 2; // green
  msg.value[2] = 3; // blue
  msg.value[1] = 4; // colorBrightness
  msg.value[0] = 5; // brightness
  rgb.handleNewValueFromServer(&msg);

  // channel values should be changed only after some time passed
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.iterateAlways();

  // a little bit later, values are set to channel
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

// with toggle - should turn off dimmer
  msg.value[5] = 1;
  msg.value[0] = 0;
  rgb.handleNewValueFromServer(&msg);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 0);

// with toggle - should turn on dimmer and restore last brightness
  msg.value[5] = 1;
  msg.value[0] = 100;
  rgb.handleNewValueFromServer(&msg);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

// with toggle - should turn off rgb
  msg.value[5] = 1;
  msg.value[1] = 0;
  msg.value[0] = 5; // restore brightness so it is consistant with last value
  rgb.handleNewValueFromServer(&msg);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 5);

// with toggle - should turn on dimmer and restore last brightness
  msg.value[5] = 1;
  msg.value[1] = 100;
  rgb.handleNewValueFromServer(&msg);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

}

TEST(RgbwDimmerTests, TurnOnOffToggleTests) {
  TimeInterfaceMock time;

  EXPECT_CALL(time, millis)
    .WillOnce(Return(1))
    .WillOnce(Return(500))
    .WillOnce(Return(1000))
    .WillOnce(Return(2000))
    .WillOnce(Return(3000))
    .WillOnce(Return(4000))
    .WillOnce(Return(5000))
    .WillOnce(Return(6000))
    .WillOnce(Return(7000))
    .WillOnce(Return(8000))
    .WillOnce(Return(9000))
    .WillOnce(Return(10000))
    ;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  rgb.onInit();
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.turnOn();
  // channel values should be changed only after some time passed
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.iterateAlways();

  // a little bit later, values are set to channel
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.turnOff();
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.setRGBW(0, 255, 0, 50, 40, false);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 50);
  EXPECT_EQ(ch->getValueBrightness(), 40);

  rgb.toggle();
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.toggle();
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 50);
  EXPECT_EQ(ch->getValueBrightness(), 40);
}

TEST(RgbwDimmerTests, HandleActionTests) {
  // time stub will return +1000 ms on each call to millis
  TimeInterfaceStub time;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  rgb.setStep(10);
  rgb.onInit();
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TURN_ON);
  // channel values should be changed only after some time passed
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.iterateAlways();

  // a little bit later, values are set to channel
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::TURN_OFF);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::BRIGHTEN_ALL);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  rgb.handleAction(1, Supla::BRIGHTEN_ALL);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::TOGGLE);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TOGGLE);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::DIM_ALL);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  rgb.handleAction(1, Supla::DIM_ALL);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::DIM_ALL);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TURN_ON);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  for (int i = 0; i < 20; i++) {
    rgb.handleAction(1, Supla::BRIGHTEN_ALL);
  };
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::BRIGHTEN_R);
  rgb.handleAction(1, Supla::BRIGHTEN_R);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 20);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_R);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_G);
  rgb.handleAction(1, Supla::DIM_G);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 235);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::BRIGHTEN_G);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);


  rgb.handleAction(1, Supla::BRIGHTEN_B);
  rgb.handleAction(1, Supla::BRIGHTEN_B);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 20);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_B);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_W);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::DIM_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::BRIGHTEN_W);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::BRIGHTEN_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_ALL);
  rgb.handleAction(1, Supla::TURN_OFF_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_ON_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_OFF_W);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TURN_ON_W);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TOGGLE_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TOGGLE_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TOGGLE_W);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TOGGLE_W);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_ON_RGB_DIMMED);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_ON_ALL_DIMMED);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_OFF);
  rgb.handleAction(1, Supla::TURN_ON_RGB_DIMMED);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::TURN_OFF);
  rgb.handleAction(1, Supla::TURN_ON_ALL_DIMMED);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 30);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 40);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  for (int i = 0; i < 6; i++) {
    rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  }
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  // after hitting 100%, iterate should stay 4 iterations before it goes down
  for (int i = 0; i < 4; i++) {
    rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  }
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  // next five iterations should change brightness to 50
  for (int i = 0; i < 5; i++) {
    rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  }
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 50);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  // next six iterations should change brightness to 5
  for (int i = 0; i < 6; i++) {
    rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  }
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 5);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  // next four iterations should keep 5
  for (int i = 0; i < 4; i++) {
    rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  }
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 5);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 15);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  // turn off
  rgb.handleAction(1, Supla::TURN_OFF);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  // after turn off, it should start from dimmed value
  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::ITERATE_DIM_W);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  rgb.handleAction(1, Supla::ITERATE_DIM_W);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  for (int i = 0; i < 12; i++) {
    rgb.handleAction(1, Supla::ITERATE_DIM_W);
  }
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  // if we iterate all, colorBrightness is copied to brightness and it operated on both values in sync
  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 30);
  EXPECT_EQ(ch->getValueBrightness(), 30);
  
  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 40);
  EXPECT_EQ(ch->getValueBrightness(), 40);
}


TEST(RgbwDimmerTests, DefaultDimmedValue) {
  // time stub will return +1000 ms on each call to millis
  TimeInterfaceStub time;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  rgb.onInit();
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TURN_ON_ALL_DIMMED);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::TURN_OFF);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.setDefaultDimmedBrightness(64);
  rgb.handleAction(1, Supla::TURN_ON_ALL_DIMMED);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 64);
  EXPECT_EQ(ch->getValueBrightness(), 64);
}

TEST(RgbwDimmerTests, IterationSteps) {
  // time stub will return +1000 ms on each call to millis
  TimeInterfaceStub time;

  RgbwBaseForTest rgb;
  rgb.setStep(10);

  auto ch = rgb.getChannel();

  rgb.onInit();
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.setStep(5);
  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 25);
  EXPECT_EQ(ch->getValueBrightness(), 25);
}

TEST(RgbwDimmerTests, SetValueOnDeviceWithoutFading) {
  // time stub will return +1000 ms on each call to millis
  TimeInterfaceStub time;
  ::testing::InSequence seq;

  RgbwBaseForTest rgb;
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 1023, 1023));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 1023, (20*1023/100)));


  auto ch = rgb.getChannel();

  // disable fade effect - so value setting on device should happen instantly
  rgb.setFadeEffectTime(0);
  rgb.onInit();
  rgb.onTimer();
  rgb.turnOn();
  rgb.onTimer();
  rgb.toggle();
  rgb.onTimer();
  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  rgb.onTimer();
  rgb.turnOff();
  rgb.onTimer();
  rgb.turnOn();
  rgb.onTimer();

  // channel value should be still empty, since no time elapsed (no calls to iterateAlways)
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
}

TEST(RgbwDimmerTests, SetValueOnDeviceWithFading) {
  // time stub will return +1000 ms on each call to millis
  TimeInterfaceStub time;
  ::testing::InSequence seq;

  RgbwBaseForTest rgb;

  // fade effect 1000 ms, time step 1000 ms
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 1023, 1023));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 1023, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));

  // fade effect 10000 ms, time step 1000 ms
  // because turnOn calls millis once, we actually do 2 s step in first shot
  // that is why (0, 255, 0, 10, 10) is missing
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 204, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 306, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 408, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 510, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 612, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 714, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 816, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 918, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 1020, (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 1023, (20*1023/100)));

  // fade effect 10000 ms, time step 1000 ms
  // because turnOn calls millis once, we actually do 2 s step in first shot
  // setting rgb values
  EXPECT_CALL(rgb, setRGBWValueOnDevice(204, 818, 0, (100*1023/100), (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(306, 802, 0, (100*1023/100), (20*1023/100)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(401, (1023*200/255), 0, (100*1023/100), (20*1023/100)));

  auto ch = rgb.getChannel();

  // time stub gives +1000 ms on each call to millis, and fade time is 1000 ms,
  // so it should set value on device as it is
  rgb.onInit();
  rgb.onTimer();
  rgb.turnOn();
  rgb.onTimer();
  rgb.toggle();
  rgb.onTimer();
  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  rgb.onTimer();
  rgb.turnOff();
  rgb.onTimer();
  rgb.turnOn();
  rgb.onTimer();
  rgb.turnOff();
  rgb.onTimer();

  // change fade effect to 10000 ms, so we'll get 1/10 steps
  rgb.setFadeEffectTime(10000);
  rgb.turnOn();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();

  rgb.setRGBW(100, 200, 0, -1, -1, false);
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();
  rgb.onTimer();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
}

TEST(RgbwDimmerTests, MinAndMaxLimits) {
  // time stub will return +1000 ms on each call to millis
  TimeInterfaceStub time;
  ::testing::InSequence seq;

  RgbwBaseForTest rgb;
  rgb.setBrightnessLimits(100, 500)
    .setColorBrightnessLimits(600, 700);

  // fade effect 1000 ms, time step 1000 ms
  // Limits: brightness (100, 500), colorBrightness (600, 700)
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 600, 100));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 700, 500));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 600, 100));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 600, (20*4 + 100 - 1)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 600, 100));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 700, (20*4 + 100 - 1)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 600, 100));

  auto ch = rgb.getChannel();

  // time stub gives +1000 ms on each call to millis, and fade time is 1000 ms,
  // so it should set value on device as it is
  rgb.onInit();
  rgb.onTimer();  // off
  rgb.turnOn();
  rgb.onTimer();  // on
  rgb.toggle();
  rgb.onTimer();  // toggle -> off
  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  rgb.onTimer();  // white ON
  rgb.turnOff();
  rgb.onTimer();  // off
  rgb.turnOn();
  rgb.onTimer();  // ON
  rgb.turnOff();
  rgb.onTimer();  // OFF

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
}

class StorageMock: public Supla::Storage {
 public:
  MOCK_METHOD(void, scheduleSave, (uint64_t), (override));
  MOCK_METHOD(void, commit, (), (override));
  MOCK_METHOD(int, readStorage, (unsigned int, unsigned char *, int, bool), (override));
  MOCK_METHOD(int, writeStorage, (unsigned int, const unsigned char *, int), (override));
  MOCK_METHOD(bool, readState, (unsigned char *, int), (override));
  MOCK_METHOD(bool, writeState, (const unsigned char *, int), (override));

};

using ::testing::_;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::Pointee;

TEST(RgbwDimmerTests, RgbwStorageTests) {
  // time stub will return +1000 ms on each call to millis
  TimeInterfaceStub time;
  ::testing::InSequence seq;

  StorageMock storage;
  RgbwBaseForTest rgb;

  // setRGBW should call scheduleSave on storage once
  EXPECT_CALL(storage, scheduleSave(5000));

  uint8_t red = 1;
  uint8_t green = 2;
  uint8_t blue = 3;
  uint8_t colorBrightness = 4;
  uint8_t brightness = 5;
  uint8_t lastColorBrightness = 6;
  uint8_t lastBrightness = 7;

  // onLoadState expectations
  EXPECT_CALL(storage, readState(_, 1))
     .WillOnce(DoAll(SetArgPointee<0>(red), Return(true)))
     .WillOnce(DoAll(SetArgPointee<0>(green), Return(true)))
     .WillOnce(DoAll(SetArgPointee<0>(blue), Return(true)))
     .WillOnce(DoAll(SetArgPointee<0>(colorBrightness), Return(true)))
     .WillOnce(DoAll(SetArgPointee<0>(brightness), Return(true)))
     .WillOnce(DoAll(SetArgPointee<0>(lastColorBrightness), Return(true)))
     .WillOnce(DoAll(SetArgPointee<0>(lastBrightness), Return(true)))
     ;

  // onSaveState expectations
  EXPECT_CALL(storage, writeState(Pointee(red), 1));
  EXPECT_CALL(storage, writeState(Pointee(green), 1));
  EXPECT_CALL(storage, writeState(Pointee(blue), 1));
  EXPECT_CALL(storage, writeState(Pointee(colorBrightness), 1));
  EXPECT_CALL(storage, writeState(Pointee(brightness), 1));
  EXPECT_CALL(storage, writeState(Pointee(lastColorBrightness), 1));
  EXPECT_CALL(storage, writeState(Pointee(lastBrightness), 1));

  rgb.setRGBW(1, 2, 3, 4, 5, false);

  rgb.onLoadState();
  rgb.onSaveState();

}

