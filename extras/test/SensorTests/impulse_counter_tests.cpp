// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string.h>

#include <protocol_layer_mock.h>
#include <simple_time.h>
#include <supla/network/html/impulse_counter_parameters.h>
#include <supla/sensor/impulse_counter.h>
#include <supla/storage/config.h>

#include "../doubles/supla_io_mock.h"

TEST(ImpulseCounterTests, IoPinConstructorUsesSeparateIoAndPullup) {
  Supla::Channel::resetToDefaults();
  SuplaIoMock ioMock;
  SimpleTime time;

  int gpioValue = LOW;
  EXPECT_CALL(ioMock, customPinMode(0, 7, INPUT_PULLUP))
      .WillOnce(::testing::Return());
  EXPECT_CALL(ioMock, customDigitalRead(0, 7))
      .WillRepeatedly(::testing::ReturnPointee(&gpioValue));

  Supla::Io::IoPin impulsePin(7, &ioMock);
  impulsePin.setPullUp(true);
  Supla::Sensor::ImpulseCounter counter(impulsePin, true);

  counter.onInit();

  EXPECT_EQ(counter.getCounter(), 0);

  gpioValue = HIGH;
  time.advance(11);
  counter.onFastTimer();

  EXPECT_EQ(counter.getCounter(), 1);
}

