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

#include <SuplaDevice.h>
#include <channel_element_mock.h>
#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_with_mac_mock.h>
#include <output_mock.h>
#include <simple_time.h>
#include <supla/actions.h>
#include <supla/control/hvac_base.h>
#include <supla/control/relay_roller_shutter_pair.h>
#include <supla/protocol/mqtt.h>
#include <supla/protocol/mqtt/hvac_mqtt.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <supla/sensor/thermometer.h>
#include <supla/tools.h>

#include <cstring>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

#include "../doubles/mqtt_mock.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace {

constexpr char kCfgPrefix[] = "prefix";
constexpr uint8_t kMac[] = {1, 2, 3, 4, 5, 0xAB};
constexpr char kExpectedPrefix[] = "prefix/supla/devices/my-device-0405ab";
constexpr char kExpectedObjectPrefix[] = "000000000000";

class MqttChannelDispatchTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }
};

class MqttTestMock : public MqttMock {
 public:
  explicit MqttTestMock(SuplaDeviceClass *sdc) : MqttMock(sdc) {
  }

  using Supla::Protocol::Mqtt::publishHADiscovery;

  void test_setChannelsCount(uint16_t count) {
    channelsCount = count;
  }
};

void initMqtt(SuplaDeviceClass &sd, Supla::Protocol::Mqtt &mqtt) {
  ConfigMock config;
  EXPECT_CALL(config, init());

  NetworkMockWithMac net;
  EXPECT_CALL(config, getMqttPrefix(_))
      .WillOnce(DoAll(
          SetArrayArgument<0>(kCfgPrefix, kCfgPrefix + strlen(kCfgPrefix) + 1),
          Return(true)));
  EXPECT_CALL(net, getMacAddr(_))
      .WillRepeatedly(DoAll(SetArrayArgument<0>(kMac, kMac + 6), Return(true)));

  sd.setName("My Device");

  mqtt.onInit();
  Supla::Protocol::RegisterHvacMqttHandler();
}

std::string expectedChannelTopic(int channel, const char *suffix) {
  return std::string(kExpectedPrefix) + "/channels/" + std::to_string(channel) +
         "/" + suffix;
}

std::string expectedDiscoveryTopic(const char *type,
                                   int channel,
                                   int subId = 0) {
  return std::string("homeassistant/") + type + "/supla/" +
         kExpectedObjectPrefix + "_" + std::to_string(channel) + "_" +
         std::to_string(subId) + "/config";
}

nlohmann::json baseDiscoveryPayload(int channel) {
  return nlohmann::json{
      {"avty_t", std::string(kExpectedPrefix) + "/state/connected"},
      {"pl_avail", "true"},
      {"pl_not_avail", "false"},
      {"~",
       std::string(kExpectedPrefix) + "/channels/" + std::to_string(channel)},
      {"dev",
       {{"ids", "my-device-0405ab"},
        {"mf", "Unknown"},
        {"name", "My Device"},
        {"sw", ""}}},
  };
}

std::string jsonToString(const nlohmann::json &value);

std::string jsonToString(const nlohmann::json &value) {
  return value.dump();
}

std::optional<std::string> jsonFirstDiff(const nlohmann::json &expected,
                                         const nlohmann::json &actual,
                                         const std::string &path = "$") {
  if (expected.is_object() && actual.is_object()) {
    for (auto it = expected.begin(); it != expected.end(); ++it) {
      const auto &key = it.key();
      if (!actual.contains(key)) {
        return path + "." + key + " missing in actual";
      }
      auto diff = jsonFirstDiff(it.value(), actual.at(key), path + "." + key);
      if (diff.has_value()) {
        return diff;
      }
    }
    for (auto it = actual.begin(); it != actual.end(); ++it) {
      const auto &key = it.key();
      if (!expected.contains(key)) {
        return path + "." + key + " unexpected in actual";
      }
    }
    return std::nullopt;
  }

  if (expected.is_array() && actual.is_array()) {
    if (expected.size() != actual.size()) {
      return path + " size mismatch expected " +
             std::to_string(expected.size()) + " actual " +
             std::to_string(actual.size());
    }
    for (size_t i = 0; i < expected.size(); i++) {
      auto diff = jsonFirstDiff(expected.at(i),
                                actual.at(i),
                                path + "[" + std::to_string(i) + "]");
      if (diff.has_value()) {
        return diff;
      }
    }
    return std::nullopt;
  }

  if (expected == actual) {
    return std::nullopt;
  }

  if (expected.is_null() != actual.is_null() ||
      expected.is_string() != actual.is_string() ||
      expected.is_boolean() != actual.is_boolean() ||
      expected.is_number() != actual.is_number() ||
      expected.is_array() != actual.is_array() ||
      expected.is_object() != actual.is_object()) {
    return path + " type mismatch expected " + jsonToString(expected) +
           " actual " + jsonToString(actual);
  }

  return path + " expected " + jsonToString(expected) + " actual " +
         jsonToString(actual);
}

class JsonEqMatcher {
 public:
  explicit JsonEqMatcher(std::string expected_json)
      : expected_json_(std::move(expected_json)) {
  }

  bool MatchAndExplain(const std::string &arg,
                       ::testing::MatchResultListener *listener) const {
    try {
      auto actual = nlohmann::json::parse(arg);
      auto expected = nlohmann::json::parse(expected_json_);
      auto diff = jsonFirstDiff(expected, actual);
      if (!diff.has_value()) {
        return true;
      }
      *listener << diff.value();
      return false;
    } catch (const std::exception &e) {
      *listener << "json parse failed: " << e.what();
      return false;
    }
  }

  void DescribeTo(std::ostream *os) const {
    *os << "JSON payload matches expected discovery payload";
  }

  void DescribeNegationTo(std::ostream *os) const {
    *os << "JSON payload differs from expected discovery payload";
  }

 private:
  std::string expected_json_;
};

inline ::testing::PolymorphicMatcher<JsonEqMatcher> JsonEq(
    std::string expected_json) {
  return ::testing::MakePolymorphicMatcher(
      JsonEqMatcher(std::move(expected_json)));
}

void configureRelay(Supla::Channel *ch, uint32_t defaultFunction) {
  ch->setType(SUPLA_CHANNELTYPE_RELAY);
  ch->setDefaultFunction(defaultFunction);
}

void configureRollerRelay(Supla::Channel *ch, uint32_t defaultFunction) {
  ch->setType(SUPLA_CHANNELTYPE_RELAY);
  ch->setDefaultFunction(defaultFunction);
  ch->setFuncList(SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER);
}

}  // namespace

