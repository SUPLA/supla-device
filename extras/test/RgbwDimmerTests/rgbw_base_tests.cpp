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
#include <simple_time.h>

using ::testing::Return;
using ::testing::_;
using ::testing::Le;

class RgbwBaseForTest : public Supla::Control::RGBWBase {
 public:
  MOCK_METHOD(void,
              setRGBWValueOnDevice,
              (uint32_t, uint32_t, uint32_t, uint32_t, uint32_t),
              (override));
};

void setValues(TRGBW_Value *value,
               int red,
               int green,
               int blue,
               int colorBrightness,
               int brightness,
               int onOff,
               int command) {
  value->onOff = onOff;
  value->command = command;
  value->R = red;
  value->G = green;
  value->B = blue;
  value->colorBrightness = colorBrightness;
  value->brightness = brightness;
}

TEST(RgbwDimmerTests, InitializationWithDefaultValues) {
  SimpleTime time;

  RgbwBaseForTest rgb;

  ASSERT_NE(rgb.getChannel(), nullptr);

  auto ch = rgb.getChannel();
  EXPECT_EQ(ch->getChannelType(), SUPLA_CHANNELTYPE_DIMMERANDRGBLED);
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING);
  EXPECT_EQ(ch->getFlags(), SUPLA_CHANNEL_FLAG_CHANNELSTATE |
      SUPLA_CHANNEL_FLAG_RGBW_COMMANDS_SUPPORTED);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(500);
  rgb.onInit();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(500);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
}

TEST(RgbwDimmerTests, FrequentSetCommandShouldNotChangeChannelValue) {
  SimpleTime time;

  RgbwBaseForTest rgb;

  ASSERT_NE(rgb.getChannel(), nullptr);

  auto ch = rgb.getChannel();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  // #1
  time.advance(100);
  rgb.onInit();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(100);
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

  time.advance(100);
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

  time.advance(100);
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

  time.advance(100);
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

  time.advance(500);
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

  time.advance(200);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 41);
  EXPECT_EQ(ch->getValueGreen(), 42);
  EXPECT_EQ(ch->getValueBlue(), 43);
  EXPECT_EQ(ch->getValueColorBrightness(), 44);
  EXPECT_EQ(ch->getValueBrightness(), 45);
}

TEST(RgbwDimmerTests, SetValueFromServer) {
  SimpleTime time;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  time.advance(100);
  rgb.onInit();
  time.advance(500);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  TSD_SuplaChannelNewValue msg = {};
  msg.value[5] = 0;  // turn on/off
  msg.value[4] = 1;  // red
  msg.value[3] = 2;  // green
  msg.value[2] = 3;  // blue
  msg.value[1] = 4;  // colorBrightness
  msg.value[0] = 5;  // brightness
  rgb.handleNewValueFromServer(&msg);

  // channel values should be changed only after some time passed
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(1000);
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
  time.advance(1000);
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
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

// with toggle - should turn off rgb
  msg.value[5] = 1;
  msg.value[1] = 0;
  msg.value[0] = 5;  // restore brightness so it is consistant with last value
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
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
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);
}

TEST(RgbwDimmerTests, TurnOnOffToggleTests) {
  SimpleTime time;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  time.advance(100);
  rgb.onInit();
  time.advance(1000);
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

  time.advance(1000);
  rgb.iterateAlways();

  // a little bit later, values are set to channel
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.turnOff();
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.setRGBW(0, 255, 0, 50, 40, false);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 50);
  EXPECT_EQ(ch->getValueBrightness(), 40);

  rgb.toggle();
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.toggle();
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 50);
  EXPECT_EQ(ch->getValueBrightness(), 40);
}

