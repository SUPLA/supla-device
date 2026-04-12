/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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
#include <simple_time.h>
#include <supla/parser/parser.h>
#include <supla/sensor/thermometer.h>
#include <supla/sensor/thermometer_parsed.h>
#include <supla/source/source.h>

#include <string>
#include <variant>

namespace {

class FakeSd4linuxThermometerSource : public Supla::Source::Source {
 public:
  bool connected = true;

  std::string getContent() override {
    return {};
  }

  bool isConnected() override {
    return connected;
  }
};

class FakeSd4linuxThermometerParser : public Supla::Parser::Parser {
 public:
  explicit FakeSd4linuxThermometerParser(Supla::Source::Source *source)
      : Supla::Parser::Parser(source) {
  }

  bool validAfterRefresh = true;
  double numericValue = 0;
  std::variant<int, bool, std::string> stateValue = 0;
  int refreshCount = 0;

  double getValue(const std::string &) override {
    return numericValue;
  }

  std::variant<int, bool, std::string> getStateValue(
      const std::string &) override {
    return stateValue;
  }

  bool isBasedOnIndex() override {
    return false;
  }

 protected:
  bool refreshSource() override {
    refreshCount++;
    valid = validAfterRefresh;
    return valid;
  }
};

class Sd4linuxThermometerParsedTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }
};

}  // namespace

TEST_F(Sd4linuxThermometerParsedTests, ReadsTemperatureValueFromParser) {
  SimpleTime time;
  FakeSd4linuxThermometerSource source;
  FakeSd4linuxThermometerParser parser(&source);
  parser.numericValue = 21.75;

  Supla::Sensor::ThermometerParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Temperature, "temperature");

  sensor.onInit();

  EXPECT_DOUBLE_EQ(sensor.getValue(), 21.75);
  EXPECT_DOUBLE_EQ(sensor.getChannel()->getValueDouble(), 21.75);
  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
}

TEST_F(Sd4linuxThermometerParsedTests, UpdatesPublishedTemperatureOnIterate) {
  SimpleTime time;
  FakeSd4linuxThermometerSource source;
  FakeSd4linuxThermometerParser parser(&source);
  parser.numericValue = 20.5;

  Supla::Sensor::ThermometerParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Temperature, "temperature");

  sensor.onInit();
  ASSERT_DOUBLE_EQ(sensor.getChannel()->getValueDouble(), 20.5);

  parser.numericValue = 23.25;

  time.advance(10001);
  sensor.iterateAlways();

  EXPECT_DOUBLE_EQ(sensor.getValue(), 23.25);
  EXPECT_DOUBLE_EQ(sensor.getChannel()->getValueDouble(), 23.25);
  EXPECT_EQ(parser.refreshCount, 2);
}

TEST_F(Sd4linuxThermometerParsedTests,
       ReturnsTemperatureNotAvailableWhenParserDataIsInvalid) {
  SimpleTime time;
  FakeSd4linuxThermometerSource source;
  FakeSd4linuxThermometerParser parser(&source);
  parser.numericValue = 19.5;

  Supla::Sensor::ThermometerParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Temperature, "temperature");

  sensor.onInit();
  ASSERT_DOUBLE_EQ(sensor.getChannel()->getValueDouble(), 19.5);

  parser.validAfterRefresh = false;

  time.advance(10001);
  sensor.iterateAlways();

  EXPECT_LE(sensor.getValue(), TEMPERATURE_NOT_AVAILABLE);
  EXPECT_LE(sensor.getChannel()->getValueDouble(), TEMPERATURE_NOT_AVAILABLE);
  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
}

TEST_F(Sd4linuxThermometerParsedTests,
       ReturnsChannelOnlineAfterParserDataBecomesValidAgain) {
  SimpleTime time;
  FakeSd4linuxThermometerSource source;
  FakeSd4linuxThermometerParser parser(&source);
  parser.numericValue = 18.0;

  Supla::Sensor::ThermometerParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Temperature, "temperature");

  sensor.onInit();

  parser.validAfterRefresh = false;
  time.advance(10001);
  sensor.iterateAlways();

  ASSERT_TRUE(sensor.getChannel()->isStateOnline());
  ASSERT_LE(sensor.getChannel()->getValueDouble(), TEMPERATURE_NOT_AVAILABLE);

  parser.validAfterRefresh = true;
  parser.numericValue = 24.5;

  time.advance(10001);
  sensor.iterateAlways();

  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
  EXPECT_DOUBLE_EQ(sensor.getValue(), 24.5);
  EXPECT_DOUBLE_EQ(sensor.getChannel()->getValueDouble(), 24.5);
}

TEST_F(Sd4linuxThermometerParsedTests,
       MarksChannelOfflineImmediatelyWhenSourceDrops) {
  SimpleTime time;
  FakeSd4linuxThermometerSource source;
  FakeSd4linuxThermometerParser parser(&source);
  parser.numericValue = 22.0;

  Supla::Sensor::ThermometerParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Temperature, "temperature");

  sensor.onInit();
  ASSERT_TRUE(sensor.getChannel()->isStateOnline());

  source.connected = false;

  time.advance(10001);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->isStateOnline());
}
