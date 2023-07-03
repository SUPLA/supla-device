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
#include <simple_time.h>
#include <storage_mock.h>
#include <supla/actions.h>
#include <supla/channel.h>
#include <supla/control/relay.h>

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::Pointee;

class RelayFixture : public testing::Test {
 public:
  DigitalInterfaceMock ioMock;
  StorageMock storage;
  SimpleTime time;

  RelayFixture() {
  }

  virtual ~RelayFixture() {
  }

  void SetUp() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
    EXPECT_CALL(storage, scheduleSave(_)).WillRepeatedly(Return());
  }

  void TearDown() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }

  void sendConfig(Supla::Control::Relay *r, uint32_t func, uint32_t timeMs) {
    TSD_ChannelConfig result = {};
    result.Func = func;
    result.ConfigType = 0;
    if (func == SUPLA_CHANNELFNC_STAIRCASETIMER) {
      result.ConfigSize = sizeof(TSD_ChannelConfig_StaircaseTimer);
      TSD_ChannelConfig_StaircaseTimer *config =
        reinterpret_cast<TSD_ChannelConfig_StaircaseTimer *>(&result.Config);
      config->TimeMS = timeMs;
    }
    r->handleChannelConfig(&result);
  }
};

TEST_F(RelayFixture, basicTests) {
  int gpio1 = 1;
  int gpio2 = 2;
  int gpio3 = 3;
  Supla::Control::Relay r1(gpio1);
  Supla::Control::Relay r2(gpio2, false);
  Supla::Control::Relay r3(gpio3, false, SUPLA_BIT_FUNC_POWERSWITCH);

  int number1 = r1.getChannelNumber();
  int number2 = r2.getChannelNumber();
  int number3 = r3.getChannelNumber();
  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  ASSERT_EQ(number1, 0);
  ASSERT_EQ(number2, 1);
  ASSERT_EQ(number3, 2);
  EXPECT_EQ(number1, Supla::Channel::reg_dev.channels[number1].Number);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number1].Type,
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number1].FuncList,
            (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number1].Default, 0);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number1].Flags,
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
            SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
  EXPECT_EQ(0,
            memcmp(Supla::Channel::reg_dev.channels[number1].value,
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(number2, Supla::Channel::reg_dev.channels[number2].Number);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number2].Type,
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number2].FuncList,
            (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number2].Default, 0);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number2].Flags,
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
            SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
  EXPECT_EQ(0,
            memcmp(Supla::Channel::reg_dev.channels[number2].value,
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(number3, Supla::Channel::reg_dev.channels[number3].Number);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number3].Type,
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number3].FuncList,
            SUPLA_BIT_FUNC_POWERSWITCH);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number3].Default, 0);
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number3].Flags,
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
            SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
  EXPECT_EQ(0,
            memcmp(Supla::Channel::reg_dev.channels[number3].value,
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(2);
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio2, 1)).Times(2);
  EXPECT_CALL(ioMock, pinMode(gpio2, OUTPUT));

  r2.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio3, 1)).Times(2);
  EXPECT_CALL(ioMock, pinMode(gpio3, OUTPUT));

  r3.onInit();
}

