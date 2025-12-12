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
#include <supla/control/virtual_relay.h>
#include <protocol_layer_mock.h>
#include <supla/device/register_device.h>
#include <supla/control/button.h>

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::Pointee;
using ::testing::AtLeast;

class VirtualRelayFixture : public testing::Test {
 public:
  DigitalInterfaceMock ioMock;
  StorageMock storage;
  SimpleTime time;
  ProtocolLayerMock protoMock;

  VirtualRelayFixture() {
  }

  virtual ~VirtualRelayFixture() {
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

TEST_F(VirtualRelayFixture, basicTests) {
  Supla::Control::VirtualRelay r1;
  Supla::Control::VirtualRelay r2;
  Supla::Control::VirtualRelay r3(SUPLA_BIT_FUNC_POWERSWITCH);

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

  r1.onInit();
  EXPECT_FALSE(r1.isOn());

  r2.onInit();
  EXPECT_FALSE(r2.isOn());

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

TEST_F(VirtualRelayFixture, stateOnInitTests) {
  Supla::Control::VirtualRelay r1;
  Supla::Control::VirtualRelay r2;
  Supla::Control::VirtualRelay r3;

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

  // virtual Relay &keepTurnOnDuration(bool keep = true);
  ::testing::InSequence seq;

  // R1
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

TEST_F(VirtualRelayFixture, vrWithButtons) {
  int buttonGpio = 1;
  int buttonGpioVal = 0;
  Supla::Control::VirtualRelay r1;
  Supla::Control::Button b1{buttonGpio};

  r1.setDefaultStateRestore();
  r1.attach(&b1);

  storage.defaultInitialization(1 * 5);

  EXPECT_CALL(ioMock, digitalRead(buttonGpio))
      .WillRepeatedly(::testing::ReturnPointee(&buttonGpioVal));
  EXPECT_CALL(ioMock, digitalWrite(buttonGpio, _))
      .WillRepeatedly(::testing::SaveArg<1>(&buttonGpioVal));

  unsigned char storedRelayFlags = 0;
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

  EXPECT_CALL(ioMock, pinMode(buttonGpio, INPUT));

  // R1
  b1.onLoadConfig(nullptr);
  r1.onLoadConfig(nullptr);
  b1.onLoadState();
  r1.onLoadState();
  b1.onInit();
  r1.onInit();
  b1.onRegistered(nullptr);
  r1.onRegistered(nullptr);
  EXPECT_FALSE(r1.isOn());

  for (int i = 0; i < 100; i++) {
    b1.onFastTimer();
    r1.onFastTimer();
    b1.onTimer();
    r1.onTimer();
    b1.iterateAlways();
    r1.iterateAlways();
    b1.iterateConnected();
    r1.iterateConnected();
    time.advance(10);
  }
  EXPECT_FALSE(r1.isOn());

  buttonGpioVal = 1;
  for (int i = 0; i < 10; i++) {
    b1.onFastTimer();
    r1.onFastTimer();
    b1.onTimer();
    r1.onTimer();
    time.advance(10);
  }
  buttonGpioVal = 0;
  for (int i = 0; i < 10; i++) {
    b1.onFastTimer();
    r1.onFastTimer();
    b1.onTimer();
    r1.onTimer();
    time.advance(10);
  }

  for (int i = 0; i < 100; i++) {
    b1.onFastTimer();
    r1.onFastTimer();
    b1.onTimer();
    r1.onTimer();
    b1.iterateAlways();
    r1.iterateAlways();
    b1.iterateConnected();
    r1.iterateConnected();
    time.advance(10);
  }
  EXPECT_TRUE(r1.isOn());
}