TEST_F(MqttChannelDispatchTests, publishChannelStateCoversBasicTypes) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  ChannelElementMock relay;
  configureRelay(relay.getChannel(), SUPLA_CHANNELFNC_POWERSWITCH);
  relay.getChannel()->setNewValue(true);

  ChannelElementMock roller;
  configureRollerRelay(roller.getChannel(),
                       SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  TDSC_RollerShutterValue rollerValue = {};
  rollerValue.position = 33;
  roller.getChannel()->setNewValue(rollerValue);

  ChannelElementMock thermometer;
  thermometer.getChannel()->setType(SUPLA_CHANNELTYPE_THERMOMETER);
  thermometer.getChannel()->setNewValue(21.5);

  ChannelElementMock humidityAndTemp;
  humidityAndTemp.getChannel()->setType(
      SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR);
  humidityAndTemp.getChannel()->setNewValue(21.5, 55.0);

  ChannelElementMock dimmer;
  dimmer.getChannel()->setType(SUPLA_CHANNELTYPE_DIMMER);
  dimmer.getChannel()->setNewValue(42);

  ChannelElementMock rgb;
  rgb.getChannel()->setType(SUPLA_CHANNELTYPE_RGBLEDCONTROLLER);
  rgb.getChannel()->setNewValue(1, 2, 3, 4, 0, 0);

  ChannelElementMock dimmerAndRgb;
  dimmerAndRgb.getChannel()->setType(SUPLA_CHANNELTYPE_DIMMERANDRGBLED);
  dimmerAndRgb.getChannel()->setNewValue(5, 6, 7, 11, 8, 0);

  mqtt.test_setChannelsCount(255);

  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(relay.getChannelNumber(),
                                                     "state/on")),
                          StrEq("true"),
                          0,
                          true));
  mqtt.publishChannelState(relay.getChannelNumber());

  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(roller.getChannelNumber(),
                                                     "state/is_calibrating")),
                          StrEq("false"),
                          0,
                          true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(roller.getChannelNumber(),
                                                     "state/shut")),
                          StrEq("33"),
                          0,
                          true));
  mqtt.publishChannelState(roller.getChannelNumber());

  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(expectedChannelTopic(thermometer.getChannelNumber(),
                                             "state/temperature")),
                  _,
                  0,
                  true));
  mqtt.publishChannelState(thermometer.getChannelNumber());

  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(expectedChannelTopic(humidityAndTemp.getChannelNumber(),
                                             "state/temperature")),
                  _,
                  0,
                  true));
  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(expectedChannelTopic(humidityAndTemp.getChannelNumber(),
                                             "state/humidity")),
                  _,
                  0,
                  true));
  mqtt.publishChannelState(humidityAndTemp.getChannelNumber());

  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(dimmer.getChannelNumber(),
                                                     "state/brightness")),
                          StrEq("42"),
                          0,
                          true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(dimmer.getChannelNumber(),
                                                     "state/on")),
                          StrEq("true"),
                          0,
                          true));
  mqtt.publishChannelState(dimmer.getChannelNumber());

  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(rgb.getChannelNumber(),
                                                     "state/color_brightness")),
                          StrEq("4"),
                          0,
                          true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(rgb.getChannelNumber(),
                                                     "state/on")),
                          StrEq("true"),
                          0,
                          true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(rgb.getChannelNumber(),
                                                     "state/color")),
                          StrEq("1,2,3"),
                          0,
                          true));
  mqtt.publishChannelState(rgb.getChannelNumber());

  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(expectedChannelTopic(dimmerAndRgb.getChannelNumber(),
                                             "state/brightness")),
                  StrEq("8"),
                  0,
                  true));
  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(expectedChannelTopic(dimmerAndRgb.getChannelNumber(),
                                             "state/color_brightness")),
                  StrEq("11"),
                  0,
                  true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(
                              dimmerAndRgb.getChannelNumber(), "state/rgb/on")),
                          StrEq("true"),
                          0,
                          true));
  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(expectedChannelTopic(dimmerAndRgb.getChannelNumber(),
                                             "state/dimmer/on")),
                  StrEq("true"),
                  0,
                  true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(
                              dimmerAndRgb.getChannelNumber(), "state/color")),
                          StrEq("5,6,7"),
                          0,
                          true));
  mqtt.publishChannelState(dimmerAndRgb.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, publishChannelStateCoversHvacAndBinarySensor) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  ChannelElementMock hvac;
  hvac.getChannel()->setType(SUPLA_CHANNELTYPE_HVAC);
  hvac.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
  hvac.getChannel()->setHvacFlagHeating(true);
  hvac.getChannel()->setHvacMode(SUPLA_HVAC_MODE_HEAT_COOL);
  hvac.getChannel()->setHvacSetpointTemperatureHeat(2100);
  hvac.getChannel()->setHvacSetpointTemperatureCool(2300);

  ChannelElementMock binarySensorClosed;
  binarySensorClosed.getChannel()->setType(SUPLA_CHANNELTYPE_BINARYSENSOR);
  binarySensorClosed.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR);
  binarySensorClosed.getChannel()->setNewValue(true);

  ChannelElementMock binarySensorOpen;
  binarySensorOpen.getChannel()->setType(SUPLA_CHANNELTYPE_BINARYSENSOR);
  binarySensorOpen.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR);
  binarySensorOpen.getChannel()->setNewValue(false);

  mqtt.test_setChannelsCount(255);

  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(hvac.getChannelNumber(),
                                                     "state/action")),
                          StrEq("heating"),
                          0,
                          true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(hvac.getChannelNumber(),
                                                     "state/mode")),
                          StrEq("heat_cool"),
                          0,
                          true));
  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(expectedChannelTopic(hvac.getChannelNumber(),
                                             "state/temperature_setpoint")),
                  StrEq(""),
                  0,
                  true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(
                              hvac.getChannelNumber(),
                              "state/temperature_setpoint_heat")),
                          StrEq("21.00"),
                          0,
                          true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(
                              hvac.getChannelNumber(),
                              "state/temperature_setpoint_cool")),
                          StrEq("23.00"),
                          0,
                          true));
  mqtt.publishChannelState(hvac.getChannelNumber());

  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(
                              binarySensorClosed.getChannelNumber(), "state")),
                          StrEq("closed"),
                          0,
                          true));
  mqtt.publishChannelState(binarySensorClosed.getChannelNumber());

  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedChannelTopic(
                              binarySensorOpen.getChannelNumber(), "state")),
                          StrEq("open"),
                          0,
                          true));
  mqtt.publishChannelState(binarySensorOpen.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, publishChannelStateActionTriggerIsNoop) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  ChannelElementMock actionTrigger;
  actionTrigger.getChannel()->setType(SUPLA_CHANNELTYPE_ACTIONTRIGGER);
  actionTrigger.getChannel()->setActionTriggerCaps(SUPLA_ACTION_CAP_HOLD);

  mqtt.test_setChannelsCount(255);

  EXPECT_CALL(mqtt, publishTest(_, _, _, _)).Times(0);
  mqtt.publishChannelState(actionTrigger.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, subscribeChannelCoversControllableTypes) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  ChannelElementMock relay;
  configureRelay(relay.getChannel(), SUPLA_CHANNELFNC_POWERSWITCH);

  ChannelElementMock roller;
  configureRollerRelay(roller.getChannel(),
                       SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);

  ChannelElementMock dimmer;
  dimmer.getChannel()->setType(SUPLA_CHANNELTYPE_DIMMER);

  ChannelElementMock rgb;
  rgb.getChannel()->setType(SUPLA_CHANNELTYPE_RGBLEDCONTROLLER);

  ChannelElementMock dimmerAndRgb;
  dimmerAndRgb.getChannel()->setType(SUPLA_CHANNELTYPE_DIMMERANDRGBLED);

  ChannelElementMock hvac;
  hvac.getChannel()->setType(SUPLA_CHANNELTYPE_HVAC);

  mqtt.test_setChannelsCount(255);

  EXPECT_CALL(
      mqtt,
      subscribeImp(
          StrEq(expectedChannelTopic(relay.getChannelNumber(), "set/on")), 0));
  EXPECT_CALL(mqtt,
              subscribeImp(StrEq(expectedChannelTopic(relay.getChannelNumber(),
                                                      "execute_action")),
                           0));
  mqtt.subscribeChannel(relay.getChannelNumber());

  EXPECT_CALL(
      mqtt,
      subscribeImp(StrEq(expectedChannelTopic(roller.getChannelNumber(),
                                              "set/closing_percentage")),
                   0));
  EXPECT_CALL(mqtt,
              subscribeImp(StrEq(expectedChannelTopic(roller.getChannelNumber(),
                                                      "set/tilt")),
                           0));
  EXPECT_CALL(mqtt,
              subscribeImp(StrEq(expectedChannelTopic(roller.getChannelNumber(),
                                                      "execute_action")),
                           0));
  mqtt.subscribeChannel(roller.getChannelNumber());

  EXPECT_CALL(mqtt,
              subscribeImp(StrEq(expectedChannelTopic(dimmer.getChannelNumber(),
                                                      "execute_action")),
                           0));
  EXPECT_CALL(mqtt,
              subscribeImp(StrEq(expectedChannelTopic(dimmer.getChannelNumber(),
                                                      "set/brightness")),
                           0));
  mqtt.subscribeChannel(dimmer.getChannelNumber());

  EXPECT_CALL(mqtt,
              subscribeImp(StrEq(expectedChannelTopic(rgb.getChannelNumber(),
                                                      "execute_action")),
                           0));
  EXPECT_CALL(mqtt,
              subscribeImp(StrEq(expectedChannelTopic(rgb.getChannelNumber(),
                                                      "set/color_brightness")),
                           0));
  EXPECT_CALL(
      mqtt,
      subscribeImp(
          StrEq(expectedChannelTopic(rgb.getChannelNumber(), "set/color")), 0));
  mqtt.subscribeChannel(rgb.getChannelNumber());

  EXPECT_CALL(
      mqtt,
      subscribeImp(StrEq(expectedChannelTopic(dimmerAndRgb.getChannelNumber(),
                                              "execute_action/rgb")),
                   0));
  EXPECT_CALL(
      mqtt,
      subscribeImp(StrEq(expectedChannelTopic(dimmerAndRgb.getChannelNumber(),
                                              "execute_action/dimmer")),
                   0));
  EXPECT_CALL(
      mqtt,
      subscribeImp(StrEq(expectedChannelTopic(dimmerAndRgb.getChannelNumber(),
                                              "set/brightness")),
                   0));
  EXPECT_CALL(
      mqtt,
      subscribeImp(StrEq(expectedChannelTopic(dimmerAndRgb.getChannelNumber(),
                                              "set/color_brightness")),
                   0));
  EXPECT_CALL(mqtt,
              subscribeImp(StrEq(expectedChannelTopic(
                               dimmerAndRgb.getChannelNumber(), "set/color")),
                           0));
  mqtt.subscribeChannel(dimmerAndRgb.getChannelNumber());

  EXPECT_CALL(mqtt,
              subscribeImp(StrEq(expectedChannelTopic(hvac.getChannelNumber(),
                                                      "execute_action")),
                           0));
  EXPECT_CALL(
      mqtt,
      subscribeImp(StrEq(expectedChannelTopic(hvac.getChannelNumber(),
                                              "set/temperature_setpoint")),
                   0));
  EXPECT_CALL(
      mqtt,
      subscribeImp(StrEq(expectedChannelTopic(hvac.getChannelNumber(),
                                              "set/temperature_setpoint_heat")),
                   0));
  EXPECT_CALL(
      mqtt,
      subscribeImp(StrEq(expectedChannelTopic(hvac.getChannelNumber(),
                                              "set/temperature_setpoint_cool")),
                   0));
  mqtt.subscribeChannel(hvac.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, subscribeChannelSkipsReadOnlyTypes) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  ChannelElementMock thermometer;
  thermometer.getChannel()->setType(SUPLA_CHANNELTYPE_THERMOMETER);

  ChannelElementMock humidityAndTemp;
  humidityAndTemp.getChannel()->setType(
      SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR);

  ChannelElementMock electricityMeter;
  electricityMeter.getChannel()->setType(SUPLA_CHANNELTYPE_ELECTRICITY_METER);

  ChannelElementMock actionTrigger;
  actionTrigger.getChannel()->setType(SUPLA_CHANNELTYPE_ACTIONTRIGGER);

  ChannelElementMock binarySensor;
  binarySensor.getChannel()->setType(SUPLA_CHANNELTYPE_BINARYSENSOR);

  mqtt.test_setChannelsCount(255);

  EXPECT_CALL(mqtt, subscribeImp(_, _)).Times(0);
  mqtt.subscribeChannel(thermometer.getChannelNumber());
  mqtt.subscribeChannel(humidityAndTemp.getChannelNumber());
  mqtt.subscribeChannel(electricityMeter.getChannelNumber());
  mqtt.subscribeChannel(actionTrigger.getChannelNumber());
  mqtt.subscribeChannel(binarySensor.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, publishHADiscoveryCoversChannelTypes) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  ChannelElementMock relay;
  configureRelay(relay.getChannel(), SUPLA_CHANNELFNC_POWERSWITCH);

  ChannelElementMock roller;
  configureRollerRelay(roller.getChannel(),
                       SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);

  ChannelElementMock thermometer;
  thermometer.getChannel()->setType(SUPLA_CHANNELTYPE_THERMOMETER);

  ChannelElementMock humidityAndTemp;
  humidityAndTemp.getChannel()->setType(
      SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR);

  ChannelElementMock dimmer;
  dimmer.getChannel()->setType(SUPLA_CHANNELTYPE_DIMMER);

  ChannelElementMock rgb;
  rgb.getChannel()->setType(SUPLA_CHANNELTYPE_RGBLEDCONTROLLER);

  ChannelElementMock dimmerAndRgb;
  dimmerAndRgb.getChannel()->setType(SUPLA_CHANNELTYPE_DIMMERANDRGBLED);

  ChannelElementMock binarySensor;
  binarySensor.getChannel()->setType(SUPLA_CHANNELTYPE_BINARYSENSOR);
  binarySensor.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR);

  ChannelElementMock actionTrigger;
  actionTrigger.getChannel()->setType(SUPLA_CHANNELTYPE_ACTIONTRIGGER);
  actionTrigger.getChannel()->setActionTriggerCaps(SUPLA_ACTION_CAP_HOLD);

  mqtt.test_setChannelsCount(255);

  auto baseDiscovery = [&](int channel) {
    return nlohmann::json{
        {"avty_t", std::string(kExpectedPrefix) + "/state/connected"},
        {"pl_avail", "true"},
        {"pl_not_avail", "false"},
        {"~",
         std::string(kExpectedPrefix) + "/channels/" + std::to_string(channel)},
        {"dev",
         {{"ids", "my-device-0405ab"},
          {"mf", "Unknown"},
          {"name", "My Device"},
          {"sw", ""}}},
    };
  };

  auto relayPayload = baseDiscovery(relay.getChannelNumber());
  relayPayload["name"] =
      std::string("#") + std::to_string(relay.getChannelNumber()) + " " +
      Supla::getRelayChannelName(SUPLA_CHANNELFNC_POWERSWITCH);
  relayPayload["uniq_id"] = std::string("supla_") + kExpectedObjectPrefix +
                            "_" + std::to_string(relay.getChannelNumber()) +
                            "_0";
  relayPayload["qos"] = 0;
  relayPayload["ret"] = false;
  relayPayload["opt"] = false;
  relayPayload["stat_t"] = "~/state/on";
  relayPayload["cmd_t"] = "~/set/on";
  relayPayload["pl_on"] = "true";
  relayPayload["pl_off"] = "false";
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "switch", relay.getChannelNumber(), 0)),
                          JsonEq(jsonToString(relayPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(relay.getChannelNumber());

  auto rollerPayload = baseDiscovery(roller.getChannelNumber());
  rollerPayload["name"] =
      std::string("#") + std::to_string(roller.getChannelNumber()) + " " +
      Supla::getRelayChannelName(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  rollerPayload["uniq_id"] = std::string("supla_") + kExpectedObjectPrefix +
                             "_" + std::to_string(roller.getChannelNumber()) +
                             "_0";
  rollerPayload["qos"] = 0;
  rollerPayload["ret"] = false;
  rollerPayload["opt"] = false;
  rollerPayload["cmd_t"] = "~/execute_action";
  rollerPayload["pl_open"] = "REVEAL";
  rollerPayload["pl_cls"] = "SHUT";
  rollerPayload["pl_stop"] = "STOP";
  rollerPayload["set_pos_t"] = "~/set/closing_percentage";
  rollerPayload["pos_t"] = "~/state/shut";
  rollerPayload["pos_open"] = 0;
  rollerPayload["pos_clsd"] = 100;
  rollerPayload["pos_tpl"] =
      "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int "
      "> 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}";
  rollerPayload["dev_cla"] = "shutter";
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "cover", roller.getChannelNumber(), 0)),
                          JsonEq(jsonToString(rollerPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(roller.getChannelNumber());

  auto thermometerPayload = baseDiscovery(thermometer.getChannelNumber());
  thermometerPayload["name"] = std::string("#") +
                               std::to_string(thermometer.getChannelNumber()) +
                               " Temperature";
  thermometerPayload["uniq_id"] =
      std::string("supla_") + kExpectedObjectPrefix + "_" +
      std::to_string(thermometer.getChannelNumber()) + "_0";
  thermometerPayload["dev_cla"] = "temperature";
  thermometerPayload["unit_of_meas"] = "°C";
  thermometerPayload["stat_cla"] = "measurement";
  thermometerPayload["expire_after"] = 30;
  thermometerPayload["qos"] = 0;
  thermometerPayload["ret"] = false;
  thermometerPayload["opt"] = false;
  thermometerPayload["stat_t"] = "~/state/temperature";
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "sensor", thermometer.getChannelNumber(), 0)),
                          JsonEq(jsonToString(thermometerPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(thermometer.getChannelNumber());

  auto humidityPayload = baseDiscovery(humidityAndTemp.getChannelNumber());
  humidityPayload["name"] = std::string("#") +
                            std::to_string(humidityAndTemp.getChannelNumber()) +
                            " Humidity";
  humidityPayload["dev_cla"] = "humidity";
  humidityPayload["stat_cla"] = "measurement";
  humidityPayload["unit_of_meas"] = "%";
  humidityPayload["uniq_id"] =
      std::string("supla_") + kExpectedObjectPrefix + "_" +
      std::to_string(humidityAndTemp.getChannelNumber()) + "_0";
  humidityPayload["expire_after"] = 30;
  humidityPayload["qos"] = 0;
  humidityPayload["ret"] = false;
  humidityPayload["opt"] = false;
  humidityPayload["stat_t"] = "~/state/humidity";
  auto humidityTemperaturePayload =
      baseDiscovery(humidityAndTemp.getChannelNumber());
  humidityTemperaturePayload["name"] =
      std::string("#") + std::to_string(humidityAndTemp.getChannelNumber()) +
      " Temperature";
  humidityTemperaturePayload["uniq_id"] =
      std::string("supla_") + kExpectedObjectPrefix + "_" +
      std::to_string(humidityAndTemp.getChannelNumber()) + "_1";
  humidityTemperaturePayload["dev_cla"] = "temperature";
  humidityTemperaturePayload["unit_of_meas"] = "°C";
  humidityTemperaturePayload["stat_cla"] = "measurement";
  humidityTemperaturePayload["expire_after"] = 30;
  humidityTemperaturePayload["qos"] = 0;
  humidityTemperaturePayload["ret"] = false;
  humidityTemperaturePayload["opt"] = false;
  humidityTemperaturePayload["stat_t"] = "~/state/temperature";
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "sensor", humidityAndTemp.getChannelNumber(), 1)),
                          JsonEq(jsonToString(humidityTemperaturePayload)),
                          0,
                          true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "sensor", humidityAndTemp.getChannelNumber(), 0)),
                          JsonEq(jsonToString(humidityPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(humidityAndTemp.getChannelNumber());

  auto dimmerPayload = baseDiscovery(dimmer.getChannelNumber());
  dimmerPayload["name"] = "Dimmer";
  dimmerPayload["uniq_id"] = std::string("supla_") + kExpectedObjectPrefix +
                             "_" + std::to_string(dimmer.getChannelNumber()) +
                             "_0";
  dimmerPayload["qos"] = 0;
  dimmerPayload["ret"] = false;
  dimmerPayload["opt"] = false;
  dimmerPayload["stat_t"] = "~/state/on";
  dimmerPayload["cmd_t"] = "~/execute_action";
  dimmerPayload["pl_on"] = "TURN_ON";
  dimmerPayload["pl_off"] = "TURN_OFF";
  dimmerPayload["stat_val_tpl"] =
      "{% if value == \"true\" %}TURN_ON{% else %}TURN_OFF{% endif %}";
  dimmerPayload["on_cmd_type"] = "last";
  dimmerPayload["bri_cmd_t"] = "~/set/brightness";
  dimmerPayload["bri_scl"] = 100;
  dimmerPayload["bri_stat_t"] = "~/state/brightness";
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "light", dimmer.getChannelNumber(), 0)),
                          JsonEq(jsonToString(dimmerPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(dimmer.getChannelNumber());

  auto rgbPayload = baseDiscovery(rgb.getChannelNumber());
  rgbPayload["name"] = "RGB Lighting";
  rgbPayload["uniq_id"] = std::string("supla_") + kExpectedObjectPrefix + "_" +
                          std::to_string(rgb.getChannelNumber()) + "_0";
  rgbPayload["qos"] = 0;
  rgbPayload["ret"] = false;
  rgbPayload["opt"] = false;
  rgbPayload["stat_t"] = "~/state/on";
  rgbPayload["cmd_t"] = "~/execute_action";
  rgbPayload["pl_on"] = "TURN_ON";
  rgbPayload["pl_off"] = "TURN_OFF";
  rgbPayload["stat_val_tpl"] =
      "{% if value == \"true\" %}TURN_ON{% else %}TURN_OFF{% endif %}";
  rgbPayload["on_cmd_type"] = "last";
  rgbPayload["bri_cmd_t"] = "~/set/color_brightness";
  rgbPayload["bri_scl"] = 100;
  rgbPayload["bri_stat_t"] = "~/state/color_brightness";
  rgbPayload["rgb_stat_t"] = "~/state/color";
  rgbPayload["rgb_cmd_t"] = "~/set/color";
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "light", rgb.getChannelNumber(), 0)),
                          JsonEq(jsonToString(rgbPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(rgb.getChannelNumber());

  auto dimmerAndRgbDimmerPayload =
      baseDiscovery(dimmerAndRgb.getChannelNumber());
  dimmerAndRgbDimmerPayload["name"] = "Dimmer";
  dimmerAndRgbDimmerPayload["uniq_id"] =
      std::string("supla_") + kExpectedObjectPrefix + "_" +
      std::to_string(dimmerAndRgb.getChannelNumber()) + "_0";
  dimmerAndRgbDimmerPayload["qos"] = 0;
  dimmerAndRgbDimmerPayload["ret"] = false;
  dimmerAndRgbDimmerPayload["opt"] = false;
  dimmerAndRgbDimmerPayload["stat_t"] = "~/state/dimmer/on";
  dimmerAndRgbDimmerPayload["cmd_t"] = "~/execute_action/dimmer";
  dimmerAndRgbDimmerPayload["pl_on"] = "TURN_ON";
  dimmerAndRgbDimmerPayload["pl_off"] = "TURN_OFF";
  dimmerAndRgbDimmerPayload["stat_val_tpl"] =
      "{% if value == \"true\" %}TURN_ON{% else %}TURN_OFF{% endif %}";
  dimmerAndRgbDimmerPayload["on_cmd_type"] = "last";
  dimmerAndRgbDimmerPayload["bri_cmd_t"] = "~/set/brightness";
  dimmerAndRgbDimmerPayload["bri_scl"] = 100;
  dimmerAndRgbDimmerPayload["bri_stat_t"] = "~/state/brightness";
  auto dimmerAndRgbRgbPayload = baseDiscovery(dimmerAndRgb.getChannelNumber());
  dimmerAndRgbRgbPayload["name"] = "RGB Lighting";
  dimmerAndRgbRgbPayload["uniq_id"] =
      std::string("supla_") + kExpectedObjectPrefix + "_" +
      std::to_string(dimmerAndRgb.getChannelNumber()) + "_1";
  dimmerAndRgbRgbPayload["qos"] = 0;
  dimmerAndRgbRgbPayload["ret"] = false;
  dimmerAndRgbRgbPayload["opt"] = false;
  dimmerAndRgbRgbPayload["stat_t"] = "~/state/rgb/on";
  dimmerAndRgbRgbPayload["cmd_t"] = "~/execute_action/rgb";
  dimmerAndRgbRgbPayload["pl_on"] = "TURN_ON";
  dimmerAndRgbRgbPayload["pl_off"] = "TURN_OFF";
  dimmerAndRgbRgbPayload["stat_val_tpl"] =
      "{% if value == \"true\" %}TURN_ON{% else %}TURN_OFF{% endif %}";
  dimmerAndRgbRgbPayload["on_cmd_type"] = "last";
  dimmerAndRgbRgbPayload["bri_cmd_t"] = "~/set/color_brightness";
  dimmerAndRgbRgbPayload["bri_scl"] = 100;
  dimmerAndRgbRgbPayload["bri_stat_t"] = "~/state/color_brightness";
  dimmerAndRgbRgbPayload["rgb_stat_t"] = "~/state/color";
  dimmerAndRgbRgbPayload["rgb_cmd_t"] = "~/set/color";
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "light", dimmerAndRgb.getChannelNumber(), 0)),
                          JsonEq(jsonToString(dimmerAndRgbDimmerPayload)),
                          0,
                          true));
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "light", dimmerAndRgb.getChannelNumber(), 1)),
                          JsonEq(jsonToString(dimmerAndRgbRgbPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(dimmerAndRgb.getChannelNumber());

  auto binarySensorPayload = baseDiscovery(binarySensor.getChannelNumber());
  binarySensorPayload["name"] =
      std::string("#") + std::to_string(binarySensor.getChannelNumber()) +
      " Door sensor";
  binarySensorPayload["uniq_id"] =
      std::string("supla_") + kExpectedObjectPrefix + "_" +
      std::to_string(binarySensor.getChannelNumber()) + "_0";
  binarySensorPayload["qos"] = 0;
  binarySensorPayload["ret"] = false;
  binarySensorPayload["opt"] = false;
  binarySensorPayload["stat_t"] = "~/state";
  binarySensorPayload["dev_cla"] = "door";
  binarySensorPayload["payload_on"] = "open";
  binarySensorPayload["payload_off"] = "closed";
  EXPECT_CALL(
      mqtt,
      publishTest(StrEq(expectedDiscoveryTopic(
                      "binary_sensor", binarySensor.getChannelNumber(), 0)),
                  JsonEq(jsonToString(binarySensorPayload)),
                  0,
                  true));
  mqtt.publishHADiscovery(binarySensor.getChannelNumber());

  nlohmann::json actionTriggerPayload = {
      {"dev",
       {{"ids", "my-device-0405ab"},
        {"mf", "Unknown"},
        {"name", "My Device"},
        {"sw", ""}}},
      {"automation_type", "trigger"},
      {"topic",
       std::string(kExpectedPrefix) + "/channels/" +
           std::to_string(actionTrigger.getChannelNumber()) +
           "/button_long_press"},
      {"type", "button_long_press"},
      {"subtype", "button_1"},
      {"payload", "button_long_press"},
      {"qos", 0},
      {"ret", false},
  };
  EXPECT_CALL(
      mqtt,
      publishTest(
          StrEq(expectedDiscoveryTopic(
              "device_automation", actionTrigger.getChannelNumber(), 0)),
          JsonEq(jsonToString(actionTriggerPayload)),
          0,
          true));
  for (int actionIdx = 1; actionIdx < 8; actionIdx++) {
    EXPECT_CALL(mqtt,
                publishTest(StrEq(expectedDiscoveryTopic(
                                "device_automation",
                                actionTrigger.getChannelNumber(),
                                actionIdx)),
                            StrEq(""),
                            0,
                            true));
  }
  mqtt.publishHADiscovery(actionTrigger.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, publishHADiscoveryCoversRelayImpulseVariants) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  struct Case {
    int function;
    const char *device_class;
  };

  const std::vector<Case> cases = {
      {SUPLA_CHANNELFNC_CONTROLLINGTHEGATE, "gate"},
      {SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR, "garage_door"},
      {SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK, "door"},
      {SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK, "door"},
  };

  ChannelElementMock relay;
  relay.getChannel()->setType(SUPLA_CHANNELTYPE_RELAY);
  mqtt.test_setChannelsCount(255);

  for (const auto &item : cases) {
    relay.getChannel()->setDefaultFunction(item.function);

    auto payload = baseDiscoveryPayload(relay.getChannelNumber());
    payload["name"] = std::string("#") +
                      std::to_string(relay.getChannelNumber()) + " " +
                      Supla::getRelayChannelName(item.function);
    payload["uniq_id"] = std::string("supla_") + kExpectedObjectPrefix + "_" +
                         std::to_string(relay.getChannelNumber()) + "_0";
    payload["qos"] = 0;
    payload["ret"] = false;
    payload["opt"] = false;
    payload["stat_t"] = "~/state/on";
    payload["cmd_t"] = "~/set/on";
    payload["payload_open"] = "true";
    if (item.device_class != nullptr) {
      payload["dev_cla"] = item.device_class;
    }
    std::string payloadText = jsonToString(payload);
    payloadText.pop_back();
    payloadText += ",\"payload_close\":null,\"payload_stop\":null}";

    EXPECT_CALL(mqtt,
                publishTest(StrEq(expectedDiscoveryTopic(
                                "cover", relay.getChannelNumber(), 0)),
                            JsonEq(payloadText),
                            0,
                            true));
    mqtt.publishHADiscovery(relay.getChannelNumber());
  }
}

TEST_F(MqttChannelDispatchTests, publishHADiscoveryCoversRelaySwitchVariants) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  struct Case {
    int function;
    const char *topic_type;
  };

  const std::vector<Case> cases = {
      {SUPLA_CHANNELFNC_POWERSWITCH, "switch"},
      {SUPLA_CHANNELFNC_LIGHTSWITCH, "light"},
      {SUPLA_CHANNELFNC_PUMPSWITCH, "switch"},
      {SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH, "switch"},
  };

  ChannelElementMock relay;
  relay.getChannel()->setType(SUPLA_CHANNELTYPE_RELAY);
  mqtt.test_setChannelsCount(255);

  for (const auto &item : cases) {
    relay.getChannel()->setDefaultFunction(item.function);

    auto payload = baseDiscoveryPayload(relay.getChannelNumber());
    payload["name"] = std::string("#") +
                      std::to_string(relay.getChannelNumber()) + " " +
                      Supla::getRelayChannelName(item.function);
    payload["uniq_id"] = std::string("supla_") + kExpectedObjectPrefix + "_" +
                         std::to_string(relay.getChannelNumber()) + "_0";
    payload["qos"] = 0;
    payload["ret"] = false;
    payload["opt"] = false;
    payload["stat_t"] = "~/state/on";
    payload["cmd_t"] = "~/set/on";
    payload["pl_on"] = "true";
    payload["pl_off"] = "false";

    EXPECT_CALL(mqtt,
                publishTest(StrEq(expectedDiscoveryTopic(
                                item.topic_type, relay.getChannelNumber(), 0)),
                            JsonEq(jsonToString(payload)),
                            0,
                            true));
    mqtt.publishHADiscovery(relay.getChannelNumber());
  }
}

