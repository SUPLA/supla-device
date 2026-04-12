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
#include <supla/sensor/binary_parsed.h>
#include <supla/source/source.h>

#include <string>
#include <variant>

namespace {

class FakeSd4linuxSource : public Supla::Source::Source {
 public:
  bool connected = true;

  std::string getContent() override {
    return {};
  }

  bool isConnected() override {
    return connected;
  }
};

class FakeSd4linuxParser : public Supla::Parser::Parser {
 public:
  explicit FakeSd4linuxParser(Supla::Source::Source *source)
      : Supla::Parser::Parser(source) {
  }

  bool validAfterRefresh = true;
  std::variant<int, bool, std::string> stateValue = 1;
  int refreshCount = 0;

  double getValue(const std::string &) override {
    return 0;
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

class Sd4linuxParserStateTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }
};

}  // namespace

TEST_F(Sd4linuxParserStateTests,
       KeepsChannelOfflineWhenParserIsStillInvalidDuringCacheHit) {
  SimpleTime time;
  FakeSd4linuxSource source;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);

  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");
  sensor.setUseOfflineOnInvalidState(true);

  time.advance(101);
  sensor.iterateAlways();

  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 1);

  parser.validAfterRefresh = false;

  time.advance(5001);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 2);

  sensor.getChannel()->clearSendValue();
  EXPECT_FALSE(sensor.getChannel()->isUpdateReady());

  time.advance(101);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 2);
  EXPECT_FALSE(sensor.getChannel()->isUpdateReady());
}

TEST_F(Sd4linuxParserStateTests,
       KeepsChannelOfflineWhenStateIsInvalidButSourceStaysConnected) {
  SimpleTime time;
  FakeSd4linuxSource source;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);
  parser.stateValue = -1;

  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");
  sensor.setUseOfflineOnInvalidState(true);

  time.advance(101);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 1);

  sensor.getChannel()->sendUpdate();

  time.advance(101);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->isStateOnline());
  EXPECT_FALSE(sensor.getChannel()->isUpdateReady());
  EXPECT_EQ(parser.refreshCount, 1);
}

TEST_F(
    Sd4linuxParserStateTests,
    KeepsOfflineWhenSourceBecomesUnavailableWithoutOfflineOnInvalidState) {
  SimpleTime time;
  FakeSd4linuxSource source;
  source.connected = false;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);

  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");

  time.advance(101);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 0);
}

TEST_F(Sd4linuxParserStateTests, ReturnsChannelOnlineAfterSourceReconnects) {
  SimpleTime time;
  FakeSd4linuxSource source;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);

  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");
  sensor.setUseOfflineOnInvalidState(true);

  time.advance(101);
  sensor.iterateAlways();

  ASSERT_TRUE(sensor.getChannel()->isStateOnline());
  ASSERT_EQ(parser.refreshCount, 1);

  source.connected = false;
  time.advance(101);
  sensor.iterateAlways();

  ASSERT_FALSE(sensor.getChannel()->isStateOnline());
  ASSERT_EQ(parser.refreshCount, 1);

  source.connected = true;
  sensor.getChannel()->clearSendValue();

  time.advance(5001);
  sensor.iterateAlways();

  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
  EXPECT_TRUE(sensor.getChannel()->isUpdateReady());
  EXPECT_EQ(parser.refreshCount, 2);
}

TEST_F(Sd4linuxParserStateTests,
       ReturnsChannelOnlineAfterParserDataBecomesValidAgain) {
  SimpleTime time;
  FakeSd4linuxSource source;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);

  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");
  sensor.setUseOfflineOnInvalidState(true);

  time.advance(101);
  sensor.iterateAlways();

  ASSERT_TRUE(sensor.getChannel()->isStateOnline());
  ASSERT_EQ(parser.refreshCount, 1);

  parser.validAfterRefresh = false;
  time.advance(5001);
  sensor.iterateAlways();

  ASSERT_FALSE(sensor.getChannel()->isStateOnline());
  ASSERT_EQ(parser.refreshCount, 2);

  parser.validAfterRefresh = true;
  sensor.getChannel()->clearSendValue();

  time.advance(5001);
  sensor.iterateAlways();

  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
  EXPECT_TRUE(sensor.getChannel()->isUpdateReady());
  EXPECT_EQ(parser.refreshCount, 3);
}

TEST_F(Sd4linuxParserStateTests,
       KeepsChannelOnlineWhenInvalidStateDoesNotMeanOffline) {
  SimpleTime time;
  FakeSd4linuxSource source;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);

  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");

  time.advance(101);
  sensor.iterateAlways();

  ASSERT_TRUE(sensor.getChannel()->isStateOnline());

  parser.validAfterRefresh = false;
  time.advance(5001);
  sensor.iterateAlways();

  EXPECT_TRUE(sensor.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 2);
}

TEST_F(Sd4linuxParserStateTests, ReadsBinaryStateFromBoolValues) {
  SimpleTime time;
  FakeSd4linuxSource source;
  FakeSd4linuxParser parser(&source);
  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");

  parser.stateValue = true;
  sensor.onInit();
  EXPECT_TRUE(sensor.getValue());
  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  parser.stateValue = false;
  time.advance(101);
  sensor.iterateAlways();
  EXPECT_FALSE(sensor.getValue());
  EXPECT_FALSE(sensor.getChannel()->getValueBool());
}

TEST_F(Sd4linuxParserStateTests, ReadsBinaryStateFromNumericValues) {
  SimpleTime time;
  FakeSd4linuxSource source;
  FakeSd4linuxParser parser(&source);
  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");

  parser.stateValue = 1;
  sensor.onInit();
  EXPECT_TRUE(sensor.getValue());
  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  parser.stateValue = 0;
  time.advance(101);
  sensor.iterateAlways();
  EXPECT_FALSE(sensor.getValue());
  EXPECT_FALSE(sensor.getChannel()->getValueBool());
}

TEST_F(Sd4linuxParserStateTests, ReadsBinaryStateFromTextValues) {
  SimpleTime time;
  FakeSd4linuxSource source;
  FakeSd4linuxParser parser(&source);
  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");

  parser.stateValue = std::string("ON");
  sensor.onInit();
  EXPECT_TRUE(sensor.getValue());
  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  parser.stateValue = std::string("no");
  time.advance(101);
  sensor.iterateAlways();
  EXPECT_FALSE(sensor.getValue());
  EXPECT_FALSE(sensor.getChannel()->getValueBool());
}

TEST_F(Sd4linuxParserStateTests,
       TimeoutBlocksRepeatedSourceOneUntilSourceReturnsToZero) {
  SimpleTime time;
  FakeSd4linuxSource source;
  FakeSd4linuxParser parser(&source);
  Supla::Sensor::BinaryParsed sensor(&parser);
  sensor.setMapping(Supla::Parser::State, "state");
  sensor.setTimeoutDs(25);

  parser.stateValue = 1;
  sensor.onInit();
  EXPECT_TRUE(sensor.getChannel()->getValueBool());

  for (int i = 0; i < 26; ++i) {
    time.advance(100);
    sensor.iterateAlways();
  }

  EXPECT_FALSE(sensor.getChannel()->getValueBool());

  parser.stateValue = 1;
  time.advance(101);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->getValueBool());

  parser.stateValue = 0;
  time.advance(101);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->getValueBool());

  parser.stateValue = -1;
  time.advance(101);
  sensor.iterateAlways();

  EXPECT_FALSE(sensor.getChannel()->getValueBool());

  parser.stateValue = 1;
  time.advance(101);
  sensor.iterateAlways();

  EXPECT_TRUE(sensor.getChannel()->getValueBool());
}
