// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <clock_stub.h>
#include <gtest/gtest.h>
#include <protocol_layer_mock.h>
#include <simple_time.h>
#include <supla/control/hvac_parsed.h>
#include <supla/control/hvac_remote.h>
#include <supla/output/output.h>
#include <supla/parser/json.h>
#include <unistd.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <string>
#include <vector>

namespace {

class HeatingFeedbackSource : public Supla::Source::Source {
 public:
  std::string getContent() override { return content; }
  bool isConnected() override { return connected; }
  std::string content;
  bool connected = true;
};

class SettingsOutput : public Supla::Output::Output {
 public:
  bool putContent(int) override { return false; }
  bool putContent(bool) override { return false; }
  bool putContent(const std::vector<int> &) override { return false; }
  bool putContent(const std::string &value) override {
    writes.push_back(nlohmann::json::parse(value));
    return acceptsWrites;
  }
  bool acceptsWrites = true;
  std::vector<nlohmann::json> writes;
};

class HvacParsedForTests : public Supla::Control::HvacParsed {
 public:
  HvacParsedForTests(const std::string &cmdOn, const std::string &cmdOff)
      : HvacParsed(cmdOn, cmdOff, "", "") {
  }

  using Supla::Control::HvacBase::setOutput;
};

class Sd4linuxHvacParsedTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();

    const auto directoryTemplate =
        std::filesystem::temp_directory_path() /
        ("supla_hvac_parsed_tests_" + std::to_string(getpid()) + "_XXXXXX");
    std::string writableDirectoryTemplate = directoryTemplate.string();
    std::vector<char> writableDirectory(writableDirectoryTemplate.begin(),
                                        writableDirectoryTemplate.end());
    writableDirectory.push_back('\0');

    ASSERT_NE(mkdtemp(writableDirectory.data()), nullptr);
    tempDirectory = writableDirectory.data();
  }

  void TearDown() override {
    if (tempDirectory.empty()) {
      return;
    }

    std::error_code error;
    std::filesystem::remove_all(tempDirectory, error);
    EXPECT_FALSE(error) << error.message();
    Supla::Channel::resetToDefaults();
  }

  std::filesystem::path filePath(const std::string &name) const {
    return tempDirectory / name;
  }

  static void configureSubfunction(HvacParsedForTests *hvac,
                                   uint8_t subfunction) {
    hvac->getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
    hvac->setSubfunction(subfunction);
  }

  SimpleTime time;
  std::filesystem::path tempDirectory;
};

}  // namespace

TEST_F(Sd4linuxHvacParsedTests, ValueOneExecutesOnCommand) {
  const auto onPath = filePath("on");
  const auto offPath = filePath("off");
  HvacParsedForTests hvac("touch " + onPath.string(),
                          "touch " + offPath.string());
  configureSubfunction(&hvac, SUPLA_HVAC_SUBFUNCTION_HEAT);

  hvac.setOutput(1, true);

  EXPECT_TRUE(std::filesystem::exists(onPath));
  EXPECT_FALSE(std::filesystem::exists(offPath));
}

TEST_F(Sd4linuxHvacParsedTests, NegativeOneExecutesOnCommandForCooling) {
  const auto onPath = filePath("on");
  const auto offPath = filePath("off");
  HvacParsedForTests hvac("touch " + onPath.string(),
                          "touch " + offPath.string());
  configureSubfunction(&hvac, SUPLA_HVAC_SUBFUNCTION_COOL);

  hvac.setOutput(-1, true);

  EXPECT_TRUE(std::filesystem::exists(onPath));
  EXPECT_FALSE(std::filesystem::exists(offPath));
}

TEST_F(Sd4linuxHvacParsedTests, ZeroExecutesOffCommand) {
  const auto onPath = filePath("on");
  const auto offPath = filePath("off");
  HvacParsedForTests hvac("touch " + onPath.string(),
                          "touch " + offPath.string());
  configureSubfunction(&hvac, SUPLA_HVAC_SUBFUNCTION_HEAT);

  hvac.setOutput(0, true);

  EXPECT_FALSE(std::filesystem::exists(onPath));
  EXPECT_TRUE(std::filesystem::exists(offPath));
}

