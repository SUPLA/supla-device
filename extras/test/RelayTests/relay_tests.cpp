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
#include <protocol_layer_mock.h>
#include "gmock/gmock.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::Pointee;
using ::testing::AtLeast;

class RelayFixture : public testing::Test {
 public:
  DigitalInterfaceMock ioMock;
  StorageMock storage;
  SimpleTime time;
  ProtocolLayerMock protoMock;

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
      result.ConfigSize = sizeof(TChannelConfig_StaircaseTimer);
      TChannelConfig_StaircaseTimer *config =
        reinterpret_cast<TChannelConfig_StaircaseTimer *>(&result.Config);
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

  r1.disableCountdownTimerFunction();
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number1].Flags,
            SUPLA_CHANNEL_FLAG_CHANNELSTATE);
  EXPECT_FALSE(r1.isCountdownTimerFunctionEnabled());
  r1.enableCountdownTimerFunction();
  EXPECT_TRUE(r1.isCountdownTimerFunctionEnabled());
  EXPECT_EQ(Supla::Channel::reg_dev.channels[number1].Flags,
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
            SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
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

  storage.defaultInitialization(3 * 5);

  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpio1Value = 0;
  int gpio2Value = 0;
  int gpio3Value = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio1))
      .WillRepeatedly(::testing::ReturnPointee(&gpio1Value));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpio1Value));
  EXPECT_CALL(ioMock, digitalRead(gpio2))
      .WillRepeatedly(::testing::ReturnPointee(&gpio2Value));
  EXPECT_CALL(ioMock, digitalWrite(gpio2, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpio2Value));
  EXPECT_CALL(ioMock, digitalRead(gpio3))
      .WillRepeatedly(::testing::ReturnPointee(&gpio3Value));
  EXPECT_CALL(ioMock, digitalWrite(gpio3, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpio3Value));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, getChannelConfig(1, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(1, 0, 0, 0));

  // virtual Relay &keepTurnOnDuration(bool keep = true);
  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  EXPECT_EQ(gpio1Value, 1);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpio1Value, 1);

  // R2
  // init
  EXPECT_CALL(ioMock, pinMode(gpio2, OUTPUT));

  r2.onLoadConfig(nullptr);
  r2.onLoadState();
  r2.onInit();
  r2.onRegistered(nullptr);
  EXPECT_EQ(gpio2Value, 0);

  for (int i = 0; i < 10; i++) {
    r2.iterateAlways();
    r2.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpio2Value, 0);

  // R3
  // init
  EXPECT_CALL(ioMock, pinMode(gpio3, OUTPUT));

  r3.onLoadConfig(nullptr);
  r3.onLoadState();
  r3.onInit();
  EXPECT_EQ(gpio3Value, 1);

  for (int i = 0; i < 10; i++) {
    r3.iterateAlways();
    r3.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpio3Value, 1);
}

TEST_F(RelayFixture, startupTestsForLight) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);

  // data is read from storage, but it is not used by Relay
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 1000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));  //
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 1, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(gpioValue, 0);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  value[0] = 1;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_OFF);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  value[0] = 0;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  value[0] = 1;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);
  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);
  r1.toggle();
  EXPECT_EQ(gpioValue, 0);
  r1.toggle();
  EXPECT_EQ(gpioValue, 1);

  EXPECT_EQ(1, r1.isOn());

  // countdown timer checks
  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  for (int i = 0; i < 2; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // check if duration wasn't stored
  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // Scenario1, from server:
  // turn on, turn off, turn off, turn on, turn on
  newValueFromServer.DurationMS = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnedOnTurnOffandTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnedOnTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // First we start with turnOff(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);

  // turnOff(2s) and then in the middle, turnOn(0)
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // turnOff(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // turnOff(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // we start with turnOn(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  //
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // turnOn(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // turnOn(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // turnOn(2s) and then in the middle, turnOn(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, turnOff(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, toggle(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  r1.toggle();
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
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
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));

  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(gpioValue, 1);
  r1.onRegistered(nullptr);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);


  r1.turnOn(1000);  // turn on for 1000 ms
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 15; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOn(200);  // turn on for 200 ms
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 5; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOn(1000);  // turn on for 1000 ms
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // additional turn on 2000 ms, when first is turn on is not completed yet
  // it should restart timer and turn off after 2 s
  r1.turnOn(2000);  // turn on for 2000 ms
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOff(1000);
  EXPECT_EQ(gpioValue, 0);

  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  ////////////////////////////////////////////////
  // Check handling of "new value from server"
  ////////////////////////////////////////////////

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.value[0] = 2;  // invalid value
  EXPECT_EQ(-1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.value[0] = 0;  // turn off with duration 1s - which is
                                    // currently ignored by s-d
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // there is no "keepTurnOnDuration" enabled
  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);

  // when countdown timer was introduced, keepTurnOnDuration() method was
  // deprecated and it doesn't have any effect, so instead of it, we
  // send configuration vie channel config message
  r1.keepTurnOnDuration(true);
  TSD_ChannelConfig result = {};
  result.Func = SUPLA_CHANNELFNC_STAIRCASETIMER;
  result.ConfigType = 0;
  result.ConfigSize = sizeof(TChannelConfig_StaircaseTimer);
  TChannelConfig_StaircaseTimer *config =
      reinterpret_cast<TChannelConfig_StaircaseTimer *>(&result.Config);
  config->TimeMS = 1000;
  r1.handleChannelConfig(&result);

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // "keepTurnOnDuration" is enabled, so it should use 1000 ms as duration
  r1.turnOn();
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // "keepTurnOnDuration" is enabled, so it should use 1000 ms as duration
  r1.turnOn(2000);
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
}

