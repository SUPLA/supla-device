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
    virtual uint64_t millis() override {
      static uint64_t value = 0;
      value += 1000;
      return value;
    }
};

class SimpleTime : public TimeInterface {
  public:
    SimpleTime() : value(0) {}

    virtual uint64_t millis() override {
      return value;
    }

    void advance(int advanceMs) {
      value += advanceMs;
    }

    uint64_t value;
};

TEST(DimmerTests, InitializationWithDefaultValues) {
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
  TimeInterfaceStub time;

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

  dimmer.iterateAlways();
  dimmer.onTimer();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  // we call with rgbw settings, which should be passed as 0
  dimmer.setRGBW(1, 2, 3, 4, 5);

  dimmer.iterateAlways();
  dimmer.onTimer();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 5);

}

TEST(DimmerTests, HandleActionTests) {
  SimpleTime time;

  DimmerBaseForTest dimmer;

  auto ch = dimmer.getChannel();
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(_, _, _, _, _)).Times(AtLeast(1));

  dimmer.setStep(10);
  dimmer.setMinIterationBrightness(10);
  dimmer.setMinMaxIterationDelay(400);
  dimmer.onInit();
  dimmer.onTimer();
  time.advance(400);
  dimmer.iterateAlways();
  dimmer.onTimer();

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
    dimmer.onTimer();
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
    dimmer.onTimer();
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
    dimmer.onTimer();
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
    dimmer.onTimer();
  }
  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 40);
}

TEST(DimmerTests, IterateDimmerChangeDirection) {
  SimpleTime time;

  DimmerBaseForTest dimmer;

  auto ch = dimmer.getChannel();

  dimmer.setStep(5); // we'll call handleAction every 50 ms, so 5% step will
                     // drive 0 to 100% in 1 s
  dimmer.setMinMaxIterationDelay(200);
  dimmer.setMinIterationBrightness(5);
  dimmer.onInit();
  dimmer.iterateAlways();
  dimmer.onTimer();

  EXPECT_EQ(ch->getValueRed(), 0);
  EXPECT_EQ(ch->getValueGreen(), 0);
  EXPECT_EQ(ch->getValueBlue(), 0);
  EXPECT_EQ(ch->getValueColorBrightness(), 0);
  EXPECT_EQ(ch->getValueBrightness(), 0);

  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, 0)).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (5*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (10*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (15*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (20*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (25*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (30*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (35*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (40*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (45*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (50*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (55*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (60*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (65*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (70*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (75*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (80*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (85*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (90*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (95*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (100*1023/100))).Times(1);

  // pass 1 s, it should set 0 on device
  for (int i = 0; i < 100; i++) {
    if (i % 5 == 0) {
      dimmer.onTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // pass another 1 s, this time with iterate dim w called every 50 ms with 5%
  // step. 200 ms is added for startup brightnening delay
  for (int i = 0; i < 120; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // pass another 1 s, with iterate dim w called every 50 ms with 5%
  // step. It should dim down from 100% to 5%. Call sequence is not verified
  // Additional 0.20 s is added for delay between direction switch
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (5*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (10*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (15*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (20*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (25*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (30*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (35*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (40*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (45*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (50*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (55*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (60*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (65*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (70*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (75*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (80*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (85*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (90*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (95*1023/100))).Times(1);
  for (int i = 0; i < 120; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // another 0.2 s should not change anything. Iterate dim w is being called,
  // however there is a delay between direction changes
  for (int i = 0; i < 20; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // another 1 s should dim from 5% to 100%. 5% is already on device, so
  // next call is 10%
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (10*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (15*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (20*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (25*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (30*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (35*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (40*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (45*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (50*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (55*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (60*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (65*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (70*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (75*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (80*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (85*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (90*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (95*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (100*1023/100))).Times(1);
  for (int i = 0; i < 100; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // another 0.2 s should not change anything. Iterate dim w is being calles,
  // however there is a delay between direction changes
  for (int i = 0; i < 20; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // another 0.55 s should dim from 100% to 50%
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (50*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (55*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (60*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (65*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (70*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (75*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (80*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (85*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (90*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (95*1023/100))).Times(1);
  for (int i = 0; i < 55; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // we don't call "iterate dim" at all during another 0.5 s. It should trigger
  // direction change on next iterate dim calls. However here, nothing should
  // happen
  for (int i = 0; i < 50; i++) {
    if (i % 5 == 0) {
      dimmer.onTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

  // another 0.50 s should dim from 50% to 100%
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (55*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (60*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (65*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (70*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (75*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (80*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (85*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (90*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (95*1023/100))).Times(1);
  EXPECT_CALL(dimmer, setRGBWValueOnDevice(0, 0, 0, 0, (100*1023/100)))
    .Times(1);
  for (int i = 0; i < 50; i++) {
    if (i % 5 == 0) {
      dimmer.handleAction(1, Supla::ITERATE_DIM_W);
      dimmer.onTimer();
    }
    dimmer.iterateAlways();
    time.advance(10);
  }

}
