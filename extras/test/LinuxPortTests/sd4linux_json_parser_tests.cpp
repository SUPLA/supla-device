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

#include <simple_time.h>
#include <supla/parser/json.h>
#include <supla/sensor/impulse_counter_parsed.h>
#include <supla/source/source.h>

namespace {

class FakeJsonSource : public Supla::Source::Source {
 public:
  std::string content;
  bool connected = true;

  std::string getContent() override {
    return content;
  }

  bool isConnected() override {
    return connected;
  }
};

class Sd4linuxJsonParserTests : public ::testing::Test {
 protected:
  void SetUp() override {
    source.content.clear();
    source.connected = true;
  }

  FakeJsonSource source;
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
