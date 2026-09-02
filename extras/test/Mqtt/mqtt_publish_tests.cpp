// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

#include <cstring>
#include <string>
#include <vector>

#include "../doubles/mqtt_mock.h"

using testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::SetArrayArgument;
using ::testing::DoAll;
using ::testing::HasSubstr;
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


class MqttPublishMock : public MqttMock {
 public:
  explicit MqttPublishMock(SuplaDeviceClass *sdc) : MqttMock(sdc) {
  }

  void test_generateClientId(char result[MQTT_CLIENTID_MAX_SIZE]) {
    generateClientId(result);
  }

  char *test_getPrefix() {
    return prefix;
  }
};

TEST_F(MqttPublishTests, powerBelow20kW) {
  ConfigMock config;
  EXPECT_CALL(config, init());
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
              publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                         "channels/0/state/total_forward_active_energy"),
                         StrEq("0.0100"),
                         0,
                         false));
  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/total_forward_active_energy"),
                 StrEq("0.0100"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/total_forward_active_energy"),
                 StrEq("0.0000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/total_forward_active_energy"),
                 StrEq("0.0000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_active"),
                 StrEq("200.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_active"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_active"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_reactive"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_reactive"),
                 StrEq("0.100"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_reactive"),
                 StrEq("0.000"),
                 0,
                 false));


  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_apparent"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_apparent"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_apparent"),
                 StrEq("0.300"),
                 0,
                 false));

  {
    MQTT_DOC_SCENARIO(
        mqtt.documentationRecorder(),
        "electricity_meter.basic_state",
        "Basic total and per-phase electricity meter state topics",
        SUPLA_CHANNELTYPE_ELECTRICITY_METER,
        SUPLA_CHANNELFNC_ELECTRICITY_METER,
        0,
        "public",
        "testowy_prefix/supla/devices/my-device-0405ab");
    mqtt.publishExtendedChannelState(0);
  }
}

TEST_F(MqttPublishTests, powerAbove20kW) {
  ConfigMock config;
  EXPECT_CALL(config, init());
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
              publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                         "channels/0/state/total_forward_active_energy"),
                         StrEq("0.0100"),
                         0,
                         false));
  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/total_forward_active_energy"),
                 StrEq("0.0100"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/total_forward_active_energy"),
                 StrEq("0.0000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/total_forward_active_energy"),
                 StrEq("0.0000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_active"),
                 StrEq("200000.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_active"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_active"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_reactive"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_reactive"),
                 StrEq("300000.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_reactive"),
                 StrEq("0.000"),
                 0,
                 false));


  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/1/power_apparent"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/2/power_apparent"),
                 StrEq("0.000"),
                 0,
                 false));

  EXPECT_CALL(mqtt,
      publishTest(StrEq("testowy_prefix/supla/devices/my-device-0405ab/"
                 "channels/0/state/phases/3/power_apparent"),
                 StrEq("400000.000"),
                 0,
                 false));

  mqtt.publishExtendedChannelState(0);
}

