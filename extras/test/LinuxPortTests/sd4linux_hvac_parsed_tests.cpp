// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/control/hvac_parsed.h>
#include <unistd.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <string>
#include <vector>

namespace {

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
