// SPDX-FileCopyrightText: AC SOFTWARE SP. Z. O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/element.h>
#include <supla/parser/parser.h>
#include <supla/output/mqtt.h>
#include <supla/source/mqtt_src.h>
#include <supla/source/source.h>

#include <string>
#include <variant>
#include <vector>

#include "linux_yaml_config.h"
#include "supla/control/rgbcct_parsed.h"

namespace Supla::Linux {
void initExtensions() {
}
}  // namespace Supla::Linux

Supla::Source::Mqtt::Mqtt(const Supla::LinuxYamlConfig&,
                          const std::vector<std::string>& topics,
                          int qos)
    : topics(topics), qos(qos) {
}

Supla::Source::Mqtt::~Mqtt() {
}

std::string Supla::Source::Mqtt::getContent() {
  return latestMessage;
}

bool Supla::Source::Mqtt::isConnected() {
  return false;
}

Supla::Output::Mqtt::Mqtt(const Supla::LinuxYamlConfig&,
                          const char* topic,
                          int qos)
    : controlTopic(topic), qos(qos) {
}

Supla::Output::Mqtt::~Mqtt() {
}

bool Supla::Output::Mqtt::putContent(int) {
  return false;
}

bool Supla::Output::Mqtt::putContent(const std::string&) {
  return false;
}

bool Supla::Output::Mqtt::putContent(const std::vector<int>&) {
  return false;
}

bool Supla::Output::Mqtt::putContent(bool) {
  return false;
}

namespace {

class FakeYamlSource : public Supla::Source::Source {
 public:
  std::string getContent() override {
    return {};
  }
};

class FakeYamlParser : public Supla::Parser::Parser {
 public:
  explicit FakeYamlParser(Supla::Source::Source* source)
      : Supla::Parser::Parser(source) {
  }

  std::variant<int, bool, std::string> state = 0;

  double getValue(const std::string&) override {
    return 0;
  }

  std::variant<int, bool, std::string> getStateValue(
      const std::string&) override {
    return state;
  }

  bool isBasedOnIndex() override {
    return false;
  }

 protected:
  bool refreshSource() override {
    valid = true;
    return true;
  }
};

class TestLinuxYamlConfig : public Supla::LinuxYamlConfig {
 public:
  TestLinuxYamlConfig() : Supla::LinuxYamlConfig("") {
  }

  using Supla::LinuxYamlConfig::addRgbCctParsed;
};

Supla::Control::RgbCctParsed* getCreatedRgb(Supla::Element* previousElement) {
  auto createdElement = Supla::Element::last();
  if (createdElement == previousElement) {
    return nullptr;
  }
  return dynamic_cast<Supla::Control::RgbCctParsed*>(createdElement);
}

void deleteCreatedElement(Supla::Element* previousElement) {
  auto createdElement = Supla::Element::last();
  if (createdElement != previousElement) {
    delete createdElement;
  }
}

}  // namespace

TEST(Sd4linuxYamlConfigTests, AllowsRgbCctWithoutStateAndRejectsMissingParser) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addRgbCctParsed(YAML::Load("fade_effect_ms: 100"),
                                     0,
                                     nullptr));
  auto rgb = getCreatedRgb(previousElement);
  ASSERT_NE(rgb, nullptr);
  EXPECT_FALSE(rgb->isParameterConfigured("state"));
  deleteCreatedElement(previousElement);

  previousElement = Supla::Element::last();
  EXPECT_FALSE(config.addRgbCctParsed(YAML::Load("state: status"),
                                      0,
                                      nullptr));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests,
     MapsRgbCctStateOnValuesAndOfflineHandlingThroughYaml) {
  SimpleTime time;
  TestLinuxYamlConfig config;
  FakeYamlSource source;
  FakeYamlParser parser(&source);
  auto previousElement = Supla::Element::last();

  auto channel = YAML::Load(
      "state: status\n"
      "state_on_values: [7]\n"
      "offline_on_invalid_state: true\n"
      "channel_number: 3\n");
  EXPECT_TRUE(config.addRgbCctParsed(channel, 0, &parser));

  auto rgb = getCreatedRgb(previousElement);
  ASSERT_NE(rgb, nullptr);
  EXPECT_TRUE(rgb->isParameterConfigured("state"));
  EXPECT_EQ(rgb->getChannelNumber(), 3);

  parser.state = 7;
  EXPECT_EQ(rgb->getStateValue(), 1);
  EXPECT_FALSE(rgb->isOffline());

  parser.state = 6;
  EXPECT_EQ(rgb->getStateValue(), 0);
  EXPECT_FALSE(rgb->isOffline());

  parser.state = -1;
  EXPECT_EQ(rgb->getStateValue(), -1);
  EXPECT_TRUE(rgb->isOffline());

  deleteCreatedElement(previousElement);
}