TEST(RgbwDimmerTests, HandleActionTests) {
  SimpleTime time;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  time.advance(1000);
  rgb.setStep(10);
  rgb.setMinIterationBrightness(10);
  rgb.setMinMaxIterationDelay(400);
  rgb.onInit();
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(1000);
  rgb.handleAction(1, Supla::TURN_ON);
  // channel values should be changed only after some time passed
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(1000);
  rgb.iterateAlways();
  //  rgb.onFastTimer();

  // a little bit later, values are set to channel
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::TURN_OFF);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::BRIGHTEN_ALL);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  rgb.handleAction(1, Supla::BRIGHTEN_ALL);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::TOGGLE);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TOGGLE);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::DIM_ALL);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  rgb.handleAction(1, Supla::DIM_ALL);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::DIM_ALL);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TURN_ON);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  for (int i = 0; i < 20; i++) {
    time.advance(100);
    rgb.handleAction(1, Supla::BRIGHTEN_ALL);
  }

  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::BRIGHTEN_R);
  time.advance(1000);
  rgb.handleAction(1, Supla::BRIGHTEN_R);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 20);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_R);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_G);
  time.advance(1000);
  rgb.handleAction(1, Supla::DIM_G);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 235);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::BRIGHTEN_G);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);


  rgb.handleAction(1, Supla::BRIGHTEN_B);
  time.advance(1000);
  rgb.handleAction(1, Supla::BRIGHTEN_B);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 20);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_B);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_W);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::DIM_RGB);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::BRIGHTEN_W);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::BRIGHTEN_RGB);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  rgb.handleAction(1, Supla::DIM_ALL);
  time.advance(1000);
  rgb.handleAction(1, Supla::TURN_OFF_RGB);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_ON_RGB);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_OFF_W);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TURN_ON_W);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TOGGLE_RGB);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TOGGLE_RGB);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TOGGLE_W);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TOGGLE_W);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_ON_RGB_DIMMED);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_ON_ALL_DIMMED);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 90);
  EXPECT_EQ(ch->getValueBrightness(), 90);

  rgb.handleAction(1, Supla::TURN_OFF);
  time.advance(1000);
  rgb.handleAction(1, Supla::TURN_ON_RGB_DIMMED);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::TURN_OFF);
  time.advance(1000);
  rgb.handleAction(1, Supla::TURN_ON_ALL_DIMMED);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);     ////////////
  EXPECT_EQ(ch->getValueColorBrightness(), 30);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  for (int i = 0; i < 8; i++) {
    time.advance(100);
    rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  }
  time.advance(2000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  // next five iterations should change brightness to 50
  for (int i = 0; i < 5; i++) {
    time.advance(50);
    rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  }
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 50);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  // next 4 iterations should change brightness to 10
  for (int i = 0; i < 4; i++) {
    time.advance(50);
    rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  }
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  // next four iterations (200 ms) should keep 10
  for (int i = 0; i < 4; i++) {
    time.advance(50);
    rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  }

  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  // turn off
  rgb.handleAction(1, Supla::TURN_OFF);
  time.advance(1000);
  rgb.iterateAlways();
  time.advance(500);
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  // after turn off, it should start from dimmed value
  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::ITERATE_DIM_RGB);
  time.advance(100);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::ITERATE_DIM_W);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  rgb.handleAction(1, Supla::ITERATE_DIM_W);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  for (int i = 0; i < 12; i++) {
    time.advance(50);
    rgb.handleAction(1, Supla::ITERATE_DIM_W);
  }
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  // if we iterate all, colorBrightness is copied to brightness and it operated
  // on both values in sync
  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 30);
  EXPECT_EQ(ch->getValueBrightness(), 30);

  rgb.handleAction(1, Supla::TURN_OFF_W);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 30);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TOGGLE);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TOGGLE);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 30);
  EXPECT_EQ(ch->getValueBrightness(), 30);
rgb.handleAction(1, Supla::TURN_OFF_RGB);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 30);

  rgb.handleAction(1, Supla::TOGGLE);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);


  rgb.handleAction(1, Supla::TURN_ON_RGB);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 30);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TOGGLE);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 245);
  EXPECT_EQ(ch->getValueBlue(), 10);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
}


TEST(RgbwDimmerTests, DefaultDimmedValue) {
  SimpleTime time;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  time.advance(1000);
  rgb.onInit();
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::TURN_ON_ALL_DIMMED);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.handleAction(1, Supla::TURN_OFF);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.setDefaultDimmedBrightness(64);
  rgb.handleAction(1, Supla::TURN_ON_ALL_DIMMED);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 64);
  EXPECT_EQ(ch->getValueBrightness(), 64);
}