TEST_F(MqttChannelDispatchTests,
       publishHADiscoveryUsesRelayRollerShutterPairSecondaryChannel) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  Supla::Control::RelayRollerShutterPair pair(1, 2);
  pair.getSecondaryChannel()->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  mqtt.test_setChannelsCount(255);

  const int channelNumber = pair.getSecondaryChannelNumber();
  auto payload = baseDiscoveryPayload(channelNumber);
  payload["name"] = std::string("#") + std::to_string(channelNumber) + " " +
                    Supla::getRelayChannelName(SUPLA_CHANNELFNC_POWERSWITCH);
  payload["uniq_id"] = std::string("supla_") + kExpectedObjectPrefix + "_" +
                       std::to_string(channelNumber) + "_0";
  payload["qos"] = 0;
  payload["ret"] = false;
  payload["opt"] = false;
  payload["stat_t"] = "~/state/on";
  payload["cmd_t"] = "~/set/on";
  payload["pl_on"] = "true";
  payload["pl_off"] = "false";

  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "switch", channelNumber, 0)),
                          JsonEq(jsonToString(payload)),
                          0,
                          true));
  mqtt.publishHADiscovery(channelNumber);
}

TEST_F(MqttChannelDispatchTests,
       publishHADiscoveryCoversRollerShutterVariants) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  struct Case {
    int function;
    uint32_t func_bit;
    const char *device_class;
    bool tilt;
  };

  const std::vector<Case> cases = {
      {SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER,
       SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER,
       "shutter",
       false},
      {SUPLA_CHANNELFNC_VERTICAL_BLIND,
       SUPLA_BIT_FUNC_VERTICAL_BLIND,
       "shutter",
       true},
      {SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND,
       SUPLA_BIT_FUNC_CONTROLLINGTHEFACADEBLIND,
       "shutter",
       true},
      {SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW,
       SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW,
       "window",
       false},
      {SUPLA_CHANNELFNC_TERRACE_AWNING,
       SUPLA_BIT_FUNC_TERRACE_AWNING,
       "awning",
       false},
      {SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR,
       SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR,
       "garage_door",
       false},
      {SUPLA_CHANNELFNC_CURTAIN, SUPLA_BIT_FUNC_CURTAIN, "curtain", false},
      {SUPLA_CHANNELFNC_PROJECTOR_SCREEN,
       SUPLA_BIT_FUNC_PROJECTOR_SCREEN,
       "shade",
       false},
  };

  ChannelElementMock roller;
  roller.getChannel()->setType(SUPLA_CHANNELTYPE_RELAY);
  mqtt.test_setChannelsCount(255);

  for (const auto &item : cases) {
    roller.getChannel()->setDefaultFunction(item.function);
    roller.getChannel()->setFuncList(item.func_bit);

    auto payload = baseDiscoveryPayload(roller.getChannelNumber());
    payload["name"] = std::string("#") +
                      std::to_string(roller.getChannelNumber()) + " " +
                      Supla::getRelayChannelName(item.function);
    payload["uniq_id"] = std::string("supla_") + kExpectedObjectPrefix + "_" +
                         std::to_string(roller.getChannelNumber()) + "_0";
    payload["qos"] = 0;
    payload["ret"] = false;
    payload["opt"] = false;
    payload["cmd_t"] = "~/execute_action";
    payload["pl_open"] = "REVEAL";
    payload["pl_cls"] = "SHUT";
    payload["pl_stop"] = "STOP";
    payload["set_pos_t"] = "~/set/closing_percentage";
    payload["pos_t"] = "~/state/shut";
    payload["pos_open"] = 0;
    payload["pos_clsd"] = 100;
    payload["pos_tpl"] =
        "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int "
        "> 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}";
    payload["dev_cla"] = item.device_class;
    if (item.tilt) {
      payload["tilt_cmd_t"] = "~/set/tilt";
      payload["tilt_status_t"] = "~/state/tilt";
      payload["tilt_min"] = 100;
      payload["tilt_max"] = 0;
      payload["tilt_opened_value"] = 0;
      payload["tilt_closed_value"] = 100;
      payload["tilt_status_tpl"] =
          "{% if int(value, default=0) <= 0 %}0{% elif value | int > 100 %}"
          "100{% else %}{{value | int}}{% endif %}";
    }

    EXPECT_CALL(mqtt,
                publishTest(StrEq(expectedDiscoveryTopic(
                                "cover", roller.getChannelNumber(), 0)),
                            JsonEq(jsonToString(payload)),
                            0,
                            true));
    mqtt.publishHADiscovery(roller.getChannelNumber());
  }
}

