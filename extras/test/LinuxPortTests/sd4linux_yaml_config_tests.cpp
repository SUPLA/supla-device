// SPDX-FileCopyrightText: AC SOFTWARE SP. Z. O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla-common/proto.h>
#include <supla/element.h>
#include <supla/parser/parser.h>
#include <supla/output/mqtt.h>
#include <supla/source/mqtt_src.h>
#include <supla/source/source.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <fstream>
#include <filesystem>  // NOLINT(build/c++17)
#include <string>
#include <variant>
#include <vector>

#include "linux_yaml_config.h"
#include "supla/control/custom_hvac.h"
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

class FakePayload : public Supla::Payload::Payload {
 public:
  FakePayload() : Supla::Payload::Payload(nullptr) {
  }

  bool isBasedOnIndex() override {
    return false;
  }

  void turnOn(const std::string&,
              std::variant<int, bool, std::string>) override {
  }

  void turnOff(const std::string&,
               std::variant<int, bool, std::string>) override {
  }
};

class TestLinuxYamlConfig : public Supla::LinuxYamlConfig {
 public:
  TestLinuxYamlConfig() : Supla::LinuxYamlConfig("") {
  }

  using Supla::LinuxYamlConfig::addRgbCctParsed;
  using Supla::LinuxYamlConfig::addCustomHvac;
  using Supla::LinuxYamlConfig::parseChannel;
  using Supla::LinuxYamlConfig::saveGuidAuth;
};

class UmaskGuard {
 public:
  explicit UmaskGuard(mode_t mask) : originalMask(::umask(mask)) {
  }

  ~UmaskGuard() {
    ::umask(originalMask);
  }

 private:
  mode_t originalMask;
};

class Sd4linuxYamlCredentialTests : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto directoryTemplate =
        std::filesystem::temp_directory_path() /
        ("supla_yaml_credentials_tests_" + std::to_string(getpid()) +
         "_XXXXXX");
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
  }

  static bool saveCredentials(const std::filesystem::path& stateDirectory) {
    TestLinuxYamlConfig config;
    std::array<char, SUPLA_GUID_SIZE> guid;
    std::array<char, SUPLA_AUTHKEY_SIZE> authkey;
    guid.fill(0x11);
    authkey.fill(0x22);

    return config.setGUID(guid.data()) && config.setAuthKey(authkey.data()) &&
           config.saveGuidAuth(stateDirectory.string());
  }

  std::filesystem::path tempDirectory;
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

TEST_F(Sd4linuxYamlCredentialTests,
       SavesCredentialsWithOwnerOnlyPermissionsRegardlessOfUmask) {
  UmaskGuard umaskGuard(0022);
  const auto stateDirectory = tempDirectory / "state";
  const auto credentialPath = stateDirectory / "guid_auth.yaml";

  ASSERT_TRUE(saveCredentials(stateDirectory));

  struct stat fileStat = {};
  ASSERT_EQ(::stat(credentialPath.c_str(), &fileStat), 0);
  EXPECT_EQ(fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), 0600);
}

TEST_F(Sd4linuxYamlCredentialTests,
       TightensPermissionsOfExistingCredentialFileWhenRewritten) {
  const auto stateDirectory = tempDirectory / "state";
  const auto credentialPath = stateDirectory / "guid_auth.yaml";
  ASSERT_TRUE(std::filesystem::create_directories(stateDirectory));

  std::ofstream existingCredentials(credentialPath);
  ASSERT_TRUE(existingCredentials.is_open());
  existingCredentials << "old credentials";
  existingCredentials.close();
  ASSERT_EQ(::chmod(credentialPath.c_str(), 0644), 0);

  ASSERT_TRUE(saveCredentials(stateDirectory));

  struct stat fileStat = {};
  ASSERT_EQ(::stat(credentialPath.c_str(), &fileStat), 0);
  EXPECT_EQ(fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), 0600);
}

TEST_F(Sd4linuxYamlCredentialTests,
       RejectsCredentialPathSymlinkWithoutModifyingItsTarget) {
  const auto stateDirectory = tempDirectory / "state";
  const auto credentialPath = stateDirectory / "guid_auth.yaml";
  const auto targetPath = tempDirectory / "credential-target";
  ASSERT_TRUE(std::filesystem::create_directories(stateDirectory));

  std::ofstream target(targetPath);
  ASSERT_TRUE(target.is_open());
  target << "unchanged";
  target.close();
  ASSERT_EQ(::symlink(targetPath.c_str(), credentialPath.c_str()), 0);

  EXPECT_FALSE(saveCredentials(stateDirectory));

  struct stat linkStat = {};
  ASSERT_EQ(::lstat(credentialPath.c_str(), &linkStat), 0);
  EXPECT_TRUE(S_ISLNK(linkStat.st_mode));

  std::ifstream targetAfterSave(targetPath);
  std::string targetContents;
  std::getline(targetAfterSave, targetContents);
  EXPECT_EQ(targetContents, "unchanged");
}

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

TEST(Sd4linuxYamlConfigTests, RejectsCustomHvacWithoutPayload) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();
  auto channel = YAML::Load(
      "type: CustomHvac\n"
      "main_thermometer_channel_no: 1\n");

  EXPECT_FALSE(config.parseChannel(channel, 0));
  EXPECT_EQ(Supla::Element::last(), previousElement);
}

TEST(Sd4linuxYamlConfigTests, AcceptsCustomHvacWithPayload) {
  TestLinuxYamlConfig config;
  FakePayload payload;
  auto previousElement = Supla::Element::last();
  auto channel = YAML::Load(
      "type: CustomHvac\n"
      "main_thermometer_channel_no: 1\n");

  EXPECT_TRUE(config.addCustomHvac(channel, 0, &payload));

  auto createdElement = Supla::Element::last();
  auto customHvac = dynamic_cast<Supla::Control::CustomHvac*>(createdElement);
  ASSERT_NE(createdElement, previousElement);
  ASSERT_NE(customHvac, nullptr);
  delete createdElement;
}