TEST_F(Sd4linuxHvacParsedTests, RemoteOutputReportsPercentageAndBinaryCooling) {
  HeatingFeedbackSource source;
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(10);
  SettingsOutput output;
  Supla::Control::HvacRemote hvac(
      parser, output, {"position", "cooling", "", "", ""}, false, true);
  hvac.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);

  source.content = R"({"position":35,"cooling":0})";
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_FALSE(hvac.isOutputControlledInternally());
  EXPECT_EQ(hvac.getChannel()->getHvacIsOnPercent(), 35);
  EXPECT_TRUE(hvac.getChannel()->isHvacFlagHeating());
  EXPECT_TRUE(output.writes.empty());

  source.content = R"({"position":60,"cooling":0})";
  time.advance(20);
  hvac.iterateAlways();
  ::testing::NiceMock<ProtocolLayerMock> protocol;
  EXPECT_CALL(protocol, sendChannelValueChanged(
      hvac.getChannelNumber(), ::testing::_, SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE,
      ::testing::_))
      .WillOnce([](uint8_t, int8_t *value, unsigned char, uint32_t) {
        EXPECT_EQ(value[0], 62);
      });
  hvac.getChannel()->sendUpdate();
  EXPECT_EQ(hvac.getChannel()->getHvacIsOnPercent(), 60);
  EXPECT_TRUE(output.writes.empty());

  source.content = R"({"cooling":1})";
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_EQ(hvac.getChannel()->getHvacIsOnRaw(), 1);
  EXPECT_TRUE(hvac.getChannel()->isHvacFlagCooling());

  source.content = R"({"position":0,"cooling":0})";
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_EQ(hvac.getChannel()->getHvacIsOnRaw(), 2);
  EXPECT_FALSE(hvac.getChannel()->isHvacFlagHeating());
  EXPECT_FALSE(hvac.getChannel()->isHvacFlagCooling());
}

TEST_F(Sd4linuxHvacParsedTests, RemoteSettingsImportAndSendWithoutEcho) {
  HeatingFeedbackSource source;
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(10);
  SettingsOutput output;
  Supla::Control::HvacRemote hvac(
      parser, output, {"position", "", "mode", "target", ""}, false, false);
  hvac.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.setSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
  hvac.setTemperatureRoomMin(500);
  hvac.setTemperatureRoomMax(3500);

  source.content = R"({"position":35,"mode":"heat","target":21.5})";
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_EQ(hvac.getMode(), SUPLA_HVAC_MODE_HEAT);
  EXPECT_EQ(hvac.getTemperatureSetpointHeat(), 2150);
  EXPECT_TRUE(output.writes.empty());

  ASSERT_TRUE(hvac.applyNewRuntimeSettings(SUPLA_HVAC_MODE_HEAT, 2200,
                                          INT16_MIN));
  output.acceptsWrites = false;
  hvac.iterateAlways();
  ASSERT_EQ(output.writes.size(), 1);
  EXPECT_EQ(output.writes.back(),
            nlohmann::json({{"mode", "heat"}, {"setpoint_heat", 22.0}}));
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_EQ(output.writes.size(), 1);

  output.acceptsWrites = true;
  time.advance(1000);
  hvac.iterateAlways();
  ASSERT_EQ(output.writes.size(), 2);
  hvac.iterateAlways();
  EXPECT_EQ(output.writes.size(), 2);
  EXPECT_EQ(hvac.getTemperatureSetpointHeat(), 2200);

  source.content = R"({"position":60,"mode":"heat","target":22})";
  time.advance(20);
  hvac.iterateAlways();
  source.content = R"({"position":40,"mode":"heat","target":23})";
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_EQ(hvac.getTemperatureSetpointHeat(), 2300);
  EXPECT_EQ(output.writes.size(), 2);

  hvac.setTargetMode(SUPLA_HVAC_MODE_OFF);
  hvac.iterateAlways();
  ASSERT_EQ(output.writes.size(), 3);
  EXPECT_EQ(output.writes.back().at("mode"), "off");
}

