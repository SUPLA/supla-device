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
#include <supla/control/dimmer_base.h>
#include <supla/actions.h>
#include <supla/storage/storage.h>

using ::testing::Return;
using ::testing::_;
using ::testing::AtLeast;

class DimmerBaseForTest : public Supla::Control::DimmerBase {
  public:
    MOCK_METHOD(void, setRGBWValueOnDevice, (uint32_t, uint32_t, uint32_t, uint32_t, uint32_t), (override));
};

class TimeInterfaceStub : public TimeInterface {
  public:
    virtual uint32_t millis() override {
      static uint32_t value = 0;
      value += 1000;
      return value;
    }
};

class SimpleTime : public TimeInterface {
  public:
    SimpleTime() : value(0) {}

    virtual uint32_t millis() override {
      return value;
    }

    void advance(int advanceMs) {
      value += advanceMs;
    }

    uint32_t value;
};

TEST(DimmerTests, InitializationWithDefaultValues) {
  Supla::Channel::resetToDefaults();
  TimeInterfaceStub time;

  DimmerBaseForTest dimmer;

  ASSERT_NE(dimmer.getChannel(), nullptr);

  auto ch = dimmer.getChannel();

  EXPECT_EQ(ch->getChannelType(), SUPLA_CHANNELTYPE_DIMMER);
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  dimmer.onInit();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  dimmer.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);
}

TEST(DimmerTests, DimmerShouldIgnoreRGBValues) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;

  DimmerBaseForTest dimmer;

  auto ch = dimmer.getChannel();
  // disable fading effect so we'll get instant setting value on device call
  dimmer.setFadeEffectTime(0);

  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, 0)).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (5*1023/100))).Times(1);

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  dimmer.onInit();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(1000);
  dimmer.iterateAlways();
  time.advance(1);
  dimmer.onFastTimer();
  time.advance(1);
  dimmer.onFastTimer();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  // we call with rgbw settings, which should be passed as 0
  dimmer.setRGBW(1, 2, 3, 4, 5);

  time.advance(1000);
  dimmer.iterateAlways();
  time.advance(1000);
  dimmer.onFastTimer();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 5);
}

TEST(DimmerTests, HandleActionTests) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;

  DimmerBaseForTest dimmer;

  auto ch = dimmer.getChannel();
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(_, _, _, _, _)).Times(AtLeast(1));

  dimmer.setStep(10);
  dimmer.setMinIterationBrightness(10);
  dimmer.setMinMaxIterationDelay(400);
  dimmer.onInit();
  dimmer.onFastTimer();
  time.advance(400);
  dimmer.iterateAlways();
  dimmer.onFastTimer();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  time.advance(400);
  dimmer.handleAction(1, Supla::ITERATE_DIM_ALL);
  for (int i = 0; i < 45; i++) {
    time.advance(10);
    dimmer.iterateAlways();
    dimmer.onFastTimer();
  }
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 10);

  dimmer.handleAction(1, Supla::ITERATE_DIM_ALL);
  for (int i = 0; i < 45; i++) {
    time.advance(10);
    dimmer.iterateAlways();
    dimmer.onFastTimer();
  }
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 20);

  dimmer.handleAction(1, Supla::ITERATE_DIM_ALL);
  for (int i = 0; i < 45; i++) {
    time.advance(10);
    dimmer.iterateAlways();
    dimmer.onFastTimer();
  }
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 30);

  dimmer.handleAction(1, Supla::ITERATE_DIM_ALL);
  for (int i = 0; i < 45; i++) {
    time.advance(10);
    dimmer.iterateAlways();
    dimmer.onFastTimer();
  }
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 40);

  dimmer.handleAction(1, Supla::TOGGLE);
  for (int i = 0; i < 45; i++) {
    time.advance(10);
    dimmer.iterateAlways();
    dimmer.onFastTimer();
  }
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  dimmer.handleAction(1, Supla::TOGGLE);
  for (int i = 0; i < 45; i++) {
    time.advance(10);
    dimmer.iterateAlways();
    dimmer.onFastTimer();
  }
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 40);
}

TEST(DimmerTests, IterateDimmerChangeDirection) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;

  DimmerBaseForTest dimmer;

  auto ch = dimmer.getChannel();

  dimmer.setStep(5);  // we'll call handleAction every 50 ms, so 5% step will
                      // drive 0 to 100% in 1 s
  dimmer.setMinMaxIterationDelay(400);
  dimmer.setMinIterationBrightness(5);
  dimmer.onInit();
  dimmer.iterateAlways();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  int prevBrightness = 0;
  enum Expectation {
    EXPECT_ZERO = 0,
    EXPECT_GREATER = 1,
    EXPECT_LESS = 2
  };

  enum Expectation expectation = EXPECT_ZERO;
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, _))
      .WillRepeatedly(
          [&prevBrightness, &expectation](
              uint32_t red,
              uint32_t green,
              uint32_t blue,
              uint32_t colorBrightness,
              uint32_t brightness) {
            if (expectation == EXPECT_ZERO) {
              EXPECT_EQ(brightness, 0);
            } else if (expectation == EXPECT_GREATER) {
              EXPECT_GT(brightness, prevBrightness);
            } else if (expectation == EXPECT_LESS) {
              EXPECT_LT(brightness, prevBrightness);
            }
            prevBrightness = brightness;
          });

  expectation = EXPECT_ZERO;

  time.advance(1);
  dimmer.onFastTimer();
  time.advance(1);
  dimmer.onFastTimer();

  // pass 1 s, it should set 0 on device
  for (int i = 0; i < 100; i++) {
    if (i % 5 == 0) {
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  expectation = EXPECT_GREATER;

  // pass another 1 s, this time with iterate dim w called every 50 ms with 5%
  // step. 400 ms is added for startup brightnening delay
  for (int i = 0; i < 140; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // another 0.4 s should not change anything. Iterate dim w is being called,
  // however there is a delay between direction changes
  for (int i = 0; i < 40; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  expectation = EXPECT_LESS;

  for (int i = 0; i < 100; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // another 0.4 s should not change anything. Iterate dim w is being called,
  // however there is a delay between direction changes
  for (int i = 0; i < 40; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  expectation = EXPECT_GREATER;

  for (int i = 0; i < 100; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // another 0.4 s should not change anything. Iterate dim w is being calles,
  // however there is a delay between direction changes
  for (int i = 0; i < 40; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  expectation = EXPECT_LESS;

  // another 0.55 s should dim from 100% to 50%
  for (int i = 0; i < 55; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // we don't call "iterate dim" at all during another 0.5 s. It should trigger
  // direction change on next iterate dim calls. However here, nothing should
  // happen
  for (int i = 0; i < 50; i++) {
    if (i % 5 == 0) {
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  expectation = EXPECT_GREATER;

  // another 0.50 s should dim from 50% to 100%
  for (int i = 0; i < 50; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onFastTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }
}
