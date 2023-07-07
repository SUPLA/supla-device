/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <supla/protocol/mqtt.h>
#include <SuplaDevice.h>
#include <config_mock.h>
#include <network_with_mac_mock.h>
#include <channel_element_mock.h>

using testing::_;
using ::testing::SetArrayArgument;
using ::testing::DoAll;
using ::testing::Return;

class MqttProcessDataTests : public ::testing::Test {
  protected:
    virtual void SetUp() {
      Supla::Channel::lastCommunicationTimeMs = 0;
      memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
    }
    virtual void TearDown() {
      Supla::Channel::lastCommunicationTimeMs = 0;
      memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
    }

};

class MqttUT : public Supla::Protocol::Mqtt {
  public:
    explicit MqttUT(SuplaDeviceClass *sdc) : Supla::Protocol::Mqtt(sdc) {}
    void disconnect() override {}
    bool iterate(uint32_t millis) override {
      return false;
    }
    void publishImp(const char *topic,
                          const char *payload,
                          int qos,
                          bool retain) override {}
    void subscribeImp(const char *topic,
                      int qos) override {}

    void test_generateClientId(char result[MQTT_CLIENTID_MAX_SIZE]) {
      generateClientId(result);
    }

    char *test_getPrefix() {
      return prefix;
    }
};

TEST_F(MqttProcessDataTests, dataProcessTests) {
  SuplaDeviceClass sd;
  MqttUT mqtt(&sd);
  ConfigMock config;
  NetworkMockWithMac net;
  ChannelElementMock el1;
  ChannelElementMock el2;

  el1.getChannel()->setType(SUPLA_CHANNELTYPE_RELAY);
  el2.getChannel()->setType(SUPLA_CHANNELTYPE_RELAY);

  char cfgPrefix[] = "testowy_prefix";

  EXPECT_CALL(config, getMqttPrefix(_)).WillOnce(DoAll(
        SetArrayArgument<0>(cfgPrefix, cfgPrefix + strlen(cfgPrefix) + 1)
        , Return(true)));
  uint8_t mac[] = {1, 2, 3, 4, 5, 0xAB};
  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(DoAll(
        SetArrayArgument<0>(mac, mac + 6)
        , Return(true)));

  sd.setName("My Device");

  mqtt.onInit();

  ASSERT_NE(mqtt.test_getPrefix(), nullptr);
  EXPECT_STREQ(mqtt.test_getPrefix(),
               "testowy_prefix/supla/devices/my-device-0405ab");

  char topicWrongPrefix[] = "this/is/test/prefix/channels/3/set/on";
  char topicMissingChannels[] =
    "testowy_prefix/supla/devices/my-device-0405ab/just_do_it/3/set/on";
  char topicWrongChannel[] =
    "testowy_prefix/supla/devices/my-device-0405ab/channels/3/set/on";
  char topic0setOn[] =
    "testowy_prefix/supla/devices/my-device-0405ab/channels/0/set/on";
  char topic1setOn[] =
    "testowy_prefix/supla/devices/my-device-0405ab/channels/1/set/on";
  char payload[] = "true";
  char payloadFalse[] = "false";

  EXPECT_FALSE(mqtt.processData(nullptr, nullptr));
  EXPECT_FALSE(mqtt.processData(nullptr, payload));
  EXPECT_FALSE(mqtt.processData(topicWrongPrefix, nullptr));
  EXPECT_FALSE(mqtt.processData(topicWrongPrefix, payload));
  EXPECT_FALSE(mqtt.processData(topicMissingChannels, payload));
  EXPECT_FALSE(mqtt.processData(topicWrongChannel, payload));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(1, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_CALL(el2, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });
  EXPECT_TRUE(mqtt.processData(topic0setOn, payload));
  EXPECT_TRUE(mqtt.processData(topic1setOn, payloadFalse));

}

TEST_F(MqttProcessDataTests, relaySetOnTests) {
  SuplaDeviceClass sd;
  MqttUT mqtt(&sd);
  ConfigMock config;
  NetworkMockWithMac net;
  ChannelElementMock el1;

  el1.getChannel()->setType(SUPLA_CHANNELTYPE_RELAY);

  char cfgPrefix[] = "";

  EXPECT_CALL(config, getMqttPrefix(_)).WillOnce(DoAll(
        SetArrayArgument<0>(cfgPrefix, cfgPrefix + strlen(cfgPrefix) + 1)
        , Return(true)));
  uint8_t mac[] = {1, 2, 3, 4, 5, 0xAB};
  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(DoAll(
        SetArrayArgument<0>(mac, mac + 6)
        , Return(true)));

  sd.setName("My Device");

  mqtt.onInit();

  ASSERT_NE(mqtt.test_getPrefix(), nullptr);
  EXPECT_STREQ(mqtt.test_getPrefix(),
               "supla/devices/my-device-0405ab");

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(1, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "true"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "false"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(1, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "1"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(1, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "YES"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(1, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "True"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "11"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "yes!"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "truest"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "FALSE"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "0"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        ""));


  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(1, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/set/on",
        "tRuE"));

}

TEST_F(MqttProcessDataTests, executeActionTests) {
  SuplaDeviceClass sd;
  MqttUT mqtt(&sd);
  ConfigMock config;
  NetworkMockWithMac net;
  ChannelElementMock el1;

  el1.getChannel()->setType(SUPLA_CHANNELTYPE_RELAY);

  char cfgPrefix[] = "";

  EXPECT_CALL(config, getMqttPrefix(_)).WillOnce(DoAll(
        SetArrayArgument<0>(cfgPrefix, cfgPrefix + strlen(cfgPrefix) + 1)
        , Return(true)));
  uint8_t mac[] = {1, 2, 3, 4, 5, 0xAB};
  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(DoAll(
        SetArrayArgument<0>(mac, mac + 6)
        , Return(true)));

  sd.setName("My Device");

  mqtt.onInit();

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(1, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/execute_action",
        "turn_on"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/execute_action",
        "turn_Off"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(1, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });

  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/execute_action",
        "toggle"));

  EXPECT_CALL(el1, handleNewValueFromServer(_))
      .Times(1)
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(0, value->value[0]);
        EXPECT_EQ(0, value->value[1]);
        EXPECT_EQ(0, value->value[2]);
        EXPECT_EQ(0, value->value[3]);
        EXPECT_EQ(0, value->value[4]);
        EXPECT_EQ(0, value->value[5]);
        EXPECT_EQ(0, value->value[6]);
        EXPECT_EQ(0, value->value[7]);
        return 0;
      });


  Supla::Channel::reg_dev.channels[0].value[0] = 1;
  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/execute_action",
        "toGGle"));

  // invalid payload - no reaction
  EXPECT_TRUE(mqtt.processData(
        "supla/devices/my-device-0405ab/channels/0/execute_action",
        "land_on_mars"));

}
