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

#include <gtest/gtest.h>
#include <supla/channel.h>
#include <arduino_mock.h>
#include <simple_time.h>
#include <storage_mock.h>

#include <supla/control/roller_shutter.h>
#include <supla/actions.h>
#include "gmock/gmock.h"

using ::testing::_;
using ::testing::AtLeast;

class RollerShutterFixture : public testing::Test {
 public:
  DigitalInterfaceMock ioMock;
  SimpleTime time;
  int gpioUp = 1;
  int gpioDown = 2;

  RollerShutterFixture() {
  }

  virtual ~RollerShutterFixture() {
  }

  void SetUp() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }

  void TearDown() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }
};

TEST_F(RollerShutterFixture, basicTests) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  int number = rs.getChannelNumber();
  ASSERT_EQ(number, 0);
  TDSC_RollerShutterValue value = {};
  EXPECT_EQ(number, Supla::Channel::reg_dev.channels[number].Number);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number].Type,
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number].FuncList,
            SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number].Default,
            SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number].Flags,
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
                SUPLA_CHANNEL_FLAG_RS_SBS_AND_STOP_ACTIONS);
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[number].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));
}

TEST_F(RollerShutterFixture, onInitHighIsOn) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(gpioDown,  OUTPUT));

  rs.onInit();
}

TEST_F(RollerShutterFixture, onInitLowIsOn) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown, false);

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(gpioDown,  OUTPUT));

  rs.onInit();
}

#pragma pack(push, 1)
struct RollerShutterStateDataTests {
  uint32_t closingTimeMs;
  uint32_t openingTimeMs;
  int8_t currentPosition;  // 0 - closed; 100 - opened
};
#pragma pack(pop)

TEST_F(RollerShutterFixture, notCalibratedStartup) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, pinMode(gpioDown,  OUTPUT));

  // move down
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

  // move up - it first call stop
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

  // then actual move up:
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

  // stop
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

  rs.onInit();

  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    time.advance(100);
  }

  TDSC_RollerShutterValue value = {};
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  rs.handleAction(0, Supla::MOVE_DOWN);
  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    time.advance(100);
  }

  value.position = -1;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  rs.handleAction(0, Supla::MOVE_UP);
  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  rs.handleAction(0, Supla::STOP);
  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    time.advance(100);
  }
}

TEST_F(RollerShutterFixture, movementTests) {
  StorageMock storage;
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  EXPECT_CALL(storage, scheduleSave(_)).Times(AtLeast(0));

  {
    ::testing::InSequence seq;

    EXPECT_CALL(storage, readState(_, /* sizeof(RollerShutterStateData) */ 9))
        .WillOnce([](unsigned char *data, int size) {
          RollerShutterStateDataTests rsData = {.closingTimeMs = 10000,
                                                .openingTimeMs = 10000,
                                                .currentPosition = 0};
          EXPECT_EQ(9, sizeof(rsData));
          memcpy(data, &rsData, sizeof(RollerShutterStateDataTests));
          return 9;
        });

    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
    EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

    // move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // move up - it first call stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // then actual move up:
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    // stop after reaching limit
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // sbs - stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move up
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  }

  rs.onLoadState();
  rs.onInit();

  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    time.advance(100);
  }

  TDSC_RollerShutterValue value = {};
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  rs.handleAction(0, Supla::MOVE_DOWN);
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 10);

  rs.handleAction(0, Supla::MOVE_UP);
  // relays are disabled after 60s timeout
  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 0);

  rs.handleAction(0, Supla::STEP_BY_STEP);  // sbs - move down
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 10);

  rs.handleAction(0, Supla::STEP_BY_STEP);  // sbs - stop
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 10);

  rs.handleAction(0, Supla::STEP_BY_STEP);  // sbs - move up
  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 0);
}

TEST_F(RollerShutterFixture, movementByServerTests) {
  StorageMock storage;
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  EXPECT_CALL(storage, scheduleSave(_)).Times(AtLeast(0));

  {
    ::testing::InSequence seq;

    EXPECT_CALL(storage, readState(_, /* sizeof(RollerShutterStateData) */ 9))
        .WillOnce([](unsigned char *data, int size) {
          RollerShutterStateDataTests rsData = {.closingTimeMs = 10000,
                                                .openingTimeMs = 10000,
                                                .currentPosition = 0};
          EXPECT_EQ(9, sizeof(rsData));
          memcpy(data, &rsData, sizeof(RollerShutterStateDataTests));
          return 9;
        });

    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
    EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

    // move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // move up - it first call stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // then actual move up:
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    // stop after reaching limit
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // sbs - stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move up
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // move up
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // move up
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));
  }

  rs.onLoadState();
  rs.onInit();

  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    time.advance(100);
  }

  TCSD_RollerShutterValue *value = nullptr;
  TSD_SuplaChannelNewValue newValueFromServer = {};

  value = reinterpret_cast<TCSD_RollerShutterValue*>(newValueFromServer.value);

  newValueFromServer.DurationMS = (100 << 16) | 100;
  newValueFromServer.ChannelNumber = 0;
  value->position = 1;  // move down

  rs.handleNewValueFromServer(&newValueFromServer);
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 10);

  value->position = 2;  // up
  rs.handleNewValueFromServer(&newValueFromServer);
  // relays are disabled after 60s timeout
  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 0);

  value->position = 5;  // step by step -> move down
  rs.handleNewValueFromServer(&newValueFromServer);
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 10);

  rs.handleNewValueFromServer(&newValueFromServer);  // sbs - stop
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 10);

  rs.handleNewValueFromServer(&newValueFromServer);  // sbs - move up
  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 0);

  rs.handleNewValueFromServer(&newValueFromServer);  // sbs - move down
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 10);

  value->position = 0;
  rs.handleNewValueFromServer(&newValueFromServer);  // stop
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 10);

  value->position = 3;  // down or stop
  rs.handleNewValueFromServer(&newValueFromServer);  // down
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 20);

  rs.handleNewValueFromServer(&newValueFromServer);  // stop (down or stop)
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 20);

  rs.handleNewValueFromServer(&newValueFromServer);  // down (down or stop)
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 30);

  value->position = 4;  // move up or stop
  rs.handleNewValueFromServer(&newValueFromServer);  // stop (up or stop)
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 30);

  rs.handleNewValueFromServer(&newValueFromServer);  // up (up or stop)
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 20);

  value->position = 1;  // down
  rs.handleNewValueFromServer(&newValueFromServer);  // down
  for (int i = 0; i < 16; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 30);

  value->position = 2;  // up
  rs.handleNewValueFromServer(&newValueFromServer);  // up
  for (int i = 0; i < 16; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(Supla::Channel::reg_dev.channels[0].value[0], 20);
}