TEST(ImpulseCounterTests, LocalRateKeepsOtherServerConfigAndRawCount) {
  Supla::Channel::resetToDefaults();
  Supla::Sensor::VirtualImpulseCounter counter;
  counter.setCounter(1000);

  TSD_ChannelConfig message = {};
  message.Func = SUPLA_CHANNELFNC_IC_WATER_METER;
  message.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  TChannelConfig_ImpulseCounter serverConfig = {};
  serverConfig.ImpulsesPerUnit = 500;
  serverConfig.InitialValue = 2500;
  serverConfig.AddToHistory = 1;
  message.ConfigSize = sizeof(serverConfig);
  memcpy(message.Config, &serverConfig, sizeof(serverConfig));

  EXPECT_EQ(counter.handleChannelConfig(&message), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_DOUBLE_EQ(counter.getCalculatedValue(counter.getCounter()), 4.5);

  EXPECT_TRUE(counter.setImpulsesPerUnit(250));
  EXPECT_FALSE(counter.setImpulsesPerUnit(-1));
  EXPECT_EQ(counter.handleChannelConfig(&message), SUPLA_CONFIG_RESULT_TRUE);

  TChannelConfig_ImpulseCounter sent = {};
  int size = 0;
  Supla::ElementWithChannelActions *channel = &counter;
  channel->fillChannelConfig(&sent, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(size, sizeof(sent));
  EXPECT_EQ(sent.ImpulsesPerUnit, 250);
  EXPECT_EQ(sent.InitialValue, 2500);
  EXPECT_EQ(sent.AddToHistory, 1);
  EXPECT_EQ(counter.getCounter(), 1000);
}

TEST(ImpulseCounterTests,
     FunctionChangeAcceptsServerConfigWithLocalEditPending) {
  Supla::Channel::resetToDefaults();
  Supla::Sensor::VirtualImpulseCounter counter;
  counter.setCounter(1);
  ASSERT_TRUE(counter.setImpulsesPerUnit(250));

  TSD_ChannelConfig message = {};
  message.Func = SUPLA_CHANNELFNC_IC_GAS_METER;
  message.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  TChannelConfig_ImpulseCounter config = {};
  config.ImpulsesPerUnit = 600;
  config.InitialValue = 1;  // less than one impulse
  memcpy(config.CustomUnit, "m3", 3);
  message.ConfigSize = sizeof(config);
  memcpy(message.Config, &config, sizeof(config));

  EXPECT_EQ(counter.handleChannelConfig(&message), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(counter.getChannel()->getDefaultFunction(), message.Func);
  EXPECT_EQ(counter.getImpulsesPerUnit(), 600);
  EXPECT_EQ(counter.getCounter(), 1);
  double value = 0;
  const char *unit = nullptr;
  ASSERT_TRUE(counter.getChannel()->getImpulseCounterValue(&value, &unit));
  EXPECT_DOUBLE_EQ(value, 0.001);
  EXPECT_STREQ(unit, "m3");
}

TEST(ImpulseCounterTests, EmptyConfigForNewFunctionUsesFirmwareDefaults) {
  Supla::Channel::resetToDefaults();
  testing::NiceMock<ProtocolLayerMock> protocol;
  Supla::Sensor::VirtualImpulseCounter counter;
  counter.setDefaultImpulsesPerUnit(750);
  counter.setCounter(123);
  ASSERT_TRUE(counter.setImpulsesPerUnit(250));
  ASSERT_TRUE(counter.setCustomUnit("gal"));
  counter.onRegistered(nullptr);

  const auto channel = counter.getChannelNumber();
  TSD_ChannelConfig message = {};
  message.Func = SUPLA_CHANNELFNC_IC_ELECTRICITY_METER;
  message.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  ASSERT_EQ(counter.handleChannelConfig(&message), SUPLA_CONFIG_RESULT_TRUE);

  EXPECT_CALL(protocol, setChannelConfig(testing::_, testing::_, testing::_,
                                         testing::_, testing::_)).Times(0);
  counter.iterateConnected();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&protocol));

  counter.handleChannelConfigFinished(channel);
  EXPECT_CALL(protocol,
              setChannelConfig(channel, message.Func, testing::_,
                               sizeof(TChannelConfig_ImpulseCounter),
                               SUPLA_CONFIG_TYPE_DEFAULT))
      .WillOnce([](uint8_t, int32_t, void *data, int, uint8_t) {
        TChannelConfig_ImpulseCounter config = {};
        memcpy(&config, data, sizeof(config));
        EXPECT_EQ(config.ImpulsesPerUnit, 750);
        EXPECT_STREQ(config.CustomUnit, "");
        return true;
      });
  counter.iterateConnected();
  EXPECT_EQ(counter.getCounter(), 123);
}

TEST(ImpulseCounterTests, OldAckDoesNotCancelDefaultsForNewFunction) {
  class Counter : public Supla::Sensor::VirtualImpulseCounter {
   public:
    using VirtualImpulseCounter::getNextConfigType;
    using VirtualImpulseCounter::setChannelConfigState;
  };
  Supla::Channel::resetToDefaults();
  Counter counter;
  ASSERT_TRUE(counter.setImpulsesPerUnit(250));
  counter.setChannelConfigState(Supla::ChannelConfigState::LocalChangeSent);

  TSD_ChannelConfig config = {};
  config.Func = SUPLA_CHANNELFNC_IC_GAS_METER;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  ASSERT_EQ(counter.handleChannelConfig(&config), SUPLA_CONFIG_RESULT_TRUE);

  TSDS_SetChannelConfigResult ack = {};
  ack.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  ack.Result = SUPLA_CONFIG_RESULT_TRUE;
  counter.handleSetChannelConfigResult(&ack);
  EXPECT_EQ(counter.getNextConfigType(), SUPLA_CONFIG_TYPE_DEFAULT);
}

TEST(ImpulseCounterTests, UnchangedWebFieldsAllowFirstServerConfig) {
  Supla::Channel::resetToDefaults();
  Supla::Sensor::VirtualImpulseCounter counter;
  Supla::Html::ImpulseCounterParameters form(&counter);

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  counter.generateKey(key, "ic_rate");
  ASSERT_TRUE(form.handleResponse(key, "1000"));
  counter.generateKey(key, "ic_unit");
  ASSERT_TRUE(form.handleResponse(key, ""));

  TSD_ChannelConfig message = {};
  message.Func = SUPLA_CHANNELFNC_IC_WATER_METER;
  message.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  TChannelConfig_ImpulseCounter config = {};
  config.ImpulsesPerUnit = 600;
  memcpy(config.CustomUnit, "gal", 4);
  message.ConfigSize = sizeof(config);
  memcpy(message.Config, &config, sizeof(config));

  ASSERT_EQ(counter.handleChannelConfig(&message), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(counter.getImpulsesPerUnit(), 600);
  EXPECT_STREQ(counter.getCustomUnit(), "gal");
}

TEST(ImpulseCounterTests, TruncatesInitialOffsetToWholeImpulses) {
  Supla::Channel::resetToDefaults();
  Supla::Sensor::VirtualImpulseCounter counter;

  TSD_ChannelConfig message = {};
  message.Func = SUPLA_CHANNELFNC_IC_WATER_METER;
  message.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  TChannelConfig_ImpulseCounter config = {};
  config.ImpulsesPerUnit = 5750;
  config.InitialValue = 44;
  message.ConfigSize = sizeof(config);
  memcpy(message.Config, &config, sizeof(config));

  ASSERT_EQ(counter.handleChannelConfig(&message), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_DOUBLE_EQ(counter.getCalculatedValue(0), 0.044);

  config.InitialValue = -44;
  memcpy(message.Config, &config, sizeof(config));
  ASSERT_EQ(counter.handleChannelConfig(&message), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_DOUBLE_EQ(counter.getCalculatedValue(505), 0.043);
  EXPECT_DOUBLE_EQ(counter.getCalculatedValue(100), 0);
}

TEST(ImpulseCounterTests, WebFieldsApplyOnlyToTheirChannel) {
  Supla::Channel::resetToDefaults();
  Supla::Sensor::VirtualImpulseCounter first;
  Supla::Sensor::VirtualImpulseCounter second;
  Supla::Html::ImpulseCounterParameters firstForm(&first, "IN1");
  Supla::Html::ImpulseCounterParameters secondForm(&second, "IN2");
  first.setCounter(12);
  second.setCounter(34);

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  second.generateKey(key, "ic_rate");
  EXPECT_FALSE(firstForm.handleResponse(key, "250"));
  EXPECT_TRUE(secondForm.handleResponse(key, "250"));
  EXPECT_EQ(first.getImpulsesPerUnit(), 1000);
  EXPECT_EQ(second.getImpulsesPerUnit(), 250);

  second.generateKey(key, "ic_reset");
  EXPECT_FALSE(firstForm.handleResponse(key, "RESET"));
  EXPECT_TRUE(secondForm.handleResponse(key, "RESET"));
  EXPECT_EQ(first.getCounter(), 12);
  EXPECT_EQ(second.getCounter(), 0);
}