TEST_F(MqttChannelDispatchTests, publishHADiscoveryCoversBinarySensorVariants) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  struct Case {
    int function;
    const char *name_suffix;
    const char *device_class;
    bool open_closed;
  };

  const std::vector<Case> cases = {
      {SUPLA_CHANNELFNC_OPENINGSENSOR_GATEWAY, "Gateway sensor", "door", true},
      {SUPLA_CHANNELFNC_OPENINGSENSOR_GATE, "Gate sensor", "garage_door", true},
      {SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR, "Door sensor", "door", true},
      {SUPLA_CHANNELFNC_OPENINGSENSOR_GARAGEDOOR,
       "Garage door sensor",
       "garage_door",
       true},
      {SUPLA_CHANNELFNC_NOLIQUIDSENSOR, "Liquid sensor", "moisture", false},
      {SUPLA_CHANNELFNC_FLOOD_SENSOR, "Flood sensor", "moisture", false},
      {SUPLA_CHANNELFNC_CONTAINER_LEVEL_SENSOR,
       "Container level sensor",
       nullptr,
       false},
      {SUPLA_CHANNELFNC_OPENINGSENSOR_ROLLERSHUTTER,
       "Roller shutter sensor",
       "window",
       true},
      {SUPLA_CHANNELFNC_OPENINGSENSOR_ROOFWINDOW,
       "Roof window sensor",
       "window",
       true},
      {SUPLA_CHANNELFNC_OPENINGSENSOR_WINDOW, "Window sensor", "window", true},
      {SUPLA_CHANNELFNC_HOTELCARDSENSOR, "Hotel card sensor", nullptr, false},
      {SUPLA_CHANNELFNC_ALARMARMAMENTSENSOR,
       "Alarm armament sensor",
       nullptr,
       false},
      {SUPLA_CHANNELFNC_MAILSENSOR, "Mail sensor", nullptr, false},
      {SUPLA_CHANNELFNC_MOTION_SENSOR, "Motion sensor", nullptr, false},
      {SUPLA_CHANNELFNC_BINARY_SENSOR, "Binary sensor", nullptr, false},
  };

  ChannelElementMock sensor;
  sensor.getChannel()->setType(SUPLA_CHANNELTYPE_BINARYSENSOR);
  mqtt.test_setChannelsCount(255);

  for (const auto &item : cases) {
    sensor.getChannel()->setDefaultFunction(item.function);

    auto payload = baseDiscoveryPayload(sensor.getChannelNumber());
    payload["name"] = std::string("#") +
                      std::to_string(sensor.getChannelNumber()) + " " +
                      item.name_suffix;
    payload["uniq_id"] = std::string("supla_") + kExpectedObjectPrefix + "_" +
                         std::to_string(sensor.getChannelNumber()) + "_0";
    payload["qos"] = 0;
    payload["ret"] = false;
    payload["opt"] = false;
    payload["stat_t"] = "~/state";
    if (item.device_class != nullptr) {
      payload["dev_cla"] = item.device_class;
    }
    if (item.open_closed) {
      payload["payload_on"] = "open";
      payload["payload_off"] = "closed";
    }

    EXPECT_CALL(mqtt,
                publishTest(StrEq(expectedDiscoveryTopic(
                                "binary_sensor", sensor.getChannelNumber(), 0)),
                            JsonEq(jsonToString(payload)),
                            0,
                            true));
    mqtt.publishHADiscovery(sensor.getChannelNumber());
  }
}