TEST_F(MqttPublishTests, publishElectricityMeterEnergyAndGlobalMeasurements) {
  ConfigMock config;
  EXPECT_CALL(config, init());
  NetworkMockWithMac net;
  SuplaDeviceClass sd;
  Supla::Sensor::ElectricityMeter em;
  ::testing::NiceMock<MqttPublishMock> mqtt(&sd);
  SimpleTime time;

  char cfgPrefix[] = "testowy_prefix";
  EXPECT_CALL(config, getMqttPrefix(_))
      .WillOnce(DoAll(
          SetArrayArgument<0>(cfgPrefix, cfgPrefix + strlen(cfgPrefix) + 1),
          Return(true)));
  uint8_t mac[] = {1, 2, 3, 4, 5, 0xAB};
  EXPECT_CALL(net, getMacAddr(_))
      .WillRepeatedly(DoAll(SetArrayArgument<0>(mac, mac + 6), Return(true)));

  sd.setName("My Device");
  em.onInit();
  em.setFwdActEnergy(0, 1000);
  em.setRvrActEnergy(0, 2000);
  em.setFwdBalancedEnergy(3000);
  em.setRvrBalancedEnergy(4000);
  em.setVoltagePhaseAngle12(1200);
  em.setVoltagePhaseAngle13(2400);
  em.setVoltagePhaseSequence(true);
  em.setCurrentPhaseSequence(false);
  time.advance(10000);
  em.iterateAlways();
  mqtt.onInit();

  const std::string prefix =
      "testowy_prefix/supla/devices/my-device-0405ab/channels/0/state/";
  EXPECT_CALL(mqtt, publishTest(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(mqtt, publishTest(StrEq(prefix + "total_reverse_active_energy"),
                                StrEq("0.0200"),
                                0,
                                false));
  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(prefix + "total_forward_balanced_active_energy"),
                  StrEq("0.0300"),
                  0,
                  false));
  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(prefix + "total_reverse_balanced_active_energy"),
                  StrEq("0.0400"),
                  0,
                  false));
  EXPECT_CALL(mqtt, publishTest(StrEq(prefix + "voltage_phase_angle_12"),
                                StrEq("120.0"),
                                0,
                                false));
  EXPECT_CALL(mqtt, publishTest(StrEq(prefix + "voltage_phase_angle_13"),
                                StrEq("240.0"),
                                0,
                                false));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(prefix + "voltage_phase_sequence_clockwise"),
                                StrEq("true"),
                                0,
                                false));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(prefix + "current_phase_sequence_clockwise"),
                                StrEq("false"),
                                0,
                                false));

  {
    MQTT_DOC_SCENARIO(
        mqtt.documentationRecorder(),
        "electricity_meter.energy_and_global_state",
        "Electricity meter energy totals and global measurements",
        SUPLA_CHANNELTYPE_ELECTRICITY_METER,
        SUPLA_CHANNELFNC_ELECTRICITY_METER,
        0,
        "public",
        "testowy_prefix/supla/devices/my-device-0405ab");
    mqtt.publishExtendedChannelState(0);
  }
}

TEST_F(MqttPublishTests, publishElectricityMeterAllPerPhaseMeasurements) {
  ConfigMock config;
  EXPECT_CALL(config, init());
  NetworkMockWithMac net;
  SuplaDeviceClass sd;
  Supla::Sensor::ElectricityMeter em;
  ::testing::NiceMock<MqttPublishMock> mqtt(&sd);
  SimpleTime time;

  char cfgPrefix[] = "testowy_prefix";
  EXPECT_CALL(config, getMqttPrefix(_))
      .WillOnce(DoAll(
          SetArrayArgument<0>(cfgPrefix, cfgPrefix + strlen(cfgPrefix) + 1),
          Return(true)));
  uint8_t mac[] = {1, 2, 3, 4, 5, 0xAB};
  EXPECT_CALL(net, getMacAddr(_))
      .WillRepeatedly(DoAll(SetArrayArgument<0>(mac, mac + 6), Return(true)));

  sd.setName("My Device");
  em.onInit();
  em.setRvrActEnergy(0, 2000);
  em.setFwdReactEnergy(0, 3000);
  em.setRvrReactEnergy(0, 4000);
  em.setVoltage(0, 23000);
  em.setCurrent(0, 1234);
  em.setPowerFactor(0, 987);
  em.setPhaseAngle(0, 123);
  em.setFreq(5000);
  time.advance(10000);
  em.iterateAlways();
  mqtt.onInit();

  const std::vector<const char *> phaseTopics = {
      "total_reverse_active_energy",
      "total_forward_reactive_energy",
      "total_reverse_reactive_energy",
      "voltage",
      "current",
      "power_factor",
      "phase_angle",
      "frequency",
  };
  EXPECT_CALL(mqtt, publishTest(_, _, _, _)).Times(AnyNumber());
  for (const auto *suffix : phaseTopics) {
    EXPECT_CALL(mqtt,
                publishTest(AllOf(HasSubstr("/state/phases/"),
                                  HasSubstr(std::string("/") + suffix)),
                            _,
                            0,
                            false))
        .Times(3);
  }

  {
    MQTT_DOC_SCENARIO(
        mqtt.documentationRecorder(),
        "electricity_meter.per_phase_state",
        "Optional per-phase electricity meter measurements",
        SUPLA_CHANNELTYPE_ELECTRICITY_METER,
        SUPLA_CHANNELFNC_ELECTRICITY_METER,
        0,
        "public",
        "testowy_prefix/supla/devices/my-device-0405ab");
    mqtt.publishExtendedChannelState(0);
  }
}
