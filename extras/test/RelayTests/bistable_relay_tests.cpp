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
#include <supla/control/bistable_relay.h>
#include <protocol_layer_mock.h>
#include <supla/device/register_device.h>
#include <supla/control/button.h>
#include <cstring>

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::Pointee;
using ::testing::AtLeast;

class BistableRelayFixture : public testing::Test {
 public:
  DigitalInterfaceMock ioMock;
  StorageMock storage;
  SimpleTime time;
  ProtocolLayerMock protoMock;

  BistableRelayFixture() {
  }

  virtual ~BistableRelayFixture() {
  }

  void SetUp() {
    Supla::Channel::resetToDefaults();
    EXPECT_CALL(storage, scheduleSave(_, 2000)).WillRepeatedly(Return());
  }

  void TearDown() {
    Supla::Channel::resetToDefaults();
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
    r->handleChannelConfig(&result, false);
  }
};

TEST_F(BistableRelayFixture, basicTests) {
  int r1Gpio = 1;
  int r2Gpio = 2;
  int r3Gpio = 3;
  int r1Value = 0;
  int r2Value = 0;
  int r3Value = 0;
  Supla::Control::BistableRelay r1(r1Gpio);
  Supla::Control::BistableRelay r2(r2Gpio);
  Supla::Control::BistableRelay r3(
      r3Gpio, -1, false, false, true, SUPLA_BIT_FUNC_POWERSWITCH);

  int number1 = r1.getChannelNumber();
  int number2 = r2.getChannelNumber();
  int number3 = r3.getChannelNumber();
  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  ASSERT_EQ(number1, 0);
  ASSERT_EQ(number2, 1);
  ASSERT_EQ(number3, 2);
  EXPECT_EQ(number1, Supla::RegisterDevice::getChannelNumber(number1));
  EXPECT_EQ(Supla::RegisterDevice::getChannelType(number1),
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFunctionList(number1),
            (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  EXPECT_EQ(Supla::RegisterDevice::getChannelDefaultFunction(number1), 0);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number1),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
            SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED |
            SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number1),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(number2, Supla::RegisterDevice::getChannelNumber(number2));
  EXPECT_EQ(Supla::RegisterDevice::getChannelType(number2),
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFunctionList(number2),
            (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  EXPECT_EQ(Supla::RegisterDevice::getChannelDefaultFunction(number2), 0);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number2),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
            SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED |
            SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number2),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(number3, Supla::RegisterDevice::getChannelNumber(number3));
  EXPECT_EQ(Supla::RegisterDevice::getChannelType(number3),
            SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFunctionList(number3),
            SUPLA_BIT_FUNC_POWERSWITCH);
  EXPECT_EQ(Supla::RegisterDevice::getChannelDefaultFunction(number3), 0);
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number3),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
            SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED |
            SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number3),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_CALL(ioMock, digitalRead(r1Gpio))
      .WillRepeatedly(::testing::ReturnPointee(&r1Value));
  EXPECT_CALL(ioMock, digitalWrite(r1Gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&r1Value));

  EXPECT_CALL(ioMock, digitalRead(r2Gpio))
      .WillRepeatedly(::testing::ReturnPointee(&r2Value));
  EXPECT_CALL(ioMock, digitalWrite(r2Gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&r2Value));

  EXPECT_CALL(ioMock, digitalRead(r3Gpio))
      .WillRepeatedly(::testing::ReturnPointee(&r3Value));
  EXPECT_CALL(ioMock, digitalWrite(r3Gpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&r3Value));

  EXPECT_CALL(ioMock, pinMode(r1Gpio, OUTPUT)).Times(2);

  r1.onInit();
  EXPECT_FALSE(r1.isOn());

  EXPECT_CALL(ioMock, pinMode(r2Gpio, OUTPUT)).Times(2);

  r2.onInit();
  EXPECT_FALSE(r2.isOn());

  EXPECT_CALL(ioMock, pinMode(r3Gpio, OUTPUT)).Times(2);

  r3.onInit();
  EXPECT_FALSE(r3.isOn());

  r1.disableCountdownTimerFunction();
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number1),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
            SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_FALSE(r1.isCountdownTimerFunctionEnabled());
  r1.enableCountdownTimerFunction();
  EXPECT_TRUE(r1.isCountdownTimerFunctionEnabled());
  EXPECT_EQ(Supla::RegisterDevice::getChannelFlags(number1),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
            SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED |
            SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
}