TEST_F(MqttChannelDispatchTests, publishHADiscoveryCoversActionTriggerAllCaps) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  ChannelElementMock actionTrigger;
  actionTrigger.getChannel()->setType(SUPLA_CHANNELTYPE_ACTIONTRIGGER);
  actionTrigger.getChannel()->setActionTriggerCaps(
      SUPLA_ACTION_CAP_HOLD | SUPLA_ACTION_CAP_TOGGLE_x1 |
      SUPLA_ACTION_CAP_TOGGLE_x2 | SUPLA_ACTION_CAP_TOGGLE_x3 |
      SUPLA_ACTION_CAP_TOGGLE_x4 | SUPLA_ACTION_CAP_TOGGLE_x5 |
      SUPLA_ACTION_CAP_TURN_ON | SUPLA_ACTION_CAP_TURN_OFF);

  mqtt.test_setChannelsCount(255);

  struct Case {
    int action_idx;
    const char *type;
  };

  const std::vector<Case> cases = {
      {0, "button_long_press"},
      {1, "button_short_press"},
      {2, "button_double_press"},
      {3, "button_triple_press"},
      {4, "button_quadruple_press"},
      {5, "button_quintuple_press"},
      {6, "button_turn_on"},
      {7, "button_turn_off"},
  };

  for (const auto &item : cases) {
    auto payload = nlohmann::json{
        {"dev",
         {{"ids", "my-device-0405ab"},
          {"mf", "Unknown"},
          {"name", "My Device"},
          {"sw", ""}}},
        {"automation_type", "trigger"},
        {"topic",
         std::string(kExpectedPrefix) + "/channels/" +
             std::to_string(actionTrigger.getChannelNumber()) + "/" +
             item.type},
        {"type", item.type},
        {"subtype", "button_1"},
        {"payload", item.type},
        {"qos", 0},
        {"ret", false},
    };

    EXPECT_CALL(mqtt,
                publishTest(StrEq(expectedDiscoveryTopic(
                                "device_automation",
                                actionTrigger.getChannelNumber(),
                                item.action_idx)),
                            JsonEq(jsonToString(payload)),
                            0,
                            true));
  }
  mqtt.publishHADiscovery(actionTrigger.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, publishHADiscoveryCoversHvac) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  OutputSimulatorWithCheck output;
  Supla::Control::HvacBase hvac(&output);
  hvac.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.setSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
  hvac.setMainThermometerChannelNo(hvac.getChannelNumber());

  mqtt.test_setChannelsCount(255);

  const std::string currentTemperatureTopic =
      std::string(kExpectedPrefix) + "/channels/" +
      std::to_string(hvac.getMainThermometerChannelNo()) + "/state/temperature";
  nlohmann::json hvacPayload = {
      {"avty_t", std::string(kExpectedPrefix) + "/state/connected"},
      {"pl_avail", "true"},
      {"pl_not_avail", "false"},
      {"~",
       std::string(kExpectedPrefix) + "/channels/" +
           std::to_string(hvac.getChannelNumber())},
      {"dev",
       {{"ids", "my-device-0405ab"},
        {"mf", "Unknown"},
        {"name", "My Device"},
        {"sw", ""}}},
      {"name",
       std::string("#") + std::to_string(hvac.getChannelNumber()) +
           " Thermostat"},
      {"uniq_id",
       std::string("supla_") + kExpectedObjectPrefix + "_" +
           std::to_string(hvac.getChannelNumber()) + "_0"},
      {"qos", 0},
      {"ret", false},
      {"opt", false},
      {"action_topic", "~/state/action"},
      {"current_temperature_topic", currentTemperatureTopic},
      {"current_humidity_topic", "None"},
      {"max_temp", "-327.68"},
      {"min_temp", "-327.68"},
      {"modes", nlohmann::json::array({"off", "auto", "heat"})},
      {"mode_stat_t", "~/state/mode"},
      {"mode_command_topic", "~/execute_action"},
      {"power_command_topic", "~/execute_action"},
      {"payload_off", "turn_off"},
      {"payload_on", "turn_on"},
      {"temperature_unit", "C"},
      {"temp_step", "0.1"},
      {"temperature_command_topic", "~/set/temperature_setpoint"},
      {"temperature_state_topic", "~/state/temperature_setpoint"},
  };
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "climate", hvac.getChannelNumber(), 0)),
                          JsonEq(jsonToString(hvacPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(hvac.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, publishHADiscoveryCoversHvacCoolSubfunction) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  OutputSimulatorWithCheck output;
  Supla::Control::HvacBase hvac(&output);
  hvac.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.setSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);
  hvac.setMainThermometerChannelNo(hvac.getChannelNumber());

  mqtt.test_setChannelsCount(255);

  const std::string currentTemperatureTopic =
      std::string(kExpectedPrefix) + "/channels/" +
      std::to_string(hvac.getMainThermometerChannelNo()) + "/state/temperature";
  nlohmann::json hvacPayload = {
      {"avty_t", std::string(kExpectedPrefix) + "/state/connected"},
      {"pl_avail", "true"},
      {"pl_not_avail", "false"},
      {"~",
       std::string(kExpectedPrefix) + "/channels/" +
           std::to_string(hvac.getChannelNumber())},
      {"dev",
       {{"ids", "my-device-0405ab"},
        {"mf", "Unknown"},
        {"name", "My Device"},
        {"sw", ""}}},
      {"name",
       std::string("#") + std::to_string(hvac.getChannelNumber()) +
           " Thermostat"},
      {"uniq_id",
       std::string("supla_") + kExpectedObjectPrefix + "_" +
           std::to_string(hvac.getChannelNumber()) + "_0"},
      {"qos", 0},
      {"ret", false},
      {"opt", false},
      {"action_topic", "~/state/action"},
      {"current_temperature_topic", currentTemperatureTopic},
      {"current_humidity_topic", "None"},
      {"max_temp", "-327.68"},
      {"min_temp", "-327.68"},
      {"modes", nlohmann::json::array({"off", "auto", "cool"})},
      {"mode_stat_t", "~/state/mode"},
      {"mode_command_topic", "~/execute_action"},
      {"power_command_topic", "~/execute_action"},
      {"payload_off", "turn_off"},
      {"payload_on", "turn_on"},
      {"temperature_unit", "C"},
      {"temp_step", "0.1"},
      {"temperature_command_topic", "~/set/temperature_setpoint"},
      {"temperature_state_topic", "~/state/temperature_setpoint"},
  };
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "climate", hvac.getChannelNumber(), 0)),
                          JsonEq(jsonToString(hvacPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(hvac.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, publishHADiscoveryCoversHvacHeatCoolVariant) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  OutputSimulatorWithCheck output;
  Supla::Control::HvacBase hvac(&output);
  hvac.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
  hvac.setMainThermometerChannelNo(hvac.getChannelNumber());

  mqtt.test_setChannelsCount(255);

  const std::string currentTemperatureTopic =
      std::string(kExpectedPrefix) + "/channels/" +
      std::to_string(hvac.getMainThermometerChannelNo()) + "/state/temperature";
  nlohmann::json hvacPayload = {
      {"avty_t", std::string(kExpectedPrefix) + "/state/connected"},
      {"pl_avail", "true"},
      {"pl_not_avail", "false"},
      {"~",
       std::string(kExpectedPrefix) + "/channels/" +
           std::to_string(hvac.getChannelNumber())},
      {"dev",
       {{"ids", "my-device-0405ab"},
        {"mf", "Unknown"},
        {"name", "My Device"},
        {"sw", ""}}},
      {"name",
       std::string("#") + std::to_string(hvac.getChannelNumber()) +
           " Thermostat"},
      {"uniq_id",
       std::string("supla_") + kExpectedObjectPrefix + "_" +
           std::to_string(hvac.getChannelNumber()) + "_0"},
      {"qos", 0},
      {"ret", false},
      {"opt", false},
      {"action_topic", "~/state/action"},
      {"current_temperature_topic", currentTemperatureTopic},
      {"current_humidity_topic", "None"},
      {"max_temp", "-327.68"},
      {"min_temp", "-327.68"},
      {"modes",
       nlohmann::json::array({"off", "auto", "heat", "cool", "heat_cool"})},
      {"mode_stat_t", "~/state/mode"},
      {"mode_command_topic", "~/execute_action"},
      {"power_command_topic", "~/execute_action"},
      {"payload_off", "turn_off"},
      {"payload_on", "turn_on"},
      {"temperature_unit", "C"},
      {"temp_step", "0.1"},
      {"temperature_high_command_topic", "~/set/temperature_setpoint_cool/"},
      {"temperature_high_state_topic", "~/state/temperature_setpoint_cool/"},
      {"temperature_low_command_topic", "~/set/temperature_setpoint_heat/"},
      {"temperature_low_state_topic", "~/state/temperature_setpoint_heat/"},
  };
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "climate", hvac.getChannelNumber(), 0)),
                          JsonEq(jsonToString(hvacPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(hvac.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests,
       publishHADiscoveryCoversHvacDifferentialVariant) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  OutputSimulatorWithCheck output;
  Supla::Control::HvacBase hvac(&output);
  hvac.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);
  hvac.setMainThermometerChannelNo(hvac.getChannelNumber());

  mqtt.test_setChannelsCount(255);

  const std::string currentTemperatureTopic =
      std::string(kExpectedPrefix) + "/channels/" +
      std::to_string(hvac.getMainThermometerChannelNo()) + "/state/temperature";
  nlohmann::json hvacPayload = {
      {"avty_t", std::string(kExpectedPrefix) + "/state/connected"},
      {"pl_avail", "true"},
      {"pl_not_avail", "false"},
      {"~",
       std::string(kExpectedPrefix) + "/channels/" +
           std::to_string(hvac.getChannelNumber())},
      {"dev",
       {{"ids", "my-device-0405ab"},
        {"mf", "Unknown"},
        {"name", "My Device"},
        {"sw", ""}}},
      {"name",
       std::string("#") + std::to_string(hvac.getChannelNumber()) +
           " Thermostat"},
      {"uniq_id",
       std::string("supla_") + kExpectedObjectPrefix + "_" +
           std::to_string(hvac.getChannelNumber()) + "_0"},
      {"qos", 0},
      {"ret", false},
      {"opt", false},
      {"action_topic", "~/state/action"},
      {"current_temperature_topic", currentTemperatureTopic},
      {"current_humidity_topic", "None"},
      {"max_temp", "-327.68"},
      {"min_temp", "-327.68"},
      {"modes", nlohmann::json::array({"off", "auto", "heat"})},
      {"mode_stat_t", "~/state/mode"},
      {"mode_command_topic", "~/execute_action"},
      {"power_command_topic", "~/execute_action"},
      {"payload_off", "turn_off"},
      {"payload_on", "turn_on"},
      {"temperature_unit", "C"},
      {"temp_step", "0.1"},
      {"temperature_command_topic", "~/set/temperature_setpoint"},
      {"temperature_state_topic", "~/state/temperature_setpoint"},
  };
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "climate", hvac.getChannelNumber(), 0)),
                          JsonEq(jsonToString(hvacPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(hvac.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests,
       publishHADiscoveryCoversHvacDomesticHotWaterVariant) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  OutputSimulatorWithCheck output;
  Supla::Control::HvacBase hvac(&output);
  hvac.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER);
  hvac.setMainThermometerChannelNo(hvac.getChannelNumber());

  mqtt.test_setChannelsCount(255);

  const std::string currentTemperatureTopic =
      std::string(kExpectedPrefix) + "/channels/" +
      std::to_string(hvac.getMainThermometerChannelNo()) + "/state/temperature";
  nlohmann::json hvacPayload = {
      {"avty_t", std::string(kExpectedPrefix) + "/state/connected"},
      {"pl_avail", "true"},
      {"pl_not_avail", "false"},
      {"~",
       std::string(kExpectedPrefix) + "/channels/" +
           std::to_string(hvac.getChannelNumber())},
      {"dev",
       {{"ids", "my-device-0405ab"},
        {"mf", "Unknown"},
        {"name", "My Device"},
        {"sw", ""}}},
      {"name",
       std::string("#") + std::to_string(hvac.getChannelNumber()) +
           " Thermostat"},
      {"uniq_id",
       std::string("supla_") + kExpectedObjectPrefix + "_" +
           std::to_string(hvac.getChannelNumber()) + "_0"},
      {"qos", 0},
      {"ret", false},
      {"opt", false},
      {"action_topic", "~/state/action"},
      {"current_temperature_topic", currentTemperatureTopic},
      {"current_humidity_topic", "None"},
      {"max_temp", "-327.68"},
      {"min_temp", "-327.68"},
      {"modes", nlohmann::json::array({"off", "auto", "heat"})},
      {"mode_stat_t", "~/state/mode"},
      {"mode_command_topic", "~/execute_action"},
      {"power_command_topic", "~/execute_action"},
      {"payload_off", "turn_off"},
      {"payload_on", "turn_on"},
      {"temperature_unit", "C"},
      {"temp_step", "0.1"},
      {"temperature_command_topic", "~/set/temperature_setpoint"},
      {"temperature_state_topic", "~/state/temperature_setpoint"},
  };
  EXPECT_CALL(mqtt,
              publishTest(StrEq(expectedDiscoveryTopic(
                              "climate", hvac.getChannelNumber(), 0)),
                          JsonEq(jsonToString(hvacPayload)),
                          0,
                          true));
  mqtt.publishHADiscovery(hvac.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, publishHADiscoveryCoversElectricityMeter) {
  SuplaDeviceClass sd;
  NiceMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  Supla::Sensor::ElectricityMeter electricityMeter;
  SimpleTime time;

  electricityMeter.onInit();
  electricityMeter.setFwdActEnergy(0, 1000);
  electricityMeter.setRvrActEnergy(0, 2000);
  electricityMeter.setFwdBalancedEnergy(3000);
  electricityMeter.setRvrBalancedEnergy(4000);
  electricityMeter.setFreq(5000);
  electricityMeter.setVoltage(0, 2300);
  electricityMeter.setCurrent(0, 1234);
  electricityMeter.setPowerActive(0, 123456);
  electricityMeter.setPowerReactive(0, 234567);
  electricityMeter.setPowerApparent(0, 345678);
  electricityMeter.setPowerFactor(0, 876);
  electricityMeter.setPhaseAngle(0, 123);

  electricityMeter.setFwdActEnergy(1, 1001);
  electricityMeter.setRvrActEnergy(1, 2001);
  electricityMeter.setFwdReactEnergy(1, 3001);
  electricityMeter.setRvrReactEnergy(1, 4001);
  electricityMeter.setVoltage(1, 2301);
  electricityMeter.setCurrent(1, 1235);
  electricityMeter.setPowerActive(1, 123457);
  electricityMeter.setPowerReactive(1, 234568);
  electricityMeter.setPowerApparent(1, 345679);
  electricityMeter.setPowerFactor(1, 877);
  electricityMeter.setPhaseAngle(1, 124);

  electricityMeter.setFwdActEnergy(2, 1002);
  electricityMeter.setRvrActEnergy(2, 2002);
  electricityMeter.setFwdReactEnergy(2, 3002);
  electricityMeter.setRvrReactEnergy(2, 4002);
  electricityMeter.setVoltage(2, 2302);
  electricityMeter.setCurrent(2, 1236);
  electricityMeter.setPowerActive(2, 123458);
  electricityMeter.setPowerReactive(2, 234569);
  electricityMeter.setPowerApparent(2, 345680);
  electricityMeter.setPowerFactor(2, 878);
  electricityMeter.setPhaseAngle(2, 125);
  time.advance(10000);
  electricityMeter.iterateAlways();

  mqtt.test_setChannelsCount(255);

  auto expectedEmPayload = [&](int parameterId,
                               const std::string &displayName,
                               const std::string &stateTopic,
                               const std::string &unitOfMeasure,
                               const std::string &statClass,
                               const std::string &deviceClass) {
    nlohmann::json payload{
        {"avty_t", std::string(kExpectedPrefix) + "/state/connected"},
        {"pl_avail", "true"},
        {"pl_not_avail", "false"},
        {"~",
         std::string(kExpectedPrefix) + "/channels/" +
             std::to_string(electricityMeter.getChannelNumber())},
        {"dev",
         {{"ids", "my-device-0405ab"},
          {"mf", "Unknown"},
          {"name", "My Device"},
          {"sw", ""}}},
        {"name",
         std::string("#") +
             std::to_string(electricityMeter.getChannelNumber()) +
             " Electricity Meter (" + displayName + ")"},
        {"uniq_id",
         std::string("supla_") + kExpectedObjectPrefix + "_" +
             std::to_string(electricityMeter.getChannelNumber()) + "_" +
             std::to_string(parameterId)},
        {"qos", 0},
        {"unit_of_meas", unitOfMeasure},
        {"stat_t", "~/state/" + stateTopic},
        {"stat_cla", statClass},
    };
    if (!deviceClass.empty()) {
      payload["dev_cla"] = deviceClass;
    }
    return payload;
  };

  struct EmDiscoveryExpectation {
    int parameter_id;
    const char *display_name;
    const char *state_topic;
    const char *unit_of_measure;
    const char *stat_class;
    const char *device_class;
  };

  const std::vector<EmDiscoveryExpectation> expectations = {
      {1,
       "Total forward active energy",
       "total_forward_active_energy",
       "kWh",
       "total_increasing",
       "energy"},
      {2,
       "Total reverse active energy",
       "total_reverse_active_energy",
       "kWh",
       "total_increasing",
       "energy"},
      {3,
       "Total forward active energy balanced",
       "total_forward_active_energy_balanced",
       "kWh",
       "total_increasing",
       "energy"},
      {4,
       "Total reverse active energy balanced",
       "total_reverse_active_energy_balanced",
       "kWh",
       "total_increasing",
       "energy"},
      {5,
       "Total forward active energy - Phase 1",
       "phases/1/total_forward_active_energy",
       "kWh",
       "total_increasing",
       "energy"},
      {6,
       "Total reverse active energy - Phase 1",
       "phases/1/total_reverse_active_energy",
       "kWh",
       "total_increasing",
       "energy"},
      {7,
       "Total forward reactive energy - Phase 1",
       "phases/1/total_forward_reactive_energy",
       "kvarh",
       "total_increasing",
       "reactive_energy"},
      {8,
       "Total reverse reactive energy - Phase 1",
       "phases/1/total_reverse_reactive_energy",
       "kvarh",
       "total_increasing",
       "reactive_energy"},
      {9,
       "Frequency - Phase 1",
       "phases/1/frequency",
       "Hz",
       "measurement",
       "frequency"},
      {10,
       "Voltage - Phase 1",
       "phases/1/voltage",
       "V",
       "measurement",
       "voltage"},
      {11,
       "Current - Phase 1",
       "phases/1/current",
       "A",
       "measurement",
       "current"},
      {12,
       "Power active - Phase 1",
       "phases/1/power_active",
       "W",
       "measurement",
       "power"},
      {13,
       "Power reactive - Phase 1",
       "phases/1/power_reactive",
       "var",
       "measurement",
       "reactive_power"},
      {14,
       "Power apparent - Phase 1",
       "phases/1/power_apparent",
       "VA",
       "measurement",
       "apparent_power"},
      {15,
       "Power factor - Phase 1",
       "phases/1/power_factor",
       "",
       "measurement",
       "power_factor"},
      {16,
       "Phase angle - Phase 1",
       "phases/1/phase_angle",
       "°",
       "measurement",
       ""},
      {17,
       "Total forward active energy - Phase 2",
       "phases/2/total_forward_active_energy",
       "kWh",
       "total_increasing",
       "energy"},
      {18,
       "Total reverse active energy - Phase 2",
       "phases/2/total_reverse_active_energy",
       "kWh",
       "total_increasing",
       "energy"},
      {19,
       "Total forward reactive energy - Phase 2",
       "phases/2/total_forward_reactive_energy",
       "kvarh",
       "total_increasing",
       "reactive_energy"},
      {20,
       "Total reverse reactive energy - Phase 2",
       "phases/2/total_reverse_reactive_energy",
       "kvarh",
       "total_increasing",
       "reactive_energy"},
      {21,
       "Frequency - Phase 2",
       "phases/2/frequency",
       "Hz",
       "measurement",
       "frequency"},
      {22,
       "Voltage - Phase 2",
       "phases/2/voltage",
       "V",
       "measurement",
       "voltage"},
      {23,
       "Current - Phase 2",
       "phases/2/current",
       "A",
       "measurement",
       "current"},
      {24,
       "Power active - Phase 2",
       "phases/2/power_active",
       "W",
       "measurement",
       "power"},
      {25,
       "Power reactive - Phase 2",
       "phases/2/power_reactive",
       "var",
       "measurement",
       "reactive_power"},
      {26,
       "Power apparent - Phase 2",
       "phases/2/power_apparent",
       "VA",
       "measurement",
       "apparent_power"},
      {27,
       "Power factor - Phase 2",
       "phases/2/power_factor",
       "",
       "measurement",
       "power_factor"},
      {28,
       "Phase angle - Phase 2",
       "phases/2/phase_angle",
       "°",
       "measurement",
       ""},
      {29,
       "Total forward active energy - Phase 3",
       "phases/3/total_forward_active_energy",
       "kWh",
       "total_increasing",
       "energy"},
      {30,
       "Total reverse active energy - Phase 3",
       "phases/3/total_reverse_active_energy",
       "kWh",
       "total_increasing",
       "energy"},
      {31,
       "Total forward reactive energy - Phase 3",
       "phases/3/total_forward_reactive_energy",
       "kvarh",
       "total_increasing",
       "reactive_energy"},
      {32,
       "Total reverse reactive energy - Phase 3",
       "phases/3/total_reverse_reactive_energy",
       "kvarh",
       "total_increasing",
       "reactive_energy"},
      {33,
       "Frequency - Phase 3",
       "phases/3/frequency",
       "Hz",
       "measurement",
       "frequency"},
      {34,
       "Voltage - Phase 3",
       "phases/3/voltage",
       "V",
       "measurement",
       "voltage"},
      {35,
       "Current - Phase 3",
       "phases/3/current",
       "A",
       "measurement",
       "current"},
      {36,
       "Power active - Phase 3",
       "phases/3/power_active",
       "W",
       "measurement",
       "power"},
      {37,
       "Power reactive - Phase 3",
       "phases/3/power_reactive",
       "var",
       "measurement",
       "reactive_power"},
      {38,
       "Power apparent - Phase 3",
       "phases/3/power_apparent",
       "VA",
       "measurement",
       "apparent_power"},
      {39,
       "Power factor - Phase 3",
       "phases/3/power_factor",
       "",
       "measurement",
       "power_factor"},
      {40,
       "Phase angle - Phase 3",
       "phases/3/phase_angle",
       "°",
       "measurement",
       ""},
  };

  for (const auto &expectation : expectations) {
    EXPECT_CALL(
        mqtt,
        publishTest(
            StrEq(expectedDiscoveryTopic("sensor",
                                         electricityMeter.getChannelNumber(),
                                         expectation.parameter_id)),
            JsonEq(jsonToString(expectedEmPayload(expectation.parameter_id,
                                                  expectation.display_name,
                                                  expectation.state_topic,
                                                  expectation.unit_of_measure,
                                                  expectation.stat_class,
                                                  expectation.device_class))),
            0,
            true));
  }
  mqtt.publishHADiscovery(electricityMeter.getChannelNumber());
}

TEST_F(MqttChannelDispatchTests, processDataCoversControlTypes) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);

  ChannelElementMock roller;
  configureRollerRelay(roller.getChannel(),
                       SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);

  ChannelElementMock dimmer;
  dimmer.getChannel()->setType(SUPLA_CHANNELTYPE_DIMMER);

  ChannelElementMock rgb;
  rgb.getChannel()->setType(SUPLA_CHANNELTYPE_RGBLEDCONTROLLER);

  ChannelElementMock dimmerAndRgb;
  dimmerAndRgb.getChannel()->setType(SUPLA_CHANNELTYPE_DIMMERANDRGBLED);

  ChannelElementMock hvac;
  hvac.getChannel()->setType(SUPLA_CHANNELTYPE_HVAC);
  hvac.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.getChannel()->setHvacFlagCoolSubfunction(
      Supla::HvacCoolSubfunctionFlag::CoolSubfunction);

  mqtt.test_setChannelsCount(255);

  EXPECT_CALL(roller, handleNewValueFromServer(_))
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(20, value->value[0]);
        return 0;
      });
  EXPECT_TRUE(mqtt.processData((expectedChannelTopic(roller.getChannelNumber(),
                                                     "set/closing_percentage"))
                                   .c_str(),
                               "10"));

  EXPECT_CALL(roller, handleNewValueFromServer(_))
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(20, value->value[1]);
        return 0;
      });
  EXPECT_TRUE(mqtt.processData(
      (expectedChannelTopic(roller.getChannelNumber(), "set/tilt")).c_str(),
      "20"));

  EXPECT_CALL(dimmer, handleNewValueFromServer(_))
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(55, value->value[0]);
        EXPECT_EQ(RGBW_COMMAND_SET_BRIGHTNESS_WITHOUT_TURN_ON, value->value[6]);
        return 0;
      });
  EXPECT_TRUE(mqtt.processData(
      (expectedChannelTopic(dimmer.getChannelNumber(), "set/brightness"))
          .c_str(),
      "55"));

  EXPECT_CALL(rgb, handleNewValueFromServer(_))
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(3, value->value[2]);
        EXPECT_EQ(2, value->value[3]);
        EXPECT_EQ(1, value->value[4]);
        EXPECT_EQ(RGBW_COMMAND_SET_RGB_WITHOUT_TURN_ON, value->value[6]);
        return 0;
      });
  EXPECT_TRUE(mqtt.processData(
      (expectedChannelTopic(rgb.getChannelNumber(), "set/color")).c_str(),
      "1,2,3"));

  EXPECT_CALL(dimmerAndRgb, handleNewValueFromServer(_))
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        EXPECT_EQ(RGBW_COMMAND_TURN_OFF_RGB, value->value[6]);
        return 0;
      });
  EXPECT_TRUE(
      mqtt.processData((expectedChannelTopic(dimmerAndRgb.getChannelNumber(),
                                             "execute_action/rgb"))
                           .c_str(),
                       "turn_off"));

  EXPECT_CALL(hvac, handleNewValueFromServer(_))
      .WillOnce([](TSD_SuplaChannelNewValue *value) {
        auto *hvacValue = reinterpret_cast<THVACValue *>(value->value);
        EXPECT_EQ(1950, hvacValue->SetpointTemperatureCool);
        EXPECT_TRUE(hvacValue->Flags &
                    SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET);
        return 0;
      });
  EXPECT_TRUE(
      mqtt.processData((expectedChannelTopic(hvac.getChannelNumber(),
                                             "set/temperature_setpoint"))
                           .c_str(),
                       "19.5"));
}