TEST_F(RelayFixture, stateOnInitTests) {
  int gpio1 = 1;
  int gpio2 = 2;
  int gpio3 = 3;
  Supla::Control::Relay r1(gpio1);
  Supla::Control::Relay r2(gpio2);
  Supla::Control::Relay r3(gpio3);

  r1.setDefaultStateOn();
  r2.setDefaultStateOff();
  r3.setDefaultStateRestore();

  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  // virtual Relay &keepTurnOnDuration(bool keep = true);
  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // R2
  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio2, 0));
  EXPECT_CALL(ioMock, pinMode(gpio2, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio2, 0));

  r2.onLoadConfig(nullptr);
  r2.onLoadState();
  r2.onInit();
  r2.onRegistered(nullptr);

  for (int i = 0; i < 10; i++) {
    r2.iterateAlways();
    time.advance(100);
  }

  // R3
  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio3, 1));
  EXPECT_CALL(ioMock, pinMode(gpio3, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio3, 1));

  r3.onLoadConfig(nullptr);
  r3.onLoadState();
  r3.onInit();

  for (int i = 0; i < 10; i++) {
    r3.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RelayFixture, startupTestsForLight) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);

  // data is read from storage, but it is not used by Relay
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // TURN_ON
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // TURN_OFF
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // TURN_ON
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // turnOff
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // turnOn
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // toggle
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // toggle
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // isOn 2x
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));

  // countdown timer checks
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // Scenario1, from server:
  // turn on, turn off, turn off, turn on, turn on
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // first one off is started with 2s timer, but in the meantime we get
  // off without timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // first one off is started with 2s timer, but in the meantime we get
  // on without timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // first one off is started with 2s timer, but in the meantime we get
  // off with another 2s timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // first one off is started with 2s timer, but in the meantime we get
  // on with another 2s timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // first one on is started with 2s timer, but in the meantime we get
  // off without timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // on is started with 2s timer, but in the meantime we get
  // on without timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // on is started with 2s timer, but in the meantime we get
  // off with another 2s timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // on is started with 2s timer, but in the meantime we get
  // on with another 2s timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // on is started with 2s timer, but in the meantime we get
  // off by button
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // on is started with 2s timer, but in the meantime we get
  // on by button
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // on is started with 2s timer, but in the meantime we get
  // toggle by button
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  value[0] = 1;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_OFF);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  value[0] = 0;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  value[0] = 1;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.turnOff();
  r1.turnOn();
  r1.toggle();
  r1.toggle();

  EXPECT_EQ(0, r1.isOn());
  EXPECT_EQ(1, r1.isOn());

  // countdown timer checks
  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // check if duration wasn't stored
  r1.turnOn();
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  r1.turnOff();
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // Scenario1, from server:
  // turn on, turn off, turn off, turn on, turn on
  newValueFromServer.DurationMS = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnedOnTurnOffandTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnedOnTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // First we start with turnOff(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  // turnOff(2s) and then in the middle, turnOn(0)
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOff(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOff(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // we start with turnOn(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  //
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOn(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOn(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOn(2s) and then in the middle, turnOn(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOff();
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, turnOff(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOn();
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, toggle(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.toggle();
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
}

TEST_F(RelayFixture, durationMsTests) {
  int gpio1 = 1;
  Supla::Control::Relay r1(gpio1);

  r1.setDefaultStateOn();

  // duration should be ignored, becuase keepTurnOnDuration is not enabled
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_ON 1000 ms
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  // TURN_OFF
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // TURN_ON 1000 ms
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_OFF
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // TURN_ON 1000 ms
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_ON 2000 ms
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_OFF
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // TURN_OFF
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));
  // TURN_OFF 1000 ms
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // TURN_ON
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // handle new value from server
  // TURN_OFF
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));
  // TURN_ON
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_ON with duration 1s
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // TURN_OFF
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_OFF
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_ON with duration 1s
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // TURN_ON with duration 1s
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // TURN_ON with duration 1s
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }


  r1.turnOn(1000);  // turn on for 1000 ms

  for (int i = 0; i < 15; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOn(200);  // turn on for 200 ms

  for (int i = 0; i < 5; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOn(1000);  // turn on for 1000 ms
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // additional turn on 2000 ms, when first is turn on is not completed yet
  // it should restart timer and turn off after 2 s
  r1.turnOn(2000);  // turn on for 2000 ms

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOff();

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOff(1000);

  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  ////////////////////////////////////////////////
  // Check handling of "new value from server"
  ////////////////////////////////////////////////

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.value[0] = 2;  // invalid value
  EXPECT_EQ(-1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.value[0] = 0;  // turn off with duration 1s - which is
                                    // currently ignored by s-d
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // there is no "keepTurnOnDuration" enabled
  r1.turnOn();

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // when countdown timer was introducetd, keepTurnOnDuration() method was
  // deprecated and it doesn't have any effect, so instead of it, we
  // send configuration vie channel config message
  r1.keepTurnOnDuration(true);
  TSD_ChannelConfig result = {};
  result.Func = SUPLA_CHANNELFNC_STAIRCASETIMER;
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TSD_ChannelConfig_StaircaseTimer);
  TSD_ChannelConfig_StaircaseTimer *config =
      reinterpret_cast<TSD_ChannelConfig_StaircaseTimer *>(&result.Config);
  config->TimeMS = 1000;
  r1.handleChannelConfig(&result);

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // "keepTurnOnDuration" is enabled, so it should use 1000 ms as duration
  r1.turnOn();
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // "keepTurnOnDuration" is enabled, so it should use 1000 ms as duration
  r1.turnOn(2000);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RelayFixture, keepTurnOnDurationRestoreOnTests) {
  int gpio1 = 1;
  Supla::Control::Relay r1(gpio1);

  r1.setDefaultStateRestore();
  r1.keepTurnOnDuration(true);

  unsigned char storedRelayFlags = RELAY_FLAGS_ON | RELAY_FLAGS_STAIRCASE;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_OFF after 2.5 s
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // Turn on
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_OFF after 2.5 s
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // explicit turn off (1000 ms)
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  // it will automatically turn off after 2.5s
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // explicit turn off
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);

  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOn(5000);

  // turn off will happen after 2.5s because of stored turn on duration
  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOff(1000);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  r1.turnOff();
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RelayFixture, keepTurnOnDurationRestoreOffTests) {
  int gpio1 = 1;
  Supla::Control::Relay r1(gpio1);

  r1.setDefaultStateRestore();
  r1.keepTurnOnDuration(true);

  unsigned char storedRelayFlags = RELAY_FLAGS_STAIRCASE;  // ON flag is not set
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // Turn on (5000 ms)
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  // TURN_OFF after 2.5 s
  // there is toggle when durationMs ends, so it reads gpio state
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  // explicit turn off
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));
  // TURN_ON after 1 s
  EXPECT_CALL(ioMock, digitalRead(gpio1)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));
  // explicit turn off
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);

  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOn(5000);

  // turn off will happen after 2.5s because of stored turn on duration
  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOff(1000);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOff();
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RelayFixture, startupTestsForLightRestoreTimerOn) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  // data is read from storage
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // TURN_ON
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // make sure that duration wasn't remebered
  r1.turnOn();
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RelayFixture, startupTestsForLightRestoreTimerOff) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // TURN_ON
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // make sure that duration wasn't remebered
  r1.turnOn();
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RelayFixture, startupTestsForLightRestoreOn) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  // data is read from storage
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RelayFixture, startupTestsForLightRestoreOff) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RelayFixture, checkTimerStateStorageForLight) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // handle new value from server
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // save
  uint8_t relayFlags = 1;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 1100);
        return true;
      });
  // save state checks relay isOn:
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // save
  relayFlags = 0;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 0);
        return true;
      });
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // ~1s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  // duration should be 0
  r1.onSaveState();
}