TEST_F(BistableRelayFixture, stateOnInitTests) {
  int r1Gpio = 1;
  int r2Gpio = 2;
  int r3Gpio = 3;
  int r1Value = 0;
  int r2Value = 0;
  int r3Value = 0;
  int r1StateGpio = 4;
  int r2StateGpio = 5;
  int r3StateGpio = 6;
  int r1StateValue = 0;
  int r2StateValue = 0;
  int r3StateValue = 0;

  Supla::Control::BistableRelay r1(r1Gpio, r1StateGpio);
  Supla::Control::BistableRelay r2(r2Gpio, r2StateGpio);
  Supla::Control::BistableRelay r3(
      r3Gpio, r3StateGpio, true, true, true, SUPLA_BIT_FUNC_POWERSWITCH);

  EXPECT_CALL(ioMock, digitalRead(r1Gpio))
      .WillRepeatedly(::testing::ReturnPointee(&r1Value));
  EXPECT_CALL(ioMock, digitalWrite(r1Gpio, _))
      .WillRepeatedly(::testing::DoAll(::testing::SaveArg<1>(&r1Value),
                                       ::testing::Invoke([&](int, int value) {
                                         if (value == 1) {
                                           r1StateValue = 1 - r1StateValue;
                                         }
                                       })));

  EXPECT_CALL(ioMock, digitalRead(r2Gpio))
      .WillRepeatedly(::testing::ReturnPointee(&r2Value));
  EXPECT_CALL(ioMock, digitalWrite(r2Gpio, _))
      .WillRepeatedly(::testing::DoAll(::testing::SaveArg<1>(&r2Value),
                                       ::testing::Invoke([&](int, int value) {
                                         if (value == 1) {
                                           r2StateValue = 1 - r2StateValue;
                                         }
                                       })));

  EXPECT_CALL(ioMock, digitalRead(r3Gpio))
      .WillRepeatedly(::testing::ReturnPointee(&r3Value));
  EXPECT_CALL(ioMock, digitalWrite(r3Gpio, _))
      .WillRepeatedly(::testing::DoAll(::testing::SaveArg<1>(&r3Value),
                                       ::testing::Invoke([&](int, int value) {
                                         if (value == 1) {
                                           r3StateValue = 1 - r3StateValue;
                                         }
                                       })));

  EXPECT_CALL(ioMock, digitalRead(r1StateGpio))
      .WillRepeatedly(::testing::ReturnPointee(&r1StateValue));

  EXPECT_CALL(ioMock, digitalRead(r2StateGpio))
      .WillRepeatedly(::testing::ReturnPointee(&r2StateValue));

  EXPECT_CALL(ioMock, digitalRead(r3StateGpio))
      .WillRepeatedly(::testing::ReturnPointee(&r3StateValue));

  r1.setDefaultStateOn();
  r2.setDefaultStateOff();
  r3.setDefaultStateRestore();

  storage.defaultInitialization(3 * 5);

  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  // send channel value is not verified in detail
  EXPECT_CALL(protoMock, sendChannelValueChanged(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(1, 0, 0, 0));

  // R1
  EXPECT_CALL(ioMock, pinMode(r1Gpio, OUTPUT)).Times(2);
  EXPECT_CALL(ioMock, pinMode(r1StateGpio, INPUT_PULLUP)).Times(1);
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  EXPECT_TRUE(r1.isOn());

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(500);
  }
  EXPECT_TRUE(r1.isOn());

  // R2
  EXPECT_CALL(ioMock, pinMode(r2Gpio, OUTPUT)).Times(2);
  EXPECT_CALL(ioMock, pinMode(r2StateGpio, INPUT_PULLUP)).Times(1);
  r2.onLoadConfig(nullptr);
  r2.onLoadState();
  r2.onInit();
  r2.onRegistered(nullptr);
  EXPECT_FALSE(r2.isOn());

  for (int i = 0; i < 10; i++) {
    r2.iterateAlways();
    r2.iterateConnected();
    time.advance(500);
  }
  EXPECT_FALSE(r2.isOn());

  // R3
  EXPECT_CALL(ioMock, pinMode(r3Gpio, OUTPUT)).Times(2);
  EXPECT_CALL(ioMock, pinMode(r3StateGpio, INPUT_PULLUP)).Times(1);
  r3.onLoadConfig(nullptr);
  r3.onLoadState();
  r3.onInit();
  EXPECT_TRUE(r3.isOn());

  for (int i = 0; i < 10; i++) {
    r3.iterateAlways();
    r3.iterateConnected();
    time.advance(500);
  }
  EXPECT_TRUE(r3.isOn());
}