TEST(RgbwDimmerTests, IterationSteps) {
  SimpleTime time;

  RgbwBaseForTest rgb;
  rgb.setStep(10);
  rgb.setMinIterationBrightness(10);

  auto ch = rgb.getChannel();

  time.advance(1000);
  rgb.onInit();
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  time.advance(400);
  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 10);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 20);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  rgb.setStep(5);
  rgb.handleAction(1, Supla::ITERATE_DIM_ALL);
  time.advance(400);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 25);
  EXPECT_EQ(ch->getValueBrightness(), 25);
}

TEST(RgbwDimmerTests, SetValueOnDeviceWithoutFading) {
  SimpleTime time;
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
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  rgb.turnOn();
  time.advance(1000);
  rgb.onFastTimer();
  rgb.toggle();
  time.advance(1000);
  rgb.onFastTimer();
  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  time.advance(1000);
  rgb.onFastTimer();
  rgb.turnOff();
  time.advance(1000);
  rgb.onFastTimer();
  rgb.turnOn();
  time.advance(1000);
  rgb.onFastTimer();

  // channel value should be still empty, since no time elapsed (no calls to
  // iterateAlways)
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
}

TEST(RgbwDimmerTests, SetValueOnDeviceWithFading) {
  SimpleTime time;
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
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 102, (10*1023/100)));
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
  EXPECT_CALL(
      rgb,
      setRGBWValueOnDevice(102, 921, 0, (100 * 1023 / 100), (20 * 1023 / 100)));
  EXPECT_CALL(
      rgb,
      setRGBWValueOnDevice(204, 819, 0, (100 * 1023 / 100), (20 * 1023 / 100)));
  EXPECT_CALL(
      rgb,
      setRGBWValueOnDevice(306, 802, 0, (100 * 1023 / 100), (20 * 1023 / 100)));
  EXPECT_CALL(
      rgb,
      setRGBWValueOnDevice(
          401, (1023 * 200 / 255), 0, (100 * 1023 / 100), (20 * 1023 / 100)));

  auto ch = rgb.getChannel();

  // time stub gives +1000 ms on each call, and fade time is 1000 ms,
  // so it should set value on device as it is
  rgb.onInit();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  rgb.turnOn();
  time.advance(1000);
  rgb.onFastTimer();
  rgb.toggle();
  time.advance(1000);
  rgb.onFastTimer();
  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  time.advance(1000);
  rgb.onFastTimer();
  rgb.turnOff();
  time.advance(1000);
  rgb.onFastTimer();
  rgb.turnOn();
  time.advance(1000);
  rgb.onFastTimer();
  rgb.turnOff();
  time.advance(1000);
  rgb.onFastTimer();

  // change fade effect to 10000 ms, so we'll get 1/10 steps
  rgb.setFadeEffectTime(10000);
  rgb.turnOn();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();

  rgb.setRGBW(100, 200, 0, -1, -1, false);
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
}

TEST(RgbwDimmerTests, MinAndMaxLimits) {
  SimpleTime time;
  ::testing::InSequence seq;

  RgbwBaseForTest rgb;
  rgb.setBrightnessLimits(100, 500);
  rgb.setColorBrightnessLimits(600, 700);

  // fade effect 1000 ms, time step 1000 ms
  // Limits: brightness (100, 500), colorBrightness (600, 700)
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 700, 500));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, (20*4 + 100 - 1)));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 700, (20*4 + 100 - 1)));
  EXPECT_CALL(
      rgb,
      setRGBWValueOnDevice(0, 1023, 0, Le(700), Le((20 * 4 + 100 - 1))));
  EXPECT_CALL(rgb, setRGBWValueOnDevice(0, 1023, 0, 0, 0));

  auto ch = rgb.getChannel();

  // time stub gives +1000 ms on each call to millis, and fade time is 1000 ms,
  // so it should set value on device as it is
  time.advance(1000);
  rgb.onInit();
  time.advance(1000);
  rgb.onFastTimer();
  time.advance(1000);
  rgb.onFastTimer();  // off
  rgb.turnOn();
  time.advance(1000);
  rgb.onFastTimer();  // on
  rgb.toggle();
  time.advance(1000);
  rgb.onFastTimer();  // toggle -> off
  rgb.handleAction(1, Supla::TURN_ON_W_DIMMED);
  time.advance(1000);
  rgb.onFastTimer();  // white ON
  rgb.turnOff();
  time.advance(1000);
  rgb.onFastTimer();  // off
  rgb.turnOn();
  time.advance(1000);
  rgb.onFastTimer();  // ON
  rgb.turnOff();
  time.advance(1);
  rgb.onFastTimer();  // turning OFF...
  time.advance(1000);
  rgb.onFastTimer();  // OFF (full)

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
}

