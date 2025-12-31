/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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
#include <supla/control/rgb_cct_base.h>
#include <supla/actions.h>
#include <storage_mock.h>
#include <simple_time.h>

using ::testing::Return;
using ::testing::_;
using ::testing::Le;

class RgbCctBaseForTest : public Supla::Control::RGBCCTBase {
 public:
  MOCK_METHOD(void,
              setRGBCCTValueOnDevice,
              (uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t),
              (override));
};

void setRGBCCTValues(TRGBW_Value *value,
               int red,
               int green,
               int blue,
               int colorBrightness,
               int brightness,
               int whiteTemperature,
               int onOff,
               int command) {
  value->onOff = onOff;
  value->command = command;
  value->R = red;
  value->G = green;
  value->B = blue;
  value->colorBrightness = colorBrightness;
  value->brightness = brightness;
  value->dimmerCct = whiteTemperature;
}

TEST(RgbCctTests, InitializationWithDefaultValues) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;

  RgbCctBaseForTest rgb;

  ASSERT_NE(rgb.getChannel(), nullptr);

  auto ch = rgb.getChannel();
  EXPECT_EQ(ch->getChannelType(), SUPLA_CHANNELTYPE_DIMMERANDRGBLED);
  EXPECT_EQ(ch->getDefaultFunction(), SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB);
  EXPECT_EQ(ch->getFlags(), SUPLA_CHANNEL_FLAG_CHANNELSTATE |
      SUPLA_CHANNEL_FLAG_RGBW_COMMANDS_SUPPORTED |
      SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 0);

  time.advance(500);
  rgb.onInit();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 0);

  time.advance(500);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 0);
}

TEST(RgbCctTests, BasicTests) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;

  RgbCctBaseForTest rgb;

  auto ch = rgb.getChannel();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 0);

  time.advance(500);
  rgb.onInit();

  time.advance(500);
  rgb.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 0);

  rgb.turnOn();
  time.advance(500);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 100);
  EXPECT_EQ(ch->getValueBrightness(), 100);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 0);

  rgb.turnOff();
  time.advance(500);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 0);

  TSD_SuplaChannelNewValue msg = {};
  TRGBW_Value *rgbwValue = reinterpret_cast<TRGBW_Value *>(msg.value);
  setRGBCCTValues(rgbwValue,
                  1,
                  2,
                  3,
                  5,
                  5,
                  100,
                  0,
                  RGBW_COMMAND_SET_DIMMER_CCT_WITHOUT_TURN_ON);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 100);

  setRGBCCTValues(rgbwValue,
                  1,
                  2,
                  3,
                  5,
                  5,
                  33,
                  0,
                  RGBW_COMMAND_SET_DIMMER_CCT_WITHOUT_TURN_ON);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 33);

  setRGBCCTValues(rgbwValue,
                  0,
                  0,
                  0,
                  0,
                  100,
                  0,
                  0,
                  RGBW_COMMAND_TURN_ON_DIMMER);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 255);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 100);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 33);

  setRGBCCTValues(rgbwValue,
                  10,
                  20,
                  30,
                  40,
                  50,
                  60,
                  0,
                  0);
  rgb.handleNewValueFromServer(&msg);
  time.advance(1000);
  rgb.iterateAlways();
  EXPECT_EQ(ch->getValueRed(), 10);
  EXPECT_EQ(ch->getValueGreen(), 20);
  EXPECT_EQ(ch->getValueBlue(), 30);
  EXPECT_EQ(ch->getValueColorBrightness(), 40);
  EXPECT_EQ(ch->getValueBrightness(), 50);
  EXPECT_EQ(ch->getValueWhiteTemperature(), 60);
}

