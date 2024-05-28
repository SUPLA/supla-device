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
#include "supla/device/register_device.h"

using testing::_;
using ::testing::SetArrayArgument;
using ::testing::DoAll;
using ::testing::Return;

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

TEST(MqttPrefixTests, mqttgenerateClientId) {
  SuplaDeviceClass sd;
  MqttUT mqtt(&sd);

  char clientId[MQTT_CLIENTID_MAX_SIZE];

  // if client id max size change, please adjust generateClientId method
  ASSERT_EQ(MQTT_CLIENTID_MAX_SIZE, 23);

  char newGuid[SUPLA_GUID_SIZE];
  for (int i = 0; i <  16; i++) {
    newGuid[i] = i + 1;
  }
  Supla::RegisterDevice::setGUID(newGuid);

  mqtt.test_generateClientId(clientId);
  EXPECT_STREQ(clientId, "SUPLA-0102030405060708");

  for (int i = 0; i <  16; i++) {
    newGuid[i] = 16 - i;
  }
  Supla::RegisterDevice::setGUID(newGuid);

  mqtt.test_generateClientId(clientId);
  EXPECT_STREQ(clientId, "SUPLA-100F0E0D0C0B0A09");

  for (int i = 0; i <  16; i++) {
    newGuid[i] = 0;
  }
  Supla::RegisterDevice::setGUID(newGuid);
}


TEST(MqttPrefixTests, mqttgeneratePrefixNoNetworkNoConfig) {
  SuplaDeviceClass sd;
  MqttUT mqtt(&sd);

  sd.setName("Supla device");

  mqtt.onInit();  // onInit generates prefix

  ASSERT_NE(mqtt.test_getPrefix(), nullptr);
  EXPECT_STREQ(mqtt.test_getPrefix(), "supla/devices/supla-device");
}

TEST(MqttPrefixTests, mqttgeneratePrefixCustomName) {
  SuplaDeviceClass sd;
  MqttUT mqtt(&sd);
  sd.setName("My Device");

  mqtt.onInit();

  ASSERT_NE(mqtt.test_getPrefix(), nullptr);
  EXPECT_STREQ(mqtt.test_getPrefix(), "supla/devices/my-device");

  sd.setName("another NAME test");

  mqtt.onInit();

  ASSERT_NE(mqtt.test_getPrefix(), nullptr);
  EXPECT_STREQ(mqtt.test_getPrefix(), "supla/devices/another-name-test");
}


TEST(MqttPrefixTests, mqttgeneratePrefix) {
  SuplaDeviceClass sd;
  MqttUT mqtt(&sd);
  ConfigMock config;
  NetworkMockWithMac net;

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

}