TEST_F(RelayFixture, startupTestsForLightRestoreOnButConfiguredToOff) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateOff();

  // data is read from storage
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RelayFixture, checkTimerStateStorageForPowerSwitch) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // handle new value from server
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // save
  uint8_t relayFlags = 1;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 1100);
        return true;
      });
  // save state checks relay isOn:
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // save
  relayFlags = 0;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 0);
        return true;
      });
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_POWERSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // ~1s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  // duration should be 0
  r1.onSaveState();
}

TEST_F(RelayFixture, checkTimerStateStorageForStaircaseTimer) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // handle new value from server
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // save
  uint8_t relayFlags = RELAY_FLAGS_ON | RELAY_FLAGS_STAIRCASE;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 4000);
        return true;
      });
  // save state checks relay isOn:
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // save
  relayFlags = RELAY_FLAGS_STAIRCASE;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 4000);
        return true;
      });
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_STAIRCASETIMER, 4000);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // ~1s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  // duration should be 0
  r1.onSaveState();
}

TEST_F(RelayFixture, checkTimerStateStorageForImpulseFunction) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // handle new value from server
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // save
  uint8_t relayFlags = RELAY_FLAGS_ON | RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return true;
      });
  // save state checks relay isOn:
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // save
  relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return true;
      });
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  // currently impulse functions doesn't have channel config Config field
  // so it still relies on durationMs send in new value
  sendConfig(&r1, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, 500);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 500;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // ~0.3s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  r1.onSaveState();
}

