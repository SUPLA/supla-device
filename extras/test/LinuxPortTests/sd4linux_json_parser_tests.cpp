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

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>

#include <simple_time.h>
#include <supla/parser/json.h>
#include <supla/sensor/binary_parsed.h>
#include <supla/sensor/impulse_counter_parsed.h>
#include <supla/source/source.h>

namespace {

class FakeJsonSource : public Supla::Source::Source {
 public:
  std::string content;
  bool connected = true;
  int readCount = 0;

  std::string getContent() override {
    readCount++;
    return content;
  }

  bool isConnected() override {
    return connected;
  }
};

class Sd4linuxJsonParserTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    source.content.clear();
    source.connected = true;
    source.readCount = 0;
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }

  FakeJsonSource source;
};

class TestImpulseCounterParsed : public Supla::Sensor::ImpulseCounterParsed {
 public:
  explicit TestImpulseCounterParsed(Supla::Parser::Parser *parser)
      : Supla::Sensor::ImpulseCounterParsed(parser) {
  }

  TChannelConfig_ImpulseCounter getDefaultConfig() {
    TChannelConfig_ImpulseCounter config = {};
    int size = 0;
    fillChannelConfig(&config, &size, SUPLA_CONFIG_TYPE_DEFAULT);
    EXPECT_EQ(size, sizeof(TChannelConfig_ImpulseCounter));
    return config;
  }
};

}  // namespace

TEST_F(Sd4linuxJsonParserTests, RejectsNanStringValue) {
  SimpleTime time;
  source.content = R"({"counter":"nan"})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());
  EXPECT_TRUE(parser.isValid());

  const double value = parser.getValue("counter");
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(parser.isValid());
}

TEST_F(Sd4linuxJsonParserTests, RejectsInfStringValue) {
  SimpleTime time;
  source.content = R"({"counter":"inf"})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());
  EXPECT_TRUE(parser.isValid());

  const double value = parser.getValue("counter");
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(parser.isValid());
}

TEST_F(Sd4linuxJsonParserTests, RejectsOrdinaryInvalidStringValues) {
  SimpleTime time;
  source.content = R"({"counter":"abc"})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());
  EXPECT_TRUE(parser.isValid());

  const double value = parser.getValue("counter");
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(parser.isValid());
}

TEST_F(Sd4linuxJsonParserTests, InvalidNumericValueDoesNotPoisonSharedParser) {
  SimpleTime time;
  source.content = R"({"state":"ready","temperature":"warm"})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());
  EXPECT_TRUE(parser.isValid());

  const double value = parser.getValue("temperature");
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(parser.isValid());

  auto state = parser.getStateValue("state");
  ASSERT_TRUE(std::holds_alternative<std::string>(state));
  EXPECT_EQ(std::get<std::string>(state), "ready");
  EXPECT_TRUE(parser.isValid());
}

TEST_F(Sd4linuxJsonParserTests,
       InvalidNumericValueDoesNotMakeBinaryChannelOfflineOnCacheHit) {
  SimpleTime time;
  source.content = R"({"state":"ready","temperature":"warm"})";
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(5000);

  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");
  sensor.setOnValues({std::string("ready")});
  sensor.setUseOfflineOnInvalidState(true);

  time.advance(101);
  sensor.iterateAlways();

  ASSERT_TRUE(sensor.getChannel()->isStateOnline());
  ASSERT_EQ(source.readCount, 1);

  EXPECT_EQ(parser.getValue("temperature"), 0);
  ASSERT_FALSE(parser.isValid());
  ASSERT_TRUE(parser.isSourceValid());

  sensor.getChannel()->clearSendValue();

  time.advance(101);
  sensor.iterateAlways();

  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
  EXPECT_FALSE(sensor.getChannel()->isUpdateReady());
  EXPECT_EQ(source.readCount, 1);
}

TEST_F(Sd4linuxJsonParserTests, ReadsNestedSmartThingsCounterValues) {
  SimpleTime time;
  source.content =
      R"({"components":{"main":{)"
      R"("powerConsumptionReport":{"powerConsumption":{)"
      R"("value":{"energy":173300}}},)"
      R"("samsungce.waterConsumptionReport":{"waterConsumption":{)"
      R"("value":{"cumulativeAmount":21820900}}}}}})";
  Supla::Parser::Json parser(&source);

  ASSERT_TRUE(parser.refreshParserSource());

  EXPECT_EQ(
      parser.getValue(
          "/components/main/powerConsumptionReport/powerConsumption/value/"
          "energy"),
      173300);
  EXPECT_EQ(
      parser.getValue(
          "/components/main/samsungce.waterConsumptionReport/waterConsumption/"
          "value/cumulativeAmount"),
      21820900);
}

TEST_F(Sd4linuxJsonParserTests,
       ImpulseCounterParsedKeepsRawCounterAndConfiguresImpulsesPerUnit) {
  SimpleTime time;
  source.content = R"({"energy":173300})";
  Supla::Parser::Json parser(&source);

  TestImpulseCounterParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Counter, "energy");
  sensor.setDefaultImpulsesPerUnit(1000);
  sensor.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_IC_ELECTRICITY_METER);

  sensor.onInit();

  EXPECT_EQ(sensor.getChannel()->getValueInt64(), 173300u);
  EXPECT_EQ(sensor.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_IC_ELECTRICITY_METER);

  auto config = sensor.getDefaultConfig();
  EXPECT_EQ(config.ImpulsesPerUnit, 1000);
}

TEST_F(Sd4linuxJsonParserTests, ImpulseCounterRejectsNanStringValue) {
  SimpleTime time;
  source.content = R"({"counter":"nan"})";
  Supla::Parser::Json parser(&source);

  Supla::Sensor::ImpulseCounterParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::Counter, "counter");

  sensor.onInit();

  EXPECT_FALSE(parser.isValid());
  EXPECT_EQ(sensor.getChannel()->getValueInt64(), 0u);
}