TEST_F(RelayFixture, keepTurnOnDurationRestoreOnTests) {
  int gpio1 = 1;
  Supla::Control::Relay r1(gpio1);

  r1.setDefaultStateRestore();
  r1.keepTurnOnDuration(true);

  storage.defaultInitialization(5);

  unsigned char storedRelayFlags = RELAY_FLAGS_ON | RELAY_FLAGS_STAIRCASE;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(gpioValue, 1);
  r1.onRegistered(nullptr);

  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  r1.turnOn(5000);
  EXPECT_EQ(gpioValue, 1);

  // turn off will happen after 2.5s because of stored turn on duration
  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);

  // turn off will last 1s, then it will turn on and off after timeout
  r1.turnOff(1000);
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 15; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 17; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 1);
  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
  r1.turnOff();
  EXPECT_EQ(gpioValue, 0);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(gpioValue, 0);
}

TEST_F(RelayFixture, keepTurnOnDurationRestoreOffTests) {
  int gpio1 = 1;
  Supla::Control::Relay r1(gpio1);

  r1.setDefaultStateRestore();
  r1.keepTurnOnDuration(true);

  storage.defaultInitialization(5);

  unsigned char storedRelayFlags = RELAY_FLAGS_STAIRCASE;  // ON flag is not set
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio1))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // R1
  // init
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));

  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);

  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  r1.turnOn(5000);
  EXPECT_EQ(1, gpioValue);

  // turn off will happen after 2.5s because of stored turn on duration
  for (int i = 0; i < 30; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  r1.turnOff(1000);
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  r1.turnOff();
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreTimerOn) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();

  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  time.advance(100);
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(1, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // make sure that duration wasn't remebered
  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreTimerOff) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  time.advance(100);
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // make sure that duration wasn't remebered
  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreOn) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(1, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreOff) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, checkTimerStateStorageForLight) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    // initial read in onLoadState
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    // read before first write
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    // read before second write
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 900;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    // initial read in onLoadState
    .WillOnce(DoAll(SetArgPointee<1>(0), Return(1)))
    // read before first write
    .WillOnce(DoAll(SetArgPointee<1>(0), Return(1)))
    // read before second write
    .WillOnce(DoAll(SetArgPointee<1>(1), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = 1;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 900);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));
  // save
  relayFlags = 0;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 0);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~1s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  // duration should be 0
  r1.onSaveState();
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, startupTestsForLightRestoreOnButConfiguredToOff) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateOff();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 2500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_LIGHTSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, checkTimerStateStorageForPowerSwitch) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 900;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillOnce(DoAll(SetArgPointee<1>(0), Return(1)))
    .WillOnce(DoAll(SetArgPointee<1>(0), Return(1)))
    .WillOnce(DoAll(SetArgPointee<1>(1), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = 1;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 900);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));
  // save
  relayFlags = 0;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 0);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_POWERSWITCH, 0);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~1s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  // duration should be 0
  r1.onSaveState();
  EXPECT_EQ(0, gpioValue);
}

TEST_F(RelayFixture, checkTimerStateStorageForStaircaseTimer) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = RELAY_FLAGS_ON | RELAY_FLAGS_STAIRCASE;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 4000);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));
  // save
  relayFlags = RELAY_FLAGS_STAIRCASE;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 4000);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_STAIRCASETIMER, 4000);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 11; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~1s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  // duration should be 0
  r1.onSaveState();
}

TEST_F(RelayFixture, checkTimerStateStorageForImpulseFunction) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = 0;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = RELAY_FLAGS_ON | RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));
  // save
  relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  // currently impulse functions doesn't have channel config Config field
  // so it still relies on durationMs send in new value
  sendConfig(&r1, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, 500);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 500;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~0.3s elapsed, then save state to storage
  r1.onSaveState();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  r1.onSaveState();
}

TEST_F(RelayFixture, checkTimerStateStorageForImpulseFunctionOnLoad) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  r1.setDefaultStateRestore();
  storage.defaultInitialization(5);

  // data is read from storage
  unsigned char storedRelayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION), Return(1)))
      .WillOnce(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION), Return(1)))
      .WillOnce(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION), Return(1)))
      .WillOnce(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION | RELAY_FLAGS_ON),
                Return(1)));

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  uint8_t relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION | RELAY_FLAGS_ON;
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return 4;
      });
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // save
  relayFlags = RELAY_FLAGS_IMPULSE_FUNCTION;
  EXPECT_CALL(storage, writeStorage(_, Pointee(relayFlags), 1))
      .WillOnce(Return(1));

  // test begins
  r1.onLoadConfig(nullptr);
  Supla::Storage::LoadStateStorage();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  // currently impulse functions doesn't have channel config Config field
  // so it still relies on durationMs send in new value
  sendConfig(&r1, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, 500);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  Supla::Storage::WriteStateStorage();

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 500;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~0.3s elapsed, then save state to storage
  Supla::Storage::WriteStateStorage();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  Supla::Storage::WriteStateStorage();
}