class StorageMock : public Supla::Storage {
 public:
  MOCK_METHOD(void, scheduleSave, (uint32_t), (override));
  MOCK_METHOD(void, commit, (), (override));
  MOCK_METHOD(int,
              readStorage,
              (unsigned int, unsigned char *, int, bool),
              (override));
  MOCK_METHOD(int,
              writeStorage,
              (unsigned int, const unsigned char *, int),
              (override));
  MOCK_METHOD(bool, readState, (unsigned char *, int), (override));
  MOCK_METHOD(bool, writeState, (const unsigned char *, int), (override));
};

using ::testing::_;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::Pointee;

TEST(RgbwDimmerTests, RgbwStorageTests) {
  SimpleTime time;
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

// TurnOnOff flag from server (msg[5]) can have value 1, 2, or 3, which in
// practice should have exactly the same result on rgbw channel
TEST(RgbwDimmerTests, SetValueFromServerTurnOnOff) {
  SimpleTime time;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  time.advance(100);
  rgb.onInit();
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  TSD_SuplaChannelNewValue msg = {};
  TRGBW_Value *rgbwValue = reinterpret_cast<TRGBW_Value *>(msg.value);
  rgbwValue->onOff = 0;  // turn on/off
  rgbwValue->R = 1;  // red
  rgbwValue->G = 2;  // green
  rgbwValue->B = 3;  // blue
  rgbwValue->colorBrightness = 4;  // colorBrightness
  rgbwValue->brightness = 5;  // brightness
  rgb.handleNewValueFromServer(&msg);

  // channel values should be changed only after some time passed
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(1000);
  rgb.iterateAlways();

  // a little bit later, values are set to channel
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

// with toggle - should turn off dimmer
  rgbwValue->onOff = 1;
  rgbwValue->brightness = 0;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 0);

// with toggle - should turn on dimmer and restore last brightness
  rgbwValue->onOff = 2;
  rgbwValue->brightness = 100;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

// with toggle - should turn off rgb
  rgbwValue->onOff = 3;
  rgbwValue->colorBrightness = 0;
  rgbwValue->brightness =
      5;  // restore brightness so it is consistant with last value
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 5);

// with toggle - should turn on dimmer and restore last brightness
  rgbwValue->onOff = 1;
  rgbwValue->colorBrightness = 100;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

  rgbwValue->onOff = 2;
  rgbwValue->colorBrightness = 0;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 5);

  rgbwValue->onOff = 3;
  rgbwValue->colorBrightness = 100;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

  rgbwValue->onOff = 1;
  rgbwValue->colorBrightness = 0;
  rgbwValue->brightness = 0;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgbwValue->onOff = 2;
  rgbwValue->colorBrightness = 100;
  rgbwValue->brightness = 100;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

  rgbwValue->onOff = 3;
  rgbwValue->colorBrightness = 100;
  rgbwValue->brightness = 0;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  rgbwValue->onOff = 2;
  rgbwValue->colorBrightness = 100;
  rgbwValue->brightness = 100;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);

  rgbwValue->onOff = 2;
  rgbwValue->colorBrightness = 100;
  rgbwValue->brightness = 100;
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 4);
  EXPECT_EQ(ch->getValueBrightness(), 5);
}