TEST_F(Sd4linuxHvacParsedTests, RemoteSetpointReadbackPreservesCountdown) {
  ClockStub clock;
  HeatingFeedbackSource source;
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(10);
  SettingsOutput output;
  Supla::Control::HvacRemote hvac(
      parser, output, {"position", "", "mode", "target", ""}, false, false);
  hvac.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.setSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
  hvac.setTemperatureRoomMin(500);
  hvac.setTemperatureRoomMax(3500);
  source.content = R"({"position":35,"mode":"heat","target":21})";
  time.advance(20);
  hvac.iterateAlways();
  ASSERT_TRUE(hvac.applyNewRuntimeSettings(SUPLA_HVAC_MODE_HEAT, 2100,
                                          INT16_MIN, 600));
  const auto ends = hvac.getCountDownTimerEnds();
  ASSERT_TRUE(hvac.isCountdownEnabled());

  source.content = R"({"position":35,"mode":"heat","target":22})";
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_EQ(hvac.getTemperatureSetpointHeat(), 2200);
  EXPECT_TRUE(hvac.isCountdownEnabled());
  EXPECT_EQ(hvac.getCountDownTimerEnds(), ends);
  EXPECT_TRUE(output.writes.empty());
}

TEST_F(Sd4linuxHvacParsedTests, RemoteSetpointReadbackKeepsScheduleActive) {
  ClockStub clock;
  HeatingFeedbackSource source;
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(10);
  SettingsOutput output;
  Supla::Control::HvacRemote hvac(
      parser, output, {"position", "", "mode", "target", ""}, false, false);
  hvac.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.setSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
  hvac.setTemperatureRoomMin(500);
  hvac.setTemperatureRoomMax(3500);
  source.content = R"({"position":35,"mode":"heat","target":21})";
  time.advance(20);
  hvac.iterateAlways();
  ASSERT_TRUE(hvac.setProgram(1, SUPLA_HVAC_MODE_HEAT, 2100, INT16_MIN));
  ASSERT_TRUE(hvac.setProgram(2, SUPLA_HVAC_MODE_HEAT, 1900, INT16_MIN));
  const int quarter = hvac.getCurrentQuarter();
  ASSERT_TRUE(hvac.setWeeklySchedule(quarter, 1));
  ASSERT_TRUE(hvac.setWeeklySchedule(quarter + 1, 2));
  hvac.setWeeklyScheduleEnabled(true);

  source.content = R"({"position":35,"mode":"heat","target":22})";
  time.advance(1001);
  hvac.iterateAlways();
  EXPECT_TRUE(hvac.isWeeklyScheduleEnabled());
  EXPECT_TRUE(hvac.isWeeklyScheduleTemporalOverride());
  EXPECT_EQ(hvac.getTemperatureSetpointHeat(), 2200);
  EXPECT_TRUE(output.writes.empty());

  time.advance(15 * 60 * 1000);
  hvac.iterateAlways();
  EXPECT_TRUE(hvac.isWeeklyScheduleEnabled());
  EXPECT_FALSE(hvac.isWeeklyScheduleTemporalOverride());
  EXPECT_EQ(hvac.getTemperatureSetpointHeat(), 1900);
  ASSERT_EQ(output.writes.size(), 1);
  EXPECT_EQ(output.writes.back().at("setpoint_heat"), 19.0);
}

TEST_F(Sd4linuxHvacParsedTests, RemoteFeedbackDisconnectMarksChannelOffline) {
  HeatingFeedbackSource source;
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(10);
  SettingsOutput output;
  Supla::Control::HvacRemote hvac(
      parser, output, {"position", "", "", "", ""}, false, false);
  hvac.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.setSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
  source.content = R"({"position":60})";
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_TRUE(hvac.getChannel()->isStateOnline());
  EXPECT_TRUE(hvac.getChannel()->isHvacFlagHeating());

  source.connected = false;
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_FALSE(hvac.getChannel()->isStateOnline());
  EXPECT_TRUE(hvac.getChannel()->isHvacFlagHeating());

  source.connected = true;
  source.content = R"({"position":0})";
  time.advance(20);
  hvac.iterateAlways();
  EXPECT_TRUE(hvac.getChannel()->isStateOnline());
  EXPECT_FALSE(hvac.getChannel()->isHvacFlagHeating());
}