TEST_F(BistableRelayFixture, mixedChecks) {
  int r1Gpio = 1;
  int r1Value = 0;
  int r1StateGpio = 4;
  int r1StateValue = 1;

  Supla::Control::BistableRelay r1(r1Gpio, r1StateGpio);

  EXPECT_CALL(ioMock, digitalRead(r1Gpio))
      .WillRepeatedly(::testing::ReturnPointee(&r1Value));
  EXPECT_CALL(ioMock, digitalWrite(r1Gpio, _))
      .WillRepeatedly(::testing::DoAll(::testing::SaveArg<1>(&r1Value),
                                       ::testing::Invoke([&](int, int value) {
                                         if (value == 1) {
                                           r1StateValue = 1 - r1StateValue;
                                         }
                                       })));

  EXPECT_CALL(ioMock, digitalRead(r1StateGpio))
      .WillRepeatedly(::testing::ReturnPointee(&r1StateValue));

  r1.setDefaultStateOn();

  storage.defaultInitialization(1 * 5);

  unsigned char storedRelayFlags = 1;
  EXPECT_CALL(storage, readStorage(_, _, 4, _))
    .WillRepeatedly([](uint32_t, unsigned char *data, int, bool) {
        uint32_t storedDurationMs = 0;
        memcpy(data, &storedDurationMs, sizeof(storedDurationMs));
        return 4;
      });
  EXPECT_CALL(storage, readStorage(_, _, 1, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(storedRelayFlags), Return(1)));

  EXPECT_CALL(protoMock,
              sendChannelValueChanged(
                  0,
                  testing::Truly([&](const int8_t *v) {
                    const uint8_t expectedValue[] = {
                        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                    return v != nullptr &&
                           std::memcmp(v, expectedValue, 8) == 0;
                  }),
                  0,
                  0)).Times(7);
  EXPECT_CALL(protoMock,
              sendChannelValueChanged(
                  0,
                  testing::Truly([&](const int8_t *v) {
                    const uint8_t expectedValue[] = {
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                    return v != nullptr &&
                           std::memcmp(v, expectedValue, 8) == 0;
                  }),
                  0,
                  0)).Times(7);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 0, 0, 0)).Times(7);
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 2000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 5000, 0, 0));
  EXPECT_CALL(protoMock, sendRemainingTimeValue(0, 3000, 0, 0)).Times(4);

  // R1
  EXPECT_CALL(ioMock, pinMode(r1Gpio, OUTPUT)).Times(2);
  EXPECT_CALL(ioMock, pinMode(r1StateGpio, INPUT_PULLUP)).Times(1);
  r1.onLoadConfig(nullptr);
  r1.onLoadState();
  r1.onInit();
  r1.onRegistered(nullptr);
  EXPECT_TRUE(r1.isOn());

  for (int i = 0; i < 100; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_TRUE(r1.isOn());
  EXPECT_EQ(r1StateValue, 1);

  r1.turnOn();
  for (int i = 0; i < 100; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_TRUE(r1.isOn());
  EXPECT_EQ(r1StateValue, 1);

  r1.turnOff();
  for (int i = 0; i < 100; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_FALSE(r1.isOn());
  EXPECT_EQ(r1StateValue, 0);


  r1.turnOn(2000);  // turn on for 2 s
  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_TRUE(r1.isOn());
  EXPECT_EQ(r1StateValue, 1);

  for (int i = 0; i < 12; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_FALSE(r1.isOn());
  EXPECT_EQ(r1StateValue, 0);

  // change state externally to 1
  r1StateValue = 1;

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_TRUE(r1.isOn());
  EXPECT_EQ(r1StateValue, 1);

  // change state externally to 0
  r1StateValue = 0;

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_FALSE(r1.isOn());
  EXPECT_EQ(r1StateValue, 0);


  r1.turnOn(5000);  // turn on for 5 s
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_TRUE(r1.isOn());
  EXPECT_EQ(r1StateValue, 1);

  // change state externally to 0 (during turn on timer)
  r1StateValue = 0;

  for (int i = 0; i < 10; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_FALSE(r1.isOn());
  EXPECT_EQ(r1StateValue, 0);

  for (int i = 0; i < 21; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }

  r1.iterateAlways();
  r1.iterateConnected();
  time.advance(100);
  EXPECT_FALSE(r1.isOn());
  EXPECT_EQ(r1StateValue, 0);

  // change function to staircase, then turn on with 3s timer.
  // Time should be saved and used later in turnOn(0);
  r1.setFunction(SUPLA_CHANNELFNC_STAIRCASETIMER);
  r1.setStoredTurnOnDurationMs(3000);  // 3 s
  r1.turnOn();  // turn on for 3 s
  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_TRUE(r1.isOn());
  EXPECT_EQ(r1StateValue, 1);

  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_FALSE(r1.isOn());
  EXPECT_EQ(r1StateValue, 0);

  // turn on externally, timer should be applied
  r1StateValue = 1;

  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_TRUE(r1.isOn());
  EXPECT_EQ(r1StateValue, 1);


  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_FALSE(r1.isOn());
  EXPECT_EQ(r1StateValue, 0);

  // second turn on externally, timer should be applied
  r1StateValue = 1;

  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_TRUE(r1.isOn());
  EXPECT_EQ(r1StateValue, 1);

  // turn off externally
  r1StateValue = 0;

  for (int i = 0; i < 5; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_FALSE(r1.isOn());
  EXPECT_EQ(r1StateValue, 0);

  // third turn on externally, timer should be applied
  r1StateValue = 1;

  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_TRUE(r1.isOn());
  EXPECT_EQ(r1StateValue, 1);


  for (int i = 0; i < 20; i++) {
    r1.iterateAlways();
    r1.iterateConnected();
    time.advance(100);
  }
  EXPECT_FALSE(r1.isOn());
  EXPECT_EQ(r1StateValue, 0);
}