TEST_F(MqttChannelDispatchTests,
       processDataRoutesRelayRollerShutterPairSecondaryRelayTopic) {
  SuplaDeviceClass sd;
  StrictMock<MqttTestMock> mqtt(&sd);
  initMqtt(sd, mqtt);
  DigitalInterfaceMock ioMock;
  const int gpio0 = 1;
  const int gpio1 = 2;
  Supla::Control::RelayRollerShutterPair pair(gpio0, gpio1);
  mqtt.test_setChannelsCount(255);

  ASSERT_EQ(Supla::Element::getElementByChannelNumber(
                pair.getSecondaryChannelNumber()),
            &pair);
  ASSERT_FALSE(pair.getSecondaryChannel()->isRollerShutterRelayType());
  EXPECT_CALL(ioMock, digitalWrite(gpio0, 0)).Times(2);
  EXPECT_CALL(ioMock, pinMode(gpio0, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 0)).Times(2);
  EXPECT_CALL(ioMock, pinMode(gpio1, OUTPUT));
  pair.onInit();

  EXPECT_CALL(ioMock, digitalWrite(gpio0, 1)).Times(0);
  EXPECT_CALL(ioMock, digitalWrite(gpio1, 1));

  EXPECT_TRUE(mqtt.processData(
      (expectedChannelTopic(pair.getSecondaryChannelNumber(), "set/on"))
          .c_str(),
      "true"));
  EXPECT_FALSE(pair.getChannel()->getValueBool());
  EXPECT_TRUE(pair.getSecondaryChannel()->getValueBool());
}