// RGBW commands
TEST(RgbwDimmerTests, RgbwCommandsTest) {
  SimpleTime time;

  RgbwBaseForTest rgb;

  auto ch = rgb.getChannel();

  time.advance(100);
  rgb.onInit();
  time.advance(1000);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  TSD_SuplaChannelNewValue msg = {};
  TRGBW_Value *rgbwValue = reinterpret_cast<TRGBW_Value *>(msg.value);
  setValues(
      rgbwValue, 1, 2, 3, 5, 5, 0, RGBW_COMMAND_SET_RGB_WITHOUT_TURN_ON);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  setValues(
      rgbwValue, 0, 0, 0, 50, 50, 0, RGBW_COMMAND_TURN_ON_DIMMER);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  setValues(
      rgbwValue, 0, 0, 0, 50, 50, 0, RGBW_COMMAND_TURN_ON_RGB);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  setValues(
      rgbwValue, 0, 0, 0, 50, 50, 0, RGBW_COMMAND_TURN_OFF_DIMMER);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  setValues(
      rgbwValue, 0, 0, 0, 50, 50, 0, RGBW_COMMAND_TOGGLE_ALL);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  setValues(
      rgbwValue, 0, 0, 0, 50, 50, 0, RGBW_COMMAND_TOGGLE_ALL);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);

  setValues(
      rgbwValue, 0, 0, 0, 50, 50, 0, RGBW_COMMAND_TOGGLE_ALL);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  setValues(
    rgbwValue, 0, 0, 0, 50, 50, 0, RGBW_COMMAND_SET_BRIGHTNESS_WITHOUT_TURN_ON);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  setValues(
    rgbwValue, 1, 1, 1, 1, 1, 0, RGBW_COMMAND_TURN_ON_ALL);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 50);

  setValues(
    rgbwValue, 0, 0, 0, 50, 60, 0, RGBW_COMMAND_SET_BRIGHTNESS_WITHOUT_TURN_ON);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 60);

  setValues(rgbwValue,
            0,
            0,
            0,
            15,
            0,
            0,
            RGBW_COMMAND_SET_COLOR_BRIGHTNESS_WITHOUT_TURN_ON);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 15);
  EXPECT_EQ(ch->getValueBrightness(), 60);

  setValues(
      rgbwValue, 0, 0, 0, 0, 0, 0, RGBW_COMMAND_TOGGLE_ALL);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  setValues(rgbwValue,
            0,
            0,
            0,
            95,
            0,
            0,
            RGBW_COMMAND_SET_COLOR_BRIGHTNESS_WITHOUT_TURN_ON);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  setValues(
    rgbwValue, 1, 1, 1, 1, 1, 0, RGBW_COMMAND_TURN_ON_ALL);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 1);
  EXPECT_EQ(ch->getValueGreen(), 2);
  EXPECT_EQ(ch->getValueBlue(), 3);
  EXPECT_EQ(ch->getValueColorBrightness(), 95);
  EXPECT_EQ(ch->getValueBrightness(), 60);

  setValues(
    rgbwValue, 10, 20, 30, 1, 1, 0, RGBW_COMMAND_SET_RGB_WITHOUT_TURN_ON);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 20);
  EXPECT_EQ(ch->getValueBlue(), 30);
  EXPECT_EQ(ch->getValueColorBrightness(), 95);
  EXPECT_EQ(ch->getValueBrightness(), 60);

  setValues(
    rgbwValue, 10, 20, 30, 1, 1, 0, RGBW_COMMAND_TOGGLE_DIMMER);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 20);
  EXPECT_EQ(ch->getValueBlue(), 30);
  EXPECT_EQ(ch->getValueColorBrightness(), 95);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  setValues(
    rgbwValue, 10, 20, 30, 1, 1, 0, RGBW_COMMAND_TOGGLE_DIMMER);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 20);
  EXPECT_EQ(ch->getValueBlue(), 30);
  EXPECT_EQ(ch->getValueColorBrightness(), 95);
  EXPECT_EQ(ch->getValueBrightness(), 60);

  setValues(
    rgbwValue, 10, 20, 30, 1, 1, 0, RGBW_COMMAND_TOGGLE_RGB);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 20);
  EXPECT_EQ(ch->getValueBlue(), 30);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 60);

  setValues(
    rgbwValue, 10, 20, 30, 1, 1, 0, RGBW_COMMAND_TOGGLE_RGB);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 20);
  EXPECT_EQ(ch->getValueBlue(), 30);
  EXPECT_EQ(ch->getValueColorBrightness(), 95);
  EXPECT_EQ(ch->getValueBrightness(), 60);
}
