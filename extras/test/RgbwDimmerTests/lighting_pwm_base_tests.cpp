// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <arduino_mock.h>
#include <supla/control/lighting_pwm_base.h>
#include <supla/actions.h>
#include <storage_mock.h>
#include <simple_time.h>

using ::testing::Return;
using ::testing::_;
using ::testing::Le;

class RgbCctBaseForTest : public Supla::Control::LightingPwmBase {
 public:
  MOCK_METHOD(void,
              setRGBCCTValueOnDevice,
              (uint32_t[5], int),
              (override));

  void setCctGainsForTest(float warmGain, float coldGain) {
    warmWhiteGain = warmGain;
    coldWhiteGain = coldGain;
  }

  void setMaxHwValueForTest(int value) {
    setMaxHwValue(value);
  }
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
  value->whiteTemperature = whiteTemperature;
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
                  RGBW_COMMAND_SET_WHITE_TEMPERATURE_WITHOUT_TURN_ON);
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
                  RGBW_COMMAND_SET_WHITE_TEMPERATURE_WITHOUT_TURN_ON);
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

TEST(RgbCctTests, CctGainMappingUsesWarmAndColdChannels) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  RgbCctBaseForTest rgb;

  rgb.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_DIMMER_CCT);
  rgb.setMaxHwValueForTest(1000);
  rgb.setCctGainsForTest(0.25f, 2.0f);
  rgb.setFadeEffectTime(0);

  time.advance(1000);
  rgb.onInit();

  TSD_SuplaChannelNewValue msg = {};
  setRGBCCTValues(reinterpret_cast<TRGBW_Value *>(msg.value),
                  0,
                  0,
                  0,
                  0,
                  100,
                  25,
                  0,
                  RGBW_COMMAND_NOT_SET);
  rgb.handleNewValueFromServer(&msg);

  uint32_t output[5] = {};
  int usedOutputs = 0;
  EXPECT_CALL(rgb, setRGBCCTValueOnDevice(_, 2))
      .WillOnce([&](uint32_t values[5], int used) {
        for (int i = 0; i < used; i++) {
          output[i] = values[i];
        }
        usedOutputs = used;
      });

  time.advance(1);
  rgb.onFastTimer();
  time.advance(1);
  rgb.onFastTimer();

  EXPECT_EQ(usedOutputs, 2);
  EXPECT_EQ(output[0], 187U);
  EXPECT_EQ(output[1], 500U);
}

TEST(RgbCctTests, LegacyStorageMigrationIsOptIn) {
  RgbCctBaseForTest rgb;

  EXPECT_FALSE(rgb.isStateStorageMigrationNeeded());

  rgb.convertStorageFromLegacyChannel(
      Supla::Control::LightingPwmBase::LegacyChannelFunction::RGB);

  EXPECT_TRUE(rgb.isStateStorageMigrationNeeded());

  rgb.setSkipLegacyMigration();
  EXPECT_FALSE(rgb.isStateStorageMigrationNeeded());
}
