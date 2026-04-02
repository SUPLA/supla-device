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
#include <supla/control/cmd_relay.h>
#include <supla/control/cmd_roller_shutter.h>
#include <supla/control/custom_relay.h>
#include <supla/control/rgbcct_parsed.h>
#include <supla/parser/parser.h>
#include <supla/payload/payload.h>
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

  int refreshCount = 0;

  double getValue(const std::string &) override {
    return 0;
  }

  std::variant<int, bool, std::string> getStateValue(
      const std::string &) override {
    return 1;
  }

  bool isBasedOnIndex() override {
    return false;
  }

 protected:
  bool refreshSource() override {
    refreshCount++;
    valid = true;
    return true;
  }
};

class FakeSd4linuxPayload : public Supla::Payload::Payload {
 public:
  FakeSd4linuxPayload() : Supla::Payload::Payload(nullptr) {
  }

  bool isBasedOnIndex() override {
    return false;
  }

  void turnOn(const std::string &,
              std::variant<int, bool, std::string>) override {
  }

  void turnOff(const std::string &,
               std::variant<int, bool, std::string>) override {
  }
};

class Sd4linuxParsedControlOfflineTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }
};

}  // namespace

TEST_F(Sd4linuxParsedControlOfflineTests,
       CmdRelayStaysOfflineWhenSourceIsDisconnected) {
  SimpleTime time;
  FakeSd4linuxSource source;
  source.connected = false;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);

  Supla::Control::CmdRelay relay(&parser);
  relay.setMapping("state", "state");

  time.advance(101);
  relay.iterateAlways();

  EXPECT_FALSE(relay.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 0);
}

TEST_F(Sd4linuxParsedControlOfflineTests,
       RgbCctParsedStaysOfflineWhenSourceIsDisconnected) {
  SimpleTime time;
  FakeSd4linuxSource source;
  source.connected = false;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);

  Supla::Control::RgbCctParsed rgb(&parser);
  rgb.setMapping("state", "state");

  time.advance(101);
  rgb.iterateAlways();

  EXPECT_FALSE(rgb.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 0);
}

TEST_F(Sd4linuxParsedControlOfflineTests,
       CustomRelayStaysOfflineWhenSourceIsDisconnected) {
  SimpleTime time;
  FakeSd4linuxSource source;
  source.connected = false;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);

  FakeSd4linuxPayload payload;
  Supla::Control::CustomRelay relay(&parser, &payload);

  time.advance(101);
  relay.iterateAlways();

  EXPECT_FALSE(relay.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 0);
}

TEST_F(Sd4linuxParsedControlOfflineTests,
       CmdRollerShutterStaysOfflineWhenSourceIsDisconnected) {
  SimpleTime time;
  FakeSd4linuxSource source;
  source.connected = false;
  FakeSd4linuxParser parser(&source);
  parser.setRefreshTime(5000);

  Supla::Control::CmdRollerShutter rs(&parser);
  rs.setMapping("state", "state");

  time.advance(101);
  rs.iterateAlways();

  EXPECT_FALSE(rs.getChannel()->isStateOnline());
  EXPECT_EQ(parser.refreshCount, 0);
}
