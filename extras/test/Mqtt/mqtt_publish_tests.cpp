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
#include <gmock/gmock.h>
#include <supla/protocol/mqtt.h>
#include <SuplaDevice.h>
#include <config_mock.h>
#include <network_with_mac_mock.h>
#include <supla/device/register_device.h>
#include <channel_element_mock.h>
#include <supla/sensor/electricity_meter.h>
#include <simple_time.h>

using testing::_;
using ::testing::SetArrayArgument;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;

class MqttPublishTests : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Supla::Channel::resetToDefaults();
  }
  virtual void TearDown() {
    Supla::Channel::resetToDefaults();
  }
};


class MqttPublishMock : public Supla::Protocol::Mqtt {
 public:
  explicit MqttPublishMock(SuplaDeviceClass *sdc) : Supla::Protocol::Mqtt(sdc) {
  }

  void disconnect() override {
  }

  bool iterate(uint32_t) override {
    return false;
  }

  MOCK_METHOD(void, publishImp, (const char *, const char *, int, bool));
  MOCK_METHOD(void, subscribeImp, (const char *, int));

  void test_generateClientId(char result[MQTT_CLIENTID_MAX_SIZE]) {
    generateClientId(result);
  }

  char *test_getPrefix() {
    return prefix;
  }
};

TEST_F(MqttPublishTests, powerBelow20kW) {
  ConfigMock config;
  NetworkMockWithMac net;
  SuplaDeviceClass sd;
  Supla::Sensor::ElectricityMeter el;
  ::testing::NiceMock<MqttPublishMock> mqtt(&sd);
  SimpleTime time;

  char cfgPrefix[] = "testowy_prefix";

  EXPECT_CALL(config, getMqttPrefix(_)).WillOnce(DoAll(
        SetArrayArgument<0>(cfgPrefix, cfgPrefix + strlen(cfgPrefix) + 1)
        , Return(true)));
  uint8_t mac[] = {1, 2, 3, 4, 5, 0xAB};
  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(DoAll(
        SetArrayArgument<0>(mac, mac + 6)
        , Return(true)));

  sd.setName("My Device");

  el.onInit();
  el.setFwdActEnergy(0, 1000);
  int64_t power = 20'000'000;
  int64_t powerReactive = 10'000;
  int64_t powerApparent = 30'000;
  el.setPowerActive(0, power);
  el.setPowerReactive(1, powerReactive);
  el.setPowerApparent(2, powerApparent);
  time.advance(10000);
  el.iterateAlways();

  mqtt.onInit();

  ASSERT_NE(mqtt.test_getPrefix(), nullptr);
  EXPECT_STREQ(mqtt.test_getPrefix(),
               "testowy_prefix/supla/devices/my-device-0405ab");

  EXPECT_CALL(mqtt,
              publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                         "channels/0/state/total_forward_active_energy"),
                         StrEq("0.0100"),
                         0,
                         false));
  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/total_forward_active_energy"),
                 StrEq("0.0100"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/total_forward_active_energy"),
                 StrEq("0.0000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/total_forward_active_energy"),
                 StrEq("0.0000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_active"),
                 StrEq("200.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_active"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_active"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_reactive"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_reactive"),
                 StrEq("0.100"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_reactive"),
                 StrEq("0.000"),
                 0,
                 false));


  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_apparent"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_apparent"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_apparent"),
                 StrEq("0.300"),
                 0,
                 false));

  mqtt.publishExtendedChannelState(0);
}

TEST_F(MqttPublishTests, powerAbove20kW) {
  ConfigMock config;
  NetworkMockWithMac net;
  SuplaDeviceClass sd;
  Supla::Sensor::ElectricityMeter el;
  ::testing::NiceMock<MqttPublishMock> mqtt(&sd);
  SimpleTime time;

  char cfgPrefix[] = "testowy_prefix";

  EXPECT_CALL(config, getMqttPrefix(_)).WillOnce(DoAll(
        SetArrayArgument<0>(cfgPrefix, cfgPrefix + strlen(cfgPrefix) + 1)
        , Return(true)));
  uint8_t mac[] = {1, 2, 3, 4, 5, 0xAB};
  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(DoAll(
        SetArrayArgument<0>(mac, mac + 6)
        , Return(true)));

  sd.setName("My Device");

  el.onInit();
  el.setFwdActEnergy(0, 1000);
  int64_t power = 20'000'000'000;
  el.setPowerActive(0, power);
  int64_t powerReactive = 30'000'000'000;
  int64_t powerApparent = 40'000'000'000;
  el.setPowerReactive(1, powerReactive);
  el.setPowerApparent(2, powerApparent);
  time.advance(10000);
  el.iterateAlways();

  mqtt.onInit();

  ASSERT_NE(mqtt.test_getPrefix(), nullptr);
  EXPECT_STREQ(mqtt.test_getPrefix(),
               "testowy_prefix/supla/devices/my-device-0405ab");

  EXPECT_CALL(mqtt,
              publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                         "channels/0/state/total_forward_active_energy"),
                         StrEq("0.0100"),
                         0,
                         false));
  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/total_forward_active_energy"),
                 StrEq("0.0100"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/total_forward_active_energy"),
                 StrEq("0.0000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/total_forward_active_energy"),
                 StrEq("0.0000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_active"),
                 StrEq("200000.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_active"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_active"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_reactive"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_reactive"),
                 StrEq("300000.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_reactive"),
                 StrEq("0.000"),
                 0,
                 false));


  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_apparent"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_apparent"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishImp(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_apparent"),
                 StrEq("400000.000"),
                 0,
                 false));

  mqtt.publishExtendedChannelState(0);
}