TEST_F(RelayFixture, checkTimerStateStorageForImpulseFunctionOnLoad) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  // data is read from storage
  unsigned char storedRelayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  uint8_t relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 600);
        return true;
      });
  // save state checks relay isOn:
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));

  // handle new value from server
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // save
  relayFlags |= RELAY_FLAGS_ON;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return true;
      });
  // save state checks relay isOn:
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // save
  relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return true;
      });
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  // currently impulse functions doesn't have channel config Config field
  // so it still relies on durationMs send in new value
  sendConfig(&r1, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, 500);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOn();
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  r1.onSaveState();

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 500;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // ~0.3s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  r1.onSaveState();
}

TEST_F(RelayFixture, checkTimerStateStorageForImpulseFunctionOnLoadNoRestore) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);

  // data is read from storage
  unsigned char storedRelayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  uint8_t relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 600);
        return true;
      });
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));

  // handle new value from server
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // save
 // relayFlags |= RELAY_FLAGS_ON;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return true;
      });
  // save state checks relay isOn:
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // save
  relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeState(_, 4))
      .WillOnce([](const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return true;
      });
  EXPECT_CALL(storage, writeState(Pointee(relayFlags), 1))
      .WillOnce(Return(true));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  // currently impulse functions doesn't have channel config Config field
  // so it still relies on durationMs send in new value
  sendConfig(&r1, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, 500);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOn();
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  r1.onSaveState();

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 500;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // ~0.3s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  r1.onSaveState();
}

// test copied from Light version, just function changed
TEST_F(RelayFixture, startupTestsForPowerSwitch) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);

  // data is read from storage, but it is not used by Relay
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readState(_, 4))
      .WillRepeatedly([](unsigned char *data, int size) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return true;
      });
  EXPECT_CALL(storage, readState(_, 1))
      .WillRepeatedly(DoAll(SetArgPointee<0>(storedRelayFlags), Return(true)));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // TURN_ON
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // TURN_OFF
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // TURN_ON
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // turnOff
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // turnOn
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // toggle
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  // toggle
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  // isOn 2x
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));

  // countdown timer checks
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // Scenario1, from server:
  // turn on, turn off, turn off, turn on, turn on
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // first one off is started with 2s timer, but in the meantime we get
  // off without timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // first one off is started with 2s timer, but in the meantime we get
  // on without timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // first one off is started with 2s timer, but in the meantime we get
  // off with another 2s timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // first one off is started with 2s timer, but in the meantime we get
  // on with another 2s timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // first one on is started with 2s timer, but in the meantime we get
  // off without timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // on is started with 2s timer, but in the meantime we get
  // on without timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // on is started with 2s timer, but in the meantime we get
  // off with another 2s timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(0));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // on is started with 2s timer, but in the meantime we get
  // on with another 2s timer
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // on is started with 2s timer, but in the meantime we get
  // off by button
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // on is started with 2s timer, but in the meantime we get
  // on by button
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));

  // on is started with 2s timer, but in the meantime we get
  // toggle by button
  EXPECT_CALL(ioMock, digitalWrite(gpio, 1));
  EXPECT_CALL(ioMock, digitalRead(gpio)).WillOnce(Return(1));
  EXPECT_CALL(ioMock, digitalWrite(gpio, 0));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_POWERSWITCH, 0);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  value[0] = 1;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_OFF);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  value[0] = 0;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  value[0] = 1;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.turnOff();
  r1.turnOn();
  r1.toggle();
  r1.toggle();

  EXPECT_EQ(0, r1.isOn());
  EXPECT_EQ(1, r1.isOn());

  // countdown timer checks
  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // check if duration wasn't stored
  r1.turnOn();
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  r1.turnOff();
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // Scenario1, from server:
  // turn on, turn off, turn off, turn on, turn on
  newValueFromServer.DurationMS = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnedOnTurnOffandTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnedOnTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // First we start with turnOff(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  // turnOff(2s) and then in the middle, turnOn(0)
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOff(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOff(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // we start with turnOn(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  //
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOn(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOn(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  // turnOn(2s) and then in the middle, turnOn(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOff();
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, turnOff(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.turnOn();
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, toggle(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    time.advance(100);
  }

  r1.toggle();
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    time.advance(100);
  }
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
}