TEST_F(RelayFixture, checkTimerStateStorageForImpulseFunctionOnLoadNoRestore) {
  int gpio = 0;

  Supla::Control::Relay r1(gpio);
  storage.defaultInitialization(5);

  // data is read from storage
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    // Load state storage
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    // First write, no change
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    // Second write, change to 500, but we read previous value
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 600;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      })
    // Last write, no change
    .WillOnce([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 500;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<1>(RELAY_FLAGS_IMPULSE_FUNCTION), Return(1)));

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // save
  EXPECT_CALL(storage, writeStorage(_, _, 4))
      .WillOnce([](uint32_t address, const unsigned char *value, int32_t buf) {
        EXPECT_EQ(*reinterpret_cast<const uint32_t*>(value), 500);
        return 4;
      });

  // test begins
  r1.onLoadConfig(nullptr);
  Supla::Storage::LoadStateStorage();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  // currently impulse functions doesn't have channel config Config field
  // so it still relies on durationMs send in new value
  sendConfig(&r1, SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, 500);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  Supla::Storage::WriteStateStorage();

  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 500;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 3; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // ~0.3s elapsed, then save state to storage
  Supla::Storage::WriteStateStorage();

  for (int i = 0; i < 42; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  Supla::Storage::WriteStateStorage();
}

// test copied from Light version, just function changed
TEST_F(RelayFixture, startupTestsForPowerSwitch) {
  int gpio = 0;
  Supla::Control::Relay r1(gpio);
  storage.defaultInitialization(5);

  // data is read from storage, but it is not used by Relay
  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t address, unsigned char *data, int size, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  int gpioValue = 0;
  EXPECT_CALL(ioMock, digitalRead(gpio))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));
  EXPECT_CALL(ioMock, digitalWrite(gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&gpioValue));
  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, getChannelConfig(0, 0)).Times(1);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, _, _, 0)).Times(AtLeast(1));

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, pinMode(gpio, OUTPUT));

  // test begins
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  EXPECT_EQ(0, gpioValue);
  r1.onRegistered(nullptr);
  sendConfig(&r1, SUPLA_CHANNELFNC_POWERSWITCH, 0);

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  value[0] = 1;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_OFF);
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  value[0] = 0;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.handleAction(0, Supla::TURN_ON);
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  value[0] = 1;
  EXPECT_EQ(0, memcmp(Supla::Channel::reg_dev.channels[0].value,
                          &value,
                          SUPLA_CHANNELVALUE_SIZE));

  r1.turnOff();
  EXPECT_EQ(0, gpioValue);
  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  r1.toggle();
  EXPECT_EQ(0, gpioValue);
  EXPECT_EQ(0, r1.isOn());
  r1.toggle();
  EXPECT_EQ(1, gpioValue);

  EXPECT_EQ(1, r1.isOn());

  // countdown timer checks
  TSD_SuplaChannelNewValue newValueFromServer = {};

  newValueFromServer.DurationMS = 1000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // check if duration wasn't stored
  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
  r1.turnOff();
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // Scenario1, from server:
  // turn on, turn off, turn off, turn on, turn on
  newValueFromServer.DurationMS = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  // Check: CountdownTimerTurnedOnTurnOffandTurnOnAfter2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  // Check: CountdownTimerTurnedOnTurnOnFor2s
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // First we start with turnOff(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);

  // turnOff(2s) and then in the middle, turnOn(0)
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // turnOff(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // turnOff(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // tests that check handling of new value from server which is without
  // timer, but currently device is running some timer
  // we start with turnOn(2s) and then in the middle various scenarios
  // happens

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  //
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 0;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 40; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // turnOn(2s) and then in the middle, turnOff(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 0;  // turn off
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(0, gpioValue);

  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  // turnOn(2s) and then in the middle, turnOn(2s), which should restart timer
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on
  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);

  for (int i = 0; i < 50; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);

  // turnOn(2s) and then in the middle, turnOn(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  r1.turnOff();
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, turnOff(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  r1.turnOn();
  EXPECT_EQ(1, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);
  /////////////////////////////////////////////////////////
  // turnOn(2s) and then in the middle, toggle(0) by button
  newValueFromServer.DurationMS = 2000;
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.value[0] = 1;  // turn on

  EXPECT_EQ(1, r1.handleNewValueFromServer(&newValueFromServer));
  EXPECT_EQ(1, gpioValue);
  // time advance 1 s - which is in the middle of scheduled timer
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(1, gpioValue);

  r1.toggle();
  EXPECT_EQ(0, gpioValue);
  for (int i = 0; i < 22; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_EQ(0, gpioValue);
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
}
