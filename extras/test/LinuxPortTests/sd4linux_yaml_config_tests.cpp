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
#include <iterator>
#include <string>
#include <variant>
#include <vector>

#include "linux_yaml_config.h"
#include "supla/control/action_trigger_parsed.h"
#include "supla/control/custom_hvac.h"
#include "supla/control/custom_relay.h"
#include "supla/control/rgbcct_parsed.h"
#include "supla/control/virtual_relay.h"

extern "C" const char *supla_test_get_last_log();
extern "C" void supla_test_clear_last_log();

namespace Supla::Linux {
void initExtensions() {
}
}  // namespace Supla::Linux

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
  bool stateValid = true;

  double getValue(const std::string&) override {
    return 0;
  }

  std::variant<int, bool, std::string> getStateValue(
      const std::string&) override {
    valid = stateValid;
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
  explicit FakePayload(bool basedOnIndex = false)
      : Supla::Payload::Payload(nullptr), basedOnIndex(basedOnIndex) {
  }

  bool isBasedOnIndex() override {
    return basedOnIndex;
  }

  void turnOn(const std::string&,
              std::variant<int, bool, std::string>) override {
  }

  void turnOff(const std::string&,
               std::variant<int, bool, std::string>) override {
  }

  void addKey(const std::string& key, int index) override {
    lastKey = key;
    lastIndex = index;
  }

  std::string lastKey;
  int lastIndex = -1;

 private:
  bool basedOnIndex;
};

class TestLinuxYamlConfig : public Supla::LinuxYamlConfig {
 public:
  TestLinuxYamlConfig() : Supla::LinuxYamlConfig("") {
  }

  using Supla::LinuxYamlConfig::addActionTriggerParsed;
  using Supla::LinuxYamlConfig::addRgbCctParsed;
  using Supla::LinuxYamlConfig::addCustomHvac;
  using Supla::LinuxYamlConfig::addCustomChannel;
  using Supla::LinuxYamlConfig::addCustomRelay;
  using Supla::LinuxYamlConfig::addCmdRelay;
  using Supla::LinuxYamlConfig::addCmdValve;
  using Supla::LinuxYamlConfig::addCmdRollerShutter;
  using Supla::LinuxYamlConfig::addHvac;
  using Supla::LinuxYamlConfig::addOutput;
  using Supla::LinuxYamlConfig::addVirtualRelay;
  using Supla::LinuxYamlConfig::addSource;
  using Supla::LinuxYamlConfig::loadTopLevelSources;
  using Supla::LinuxYamlConfig::parseChannel;
  using Supla::LinuxYamlConfig::saveGuidAuth;
  using Supla::LinuxYamlConfig::config;
  bool hasPendingSave() const { return saveDelayMs != 0; }
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

TEST_F(Sd4linuxYamlCredentialTests,
       RetriesFailedDelayedSaveAfterPathRecovery) {
  SimpleTime time;
  const auto storagePath = tempDirectory / "config_storage.bin";
  ASSERT_TRUE(std::filesystem::create_directory(storagePath));
  TestLinuxYamlConfig config;
  config.config["state_files_path"] = tempDirectory.string();
  ASSERT_TRUE(config.setString("test_key", "test_value"));
  config.saveWithDelay(1);
  time.advance(2);
  config.saveIfNeeded();
  EXPECT_TRUE(config.hasPendingSave());

  ASSERT_TRUE(std::filesystem::remove(storagePath));
  time.advance(4999);
  config.saveIfNeeded();
  EXPECT_FALSE(std::filesystem::exists(storagePath));
  time.advance(2);
  config.saveIfNeeded();
  EXPECT_FALSE(config.hasPendingSave());
  EXPECT_TRUE(std::filesystem::is_regular_file(storagePath));
}

TEST_F(Sd4linuxYamlCredentialTests,
       SavesNewAndExistingConfigStorageWithOwnerOnlyPermissions) {
  UmaskGuard umaskGuard(0022);
  const auto storagePath = tempDirectory / "config_storage.bin";
  TestLinuxYamlConfig config;
  config.config["state_files_path"] = tempDirectory.string();
  ASSERT_TRUE(config.setString("test_key", "test_value"));
  std::array<uint8_t, 256> serialized = {};
  const auto size = config.serializeToMemory(serialized.data(),
                                             serialized.size());
  ASSERT_GT(size, 0U);
  const std::string expected(reinterpret_cast<char*>(serialized.data()), size);

  for (bool existing : {false, true}) {
    if (existing) {
      std::ofstream oldFile(storagePath, std::ios::binary);
      ASSERT_TRUE(oldFile.is_open());
      oldFile << std::string(1024, 'x');
      oldFile.close();
      ASSERT_EQ(::chmod(storagePath.c_str(), 0644), 0);
    }
    config.commit();

    struct stat fileStat = {};
    ASSERT_EQ(::stat(storagePath.c_str(), &fileStat), 0);
    EXPECT_EQ(fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), 0600);
    std::ifstream saved(storagePath, std::ios::binary);
    ASSERT_TRUE(saved.is_open());
    const std::string actual{std::istreambuf_iterator<char>(saved),
                             std::istreambuf_iterator<char>()};
    EXPECT_EQ(actual, expected);
  }
}

TEST_F(Sd4linuxYamlCredentialTests,
       RejectsConfigStorageSymlinkWithoutModifyingItsTarget) {
  const auto targetPath = tempDirectory / "target";
  const auto storagePath = tempDirectory / "config_storage.bin";
  {
    std::ofstream target(targetPath);
    ASSERT_TRUE(target.is_open());
    target << "unchanged";
  }
  ASSERT_EQ(::chmod(targetPath.c_str(), 0644), 0);
  ASSERT_EQ(::symlink(targetPath.c_str(), storagePath.c_str()), 0);
  TestLinuxYamlConfig config;
  config.config["state_files_path"] = tempDirectory.string();
  ASSERT_TRUE(config.setString("test_key", "test_value"));
  supla_test_clear_last_log();

  config.commit();

  EXPECT_NE(std::string(supla_test_get_last_log()).find(
                "failed to write read/write config storage file"),
            std::string::npos);
  struct stat fileStat = {};
  ASSERT_EQ(::stat(targetPath.c_str(), &fileStat), 0);
  EXPECT_EQ(fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), 0644);
  std::ifstream target(targetPath);
  std::string content;
  std::getline(target, content);
  EXPECT_EQ(content, "unchanged");
}

TEST_F(Sd4linuxYamlCredentialTests, LogsConfigStorageWriteFailure) {
  TestLinuxYamlConfig config;
  config.config["state_files_path"] = (tempDirectory / "missing").string();
  ASSERT_TRUE(config.setString("test_key", "test_value"));
  supla_test_clear_last_log();

  config.commit();

  EXPECT_NE(std::string(supla_test_get_last_log()).find(
                "failed to write read/write config storage file"),
            std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(tempDirectory / "missing"));
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
     RelayDefaultFunctionKeepsWeeklyScheduleOptIn) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addVirtualRelay(
      YAML::Load("default_function: light_switch"), 0));
  auto relay = dynamic_cast<Supla::Control::VirtualRelay*>(
      Supla::Element::last());
  ASSERT_NE(relay, nullptr);
  EXPECT_EQ(relay->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_LIGHTSWITCH);
  EXPECT_FALSE(relay->getChannel()->isWeeklyScheduleAvailable());
  deleteCreatedElement(previousElement);

  previousElement = Supla::Element::last();
  EXPECT_TRUE(config.addVirtualRelay(
      YAML::Load("default_function: light_switch\nweekly_schedule: true"),
      0));
  relay = dynamic_cast<Supla::Control::VirtualRelay*>(Supla::Element::last());
  ASSERT_NE(relay, nullptr);
  EXPECT_TRUE(relay->isWeeklyScheduleAvailable());
  EXPECT_TRUE(relay->getChannel()->isWeeklyScheduleAvailable());
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests,
     ParsedChannelsAcceptDefaultFunctionNumberAsCommonParameter) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addRgbCctParsed(
      YAML::Load("default_function_number: 180"), 0, nullptr));
  auto rgb = getCreatedRgb(previousElement);
  ASSERT_NE(rgb, nullptr);
  EXPECT_EQ(rgb->getChannel()->getDefaultFunction(), 180);
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests,
     ActionTriggerAcceptsCommonDefaultFunctionNumber) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addActionTriggerParsed(
      YAML::Load("name: at\ndefault_function_number: 700"), 0));
  auto at = dynamic_cast<Supla::Control::ActionTriggerParsed*>(
      Supla::Element::last());
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getChannel()->getDefaultFunction(), 700);
  EXPECT_FALSE(at->isWeeklyScheduleSupported());
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests,
     ActionTriggerWeeklyScheduleCanBeEnabledFromYaml) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addActionTriggerParsed(
      YAML::Load("name: at\nweekly_schedule: true"), 0));
  auto at = dynamic_cast<Supla::Control::ActionTriggerParsed*>(
      Supla::Element::last());
  ASSERT_NE(at, nullptr);
  EXPECT_TRUE(at->isWeeklyScheduleSupported());
  EXPECT_TRUE(at->getChannel()->isWeeklyScheduleAvailable());
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests,
     ParsedRelayWeeklyScheduleCanBeEnabledFromYaml) {
  TestLinuxYamlConfig config;
  FakePayload payload(true);
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addCustomRelay(
      YAML::Load("default_function: light_switch\nweekly_schedule: true"),
      0,
      nullptr,
      &payload));
  auto relay = dynamic_cast<Supla::Control::CustomRelay*>(
      Supla::Element::last());
  ASSERT_NE(relay, nullptr);
  EXPECT_TRUE(relay->isWeeklyScheduleAvailable());
  EXPECT_TRUE(relay->getChannel()->isWeeklyScheduleAvailable());
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, RejectsUnknownRelayDefaultFunction) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_FALSE(config.addVirtualRelay(
      YAML::Load("default_function: coffee_machine"), 0));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, UsesFixedTlsSuplaServerPort) {
  TestLinuxYamlConfig config;
  config.config = YAML::Load("port: 2015\n");

  EXPECT_EQ(config.getSuplaServerPort(), 2016);
}

TEST(Sd4linuxYamlConfigTests, UsesDefaultAndConfiguredSuplaProtocolVersion) {
  TestLinuxYamlConfig config;

  config.config = YAML::Load("supla: {}\n");
  EXPECT_EQ(config.getProtoVersion(), 27);

  config.config = YAML::Load("supla:\n  proto: 29\n");
  EXPECT_EQ(config.getProtoVersion(), 29);

  config.config = YAML::Load("supla:\n  proto: 22\n");
  EXPECT_EQ(config.getProtoVersion(), 23);

  config.config = YAML::Load("supla:\n  proto: 30\n");
  EXPECT_EQ(config.getProtoVersion(), 29);
}

TEST(Sd4linuxYamlConfigTests, RejectsQosForMqttSource) {
  TestLinuxYamlConfig config;

  EXPECT_EQ(config.addSource(YAML::Load(
                "type: MQTT\n"
                "state_topic: state\n"
                "qos: 1\n")),
            nullptr);
}

TEST(Sd4linuxYamlConfigTests, UnusedMqttDoesNotRequireSecurityFields) {
  ASSERT_EQ(Supla::Element::begin(), nullptr);
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();
  config.config = YAML::Load("channels: [{type: VirtualRelay}]");
  EXPECT_TRUE(config.loadChannels());
  deleteCreatedElement(previousElement);
  ASSERT_EQ(Supla::Element::begin(), nullptr);
}

TEST(Sd4linuxYamlConfigTests, RequiresExplicitMqttSecurityForSourceAndOutput) {
  for (const char* fields : {"", "use_ssl: true",
                             "use_ssl: false, port: 8883"}) {
    SCOPED_TRACE(fields);
    TestLinuxYamlConfig config;
    config.config = YAML::Load(std::string("mqtt: {host: localhost, ") +
                               fields + "}");
    EXPECT_EQ(config.addSource(YAML::Load(
                  "type: MQTT\nstate_topic: state\n")), nullptr);
    EXPECT_EQ(config.addOutput(YAML::Load(
                  "type: MQTT\ncontrol_topic: control\n")), nullptr);
    EXPECT_FALSE(config.loadTopLevelSources(YAML::Load(
        "named: {type: MQTT, state_topic: state}")));
  }
}

TEST(Sd4linuxYamlConfigTests, RejectsIncompleteMqttDuringChannelLoading) {
  for (const char* usage : {
           "channels: [{type: VirtualRelay, source: "
           "{type: MQTT, state_topic: state}}]",
           "channels: [{type: VirtualRelay, output: "
           "{type: MQTT, control_topic: control}}]",
           "sources: {named: {type: MQTT, state_topic: state}}\n"
           "channels: [{type: VirtualRelay, source: {use: named}}]",
           "channels:\n"
           "- {type: VirtualRelay, source: "
           "{name: named, type: MQTT, state_topic: state}}\n"
           "- {type: VirtualRelay, source: {use: named}}"}) {
    TestLinuxYamlConfig config;
    config.config = YAML::Load(std::string(usage) +
                               "\nmqtt: {host: localhost, use_ssl: true}");
    EXPECT_FALSE(config.loadChannels());
  }
}

TEST(Sd4linuxYamlConfigTests,
     ExplicitMqttSecurityPreservesPortsAndVerification) {
  struct Case {
    const char* fields;
    int port;
    bool ssl;
    bool verify;
  };
  const Case cases[] = {
      {"use_ssl: false", 1883, false, false},
      {"use_ssl: true, verify_ca: false", 8883, true, false},
      {"use_ssl: true, verify_ca: true", 8883, true, true},
      {"use_ssl: true, verify_ca: true, ca_file: ca.pem", 8883, true, true},
      {"use_ssl: true, verify_ca: true, port: 1884", 1884, true, true},
      {"use_ssl: false, port: 1884", 1884, false, false}};
  for (const auto& entry : cases) {
    SCOPED_TRACE(entry.fields);
    TestLinuxYamlConfig config;
    config.config = YAML::Load(std::string("mqtt: {host: localhost, ") +
                               entry.fields + "}");
    ASSERT_TRUE(config.isValidMqttConfig());
    EXPECT_EQ(config.getMqttClientUseSSL(), entry.ssl);
    EXPECT_EQ(config.getMqttClientVerifyCA(), entry.verify);
    EXPECT_EQ(config.getMqttClientPort(), entry.port);
    EXPECT_NE(config.addSource(YAML::Load(
                  "type: MQTT\nstate_topic: state\n")), nullptr);
    EXPECT_NE(config.addOutput(YAML::Load(
                  "type: MQTT\ncontrol_topic: control\n")), nullptr);
    ASSERT_TRUE(config.loadTopLevelSources(YAML::Load(
        "named: {type: MQTT, state_topic: state}")));
    EXPECT_NE(config.addSource(YAML::Load("use: named")), nullptr);
    EXPECT_NE(config.addSource(YAML::Load(
                  "name: inline\ntype: MQTT\nstate_topic: state\n")), nullptr);
    EXPECT_NE(config.addSource(YAML::Load("use: inline")), nullptr);
    EXPECT_NE(config.addOutput(YAML::Load(
                  "name: control\ntype: MQTT\ncontrol_topic: control\n")),
              nullptr);
    EXPECT_NE(config.addOutput(YAML::Load("use: control")), nullptr);
  }
}

TEST(Sd4linuxYamlConfigTests,
     RejectsInvalidMqttBooleanTypesWithFieldDiagnostic) {
  for (const char* value : {"null", "[]", "{}", "1", "broken", "\"false\"",
                            "!!str true"}) {
    for (const char* field : {"use_ssl", "verify_ca"}) {
      SCOPED_TRACE(std::string(field) + ": " + value);
      TestLinuxYamlConfig config;
      config.config = YAML::Load("mqtt: {host: localhost, use_ssl: true}");
      config.config["mqtt"][field] = YAML::Load(value);
      supla_test_clear_last_log();
      EXPECT_FALSE(config.isValidMqttConfig());
      EXPECT_NE(std::string(supla_test_get_last_log()).find(
                    std::string("mqtt.") + field + " must be a boolean"),
                std::string::npos);
      EXPECT_EQ(config.addSource(YAML::Load(
                    "type: MQTT\nstate_topic: state\n")), nullptr);
      EXPECT_EQ(config.addOutput(YAML::Load(
                    "type: MQTT\ncontrol_topic: control\n")), nullptr);
    }
  }
}

TEST(Sd4linuxYamlConfigTests, ExplainsMissingSecurityAndConflictingPort) {
  const char* fields[] = {"", "use_ssl: true", "use_ssl: false, port: 8883"};
  const char* messages[] = {
      "Config: mqtt.use_ssl is required (true/false)",
      "Config: mqtt.verify_ca is required when mqtt.use_ssl=true",
      "Config: mqtt.use_ssl=false conflicts with mqtt.port=8883"};
  for (int i = 0; i < 3; ++i) {
    TestLinuxYamlConfig config;
    config.config = YAML::Load(std::string("mqtt: {host: localhost, ") +
                               fields[i] + "}");
    supla_test_clear_last_log();
    EXPECT_FALSE(config.isValidMqttConfig());
    EXPECT_NE(std::string(supla_test_get_last_log()).find(messages[i]),
              std::string::npos);
  }
}

TEST(Sd4linuxYamlConfigTests, RejectsQosForReusedMqttSource) {
  TestLinuxYamlConfig config;
  config.config = YAML::Load("mqtt: {host: localhost, use_ssl: false}");

  ASSERT_TRUE(config.loadTopLevelSources(YAML::Load(
      "mqtt_state:\n"
      "  type: MQTT\n"
      "  state_topic: state\n")));

  EXPECT_EQ(config.addSource(YAML::Load(
                "use: mqtt_state\n"
                "qos: 1\n")),
            nullptr);
}

TEST(Sd4linuxYamlConfigTests, AcceptsMqttOutputQosRange) {
  TestLinuxYamlConfig config;
  config.config = YAML::Load("mqtt: {host: localhost, use_ssl: false}");

  for (int qos = 0; qos <= 2; qos++) {
    EXPECT_NE(config.addOutput(YAML::Load(
                  "type: MQTT\n"
                  "control_topic: control\n"
                  "qos: " +
                  std::to_string(qos))),
              nullptr);
  }
}

TEST(Sd4linuxYamlConfigTests, RejectsMqttOutputQosOutsideRange) {
  TestLinuxYamlConfig config;
  config.config = YAML::Load("mqtt: {host: localhost, use_ssl: false}");

  EXPECT_EQ(config.addOutput(YAML::Load(
                "type: MQTT\n"
                "control_topic: control\n"
                "qos: -1\n")),
            nullptr);
  EXPECT_EQ(config.addOutput(YAML::Load(
                "type: MQTT\n"
                "control_topic: control\n"
                "qos: 3\n")),
            nullptr);
}

TEST(Sd4linuxYamlConfigTests, RejectsCustomCaSecurityLevel) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("supla_yaml_security_level_" +
                     std::to_string(getpid()) + ".yaml");
  {
    std::ofstream output(path);
    ASSERT_TRUE(output.is_open());
    output << "security_level: 1\n";
  }

  Supla::LinuxYamlConfig config(path.string());
  EXPECT_FALSE(config.init());

  std::error_code error;
  EXPECT_TRUE(std::filesystem::remove(path, error));
  EXPECT_FALSE(error) << error.message();
}

TEST(Sd4linuxYamlConfigTests, RejectsUnsupportedSecurityLevel) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("supla_yaml_security_level_unsupported_" +
                     std::to_string(getpid()) + ".yaml");
  {
    std::ofstream output(path);
    ASSERT_TRUE(output.is_open());
    output << "security_level: 3\n";
  }

  Supla::LinuxYamlConfig config(path.string());
  EXPECT_FALSE(config.init());

  std::error_code error;
  EXPECT_TRUE(std::filesystem::remove(path, error));
  EXPECT_FALSE(error) << error.message();
}

TEST(Sd4linuxYamlConfigTests, RejectsUnsupportedLogLevel) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("supla_yaml_log_level_unsupported_" +
                     std::to_string(getpid()) + ".yaml");
  {
    std::ofstream output(path);
    ASSERT_TRUE(output.is_open());
    output << "log_level: trace\n";
  }

  Supla::LinuxYamlConfig config(path.string());
  EXPECT_FALSE(config.init());

  std::error_code error;
  EXPECT_TRUE(std::filesystem::remove(path, error));
  EXPECT_FALSE(error) << error.message();
}

TEST(Sd4linuxYamlConfigTests, RejectsUnsupportedInitialStateForRelays) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();
  auto invalidState = YAML::Load("initial_state: invalid\n");

  EXPECT_FALSE(config.addVirtualRelay(invalidState, 0));
  EXPECT_EQ(Supla::Element::last(), previousElement);
  EXPECT_FALSE(config.addCmdRelay(invalidState, 0, nullptr));
  EXPECT_EQ(Supla::Element::last(), previousElement);

  FakePayload payload;
  EXPECT_FALSE(config.addCustomRelay(invalidState, 0, nullptr, &payload));
  EXPECT_EQ(Supla::Element::last(), previousElement);
}

TEST(Sd4linuxYamlConfigTests, RejectsUnsupportedHvacDefaultFunction) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();
  auto invalidFunction = YAML::Load(
      "cmd_on: on\n"
      "cmd_off: off\n"
      "main_thermometer_channel_no: 1\n"
      "default_function: unknown\n");

  EXPECT_FALSE(config.addHvac(invalidFunction, 0));
  deleteCreatedElement(previousElement);

  FakePayload payload;
  previousElement = Supla::Element::last();
  auto customInvalidFunction = YAML::Load(
      "main_thermometer_channel_no: 1\n"
      "default_function: unknown\n");
  EXPECT_FALSE(config.addCustomHvac(customInvalidFunction, 0, &payload));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, ParseErrorDoesNotLogYamlSource) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("supla_yaml_parse_error_" + std::to_string(getpid()) +
                     ".yaml");
  {
    std::ofstream output(path);
    ASSERT_TRUE(output.is_open());
    output << "password: [SUPER_SECRET_TEST_VALUE\n";
  }

  supla_test_clear_last_log();
  Supla::LinuxYamlConfig config(path.string());
  EXPECT_FALSE(config.init());

  std::string log = supla_test_get_last_log();
  EXPECT_NE(log.find("Config file YAML error"), std::string::npos);
  EXPECT_NE(log.find("line"), std::string::npos);
  EXPECT_NE(log.find("column"), std::string::npos);
  EXPECT_EQ(log.find("SUPER_SECRET_TEST_VALUE"), std::string::npos);

  std::error_code error;
  EXPECT_TRUE(std::filesystem::remove(path, error));
  EXPECT_FALSE(error) << error.message();
}

TEST(Sd4linuxYamlConfigTests, AllowsParserlessRgbCctForcedBatteryPowered) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addRgbCctParsed(
      YAML::Load("force_battery_powered: true"), 0, nullptr));
  auto rgb = getCreatedRgb(previousElement);
  ASSERT_NE(rgb, nullptr);
  EXPECT_TRUE(rgb->getChannel()->isBatteryPowered());
  EXPECT_FALSE(rgb->isParameterConfigured("force_battery_powered"));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, AllowsParserlessRgbCctWithoutForcedBattery) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addRgbCctParsed(
      YAML::Load("force_battery_powered: false"), 0, nullptr));
  auto rgb = getCreatedRgb(previousElement);
  ASSERT_NE(rgb, nullptr);
  EXPECT_FALSE(rgb->getChannel()->isBatteryPowered());
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests,
     RejectsParserlessRgbCctDynamicBatteryParameters) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_FALSE(config.addRgbCctParsed(YAML::Load("battery_level: level"),
                                      0,
                                      nullptr));
  deleteCreatedElement(previousElement);

  previousElement = Supla::Element::last();
  EXPECT_FALSE(config.addRgbCctParsed(YAML::Load("battery_powered: powered"),
                                      0,
                                      nullptr));
  deleteCreatedElement(previousElement);

  previousElement = Supla::Element::last();
  EXPECT_FALSE(config.addRgbCctParsed(YAML::Load("battery_state: state"),
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

TEST(Sd4linuxYamlConfigTests, ParserBackedRgbCctCanForceBatteryPowered) {
  TestLinuxYamlConfig config;
  FakeYamlSource source;
  FakeYamlParser parser(&source);
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addRgbCctParsed(
      YAML::Load("force_battery_powered: true"), 0, &parser));
  auto rgb = getCreatedRgb(previousElement);
  ASSERT_NE(rgb, nullptr);
  EXPECT_TRUE(rgb->getChannel()->isBatteryPowered());
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, MapsBatteryStateAndWholeDeviceFlag) {
  SimpleTime time;
  TestLinuxYamlConfig config;
  FakeYamlSource source;
  FakeYamlParser parser(&source);
  auto previousElement = Supla::Element::last();

  auto channel = YAML::Load(
      "battery_state: battery\n"
      "battery_state_applies_to_whole_device: true\n");
  ASSERT_TRUE(config.addRgbCctParsed(channel, 0, &parser));
  auto rgb = getCreatedRgb(previousElement);
  ASSERT_NE(rgb, nullptr);
  EXPECT_TRUE(rgb->isParameterConfigured("battery_state"));

  parser.state = std::string("low");
  ASSERT_TRUE(rgb->refreshParserSource());
  EXPECT_EQ(rgb->getChannel()->getBatteryState(), Supla::BatteryState::Low);
  EXPECT_TRUE(rgb->getChannel()->isBatteryStateAppliedToWholeDevice());

  TDSC_ChannelState channelState = {};
  channelState.ChannelNumber = rgb->getChannelNumber();
  rgb->handleGetChannelState(&channelState);
  EXPECT_NE(channelState.Fields & SUPLA_CHANNELSTATE_FIELD_BATTERY_STATE, 0);
  EXPECT_NE(channelState.Fields &
                SUPLA_CHANNELSTATE_FIELD_DEVICE_BATTERY_STATE,
            0);
  EXPECT_EQ(channelState.BatteryState, SUPLA_BATTERY_STATE_LOW);

  parser.state = std::string("normal");
  rgb->updateBatteryInfoFlags();
  EXPECT_EQ(rgb->getChannel()->getBatteryState(), Supla::BatteryState::Normal);

  parser.state = 1;
  rgb->updateBatteryInfoFlags();
  EXPECT_EQ(rgb->getChannel()->getBatteryState(), Supla::BatteryState::Low);

  parser.state = 0;
  rgb->updateBatteryInfoFlags();
  EXPECT_EQ(rgb->getChannel()->getBatteryState(), Supla::BatteryState::Normal);

  parser.state = std::string("unknown");
  rgb->updateBatteryInfoFlags();
  EXPECT_EQ(rgb->getChannel()->getBatteryState(), Supla::BatteryState::NotSet);
  EXPECT_FALSE(rgb->getChannel()->isBatteryStateAppliedToWholeDevice());

  parser.state = std::string("low");
  parser.stateValid = false;
  rgb->updateBatteryInfoFlags();
  EXPECT_EQ(rgb->getChannel()->getBatteryState(), Supla::BatteryState::NotSet);

  channelState = {};
  channelState.ChannelNumber = rgb->getChannelNumber();
  rgb->handleGetChannelState(&channelState);
  EXPECT_EQ(channelState.Fields & SUPLA_CHANNELSTATE_FIELD_BATTERY_STATE, 0);
  EXPECT_EQ(channelState.Fields &
                SUPLA_CHANNELSTATE_FIELD_DEVICE_BATTERY_STATE,
            0);

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

TEST(Sd4linuxYamlConfigTests, PreservesArbitraryCustomChannelTypes) {
  TestLinuxYamlConfig config;

  for (uint32_t type : {static_cast<uint32_t>(SUPLA_CHANNELTYPE_DIGIGLASS),
                        424242u}) {
    auto previousElement = Supla::Element::last();
    auto channel = YAML::Load("channel_type: " + std::to_string(type) +
                              "\nvalue: 01 02 03 04 05 06 07 08\n");

    ASSERT_TRUE(config.addCustomChannel(channel, 0, nullptr));
    auto createdElement = Supla::Element::last();
    ASSERT_NE(createdElement, previousElement);

    TDS_SuplaDeviceChannel_D registration = {};
    createdElement->getChannel()->fillDeviceChannelStruct(&registration);
    EXPECT_EQ(registration.Type, type);
    deleteCreatedElement(previousElement);
  }
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

TEST(Sd4linuxYamlConfigTests, MapsCustomHvacJsonStateField) {
  TestLinuxYamlConfig config;
  FakePayload payload;
  auto previousElement = Supla::Element::last();
  auto channel = YAML::Load(
      "type: CustomHvac\n"
      "main_thermometer_channel_no: 1\n"
      "set_state: state\n");

  EXPECT_TRUE(config.addCustomHvac(channel, 0, &payload));
  EXPECT_EQ(payload.lastKey, "state");
  EXPECT_EQ(payload.lastIndex, -1);

  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, RejectsCustomRelayWithoutPayload) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_FALSE(config.parseChannel(YAML::Load("type: CustomRelay\n"), 0));
  EXPECT_EQ(Supla::Element::last(), previousElement);
}

TEST(Sd4linuxYamlConfigTests, RequiresStateFieldForCustomRelayJsonPayload) {
  TestLinuxYamlConfig config;
  FakePayload payload;
  auto previousElement = Supla::Element::last();

  EXPECT_FALSE(config.addCustomRelay(YAML::Load("type: CustomRelay\n"),
                                     0,
                                     nullptr,
                                     &payload));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, AllowsCustomRelaySimplePayloadWithoutStateField) {
  TestLinuxYamlConfig config;
  FakePayload payload(true);
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.addCustomRelay(YAML::Load("type: CustomRelay\n"),
                                    0,
                                    nullptr,
                                    &payload));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, RejectsCmdValveWithoutRequiredCommand) {
  TestLinuxYamlConfig config;
  FakeYamlSource source;
  FakeYamlParser parser(&source);
  auto previousElement = Supla::Element::last();

  EXPECT_FALSE(config.addCmdValve(YAML::Load(
                                     "cmd_close: close\n"
                                     "state: state\n"),
                                 0,
                                 &parser));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests,
     RejectsCmdRollerShutterWithoutRequiredCommand) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_FALSE(config.addCmdRollerShutter(YAML::Load(
                                              "cmd_up_on: up\n"
                                              "cmd_up_off: up_off\n"
                                              "cmd_down_on: down\n"),
                                          0,
                                          nullptr));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, RejectsChannelNumberOutsideAllowedRange) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_FALSE(config.parseChannel(
      YAML::Load("type: VirtualRelay\nchannel_number: -1\n"), 0));
  EXPECT_EQ(Supla::Element::last(), previousElement);

  EXPECT_FALSE(config.parseChannel(
      YAML::Load("type: VirtualRelay\nchannel_number: 128\n"), 0));
  EXPECT_EQ(Supla::Element::last(), previousElement);
}

TEST(Sd4linuxYamlConfigTests,
     RejectsDuplicateChannelNumberWithoutReassigningExistingChannel) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_TRUE(config.parseChannel(
      YAML::Load("type: VirtualRelay\nchannel_number: 5\n"), 0));
  auto firstChannel = Supla::Element::last();
  ASSERT_NE(firstChannel, previousElement);
  EXPECT_EQ(firstChannel->getChannelNumber(), 5);

  EXPECT_FALSE(config.parseChannel(
      YAML::Load("type: VirtualRelay\nchannel_number: 5\n"), 1));
  EXPECT_EQ(Supla::Element::last(), firstChannel);
  EXPECT_EQ(firstChannel->getChannelNumber(), 5);

  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, RejectsIconIdOutsideAllowedRange) {
  TestLinuxYamlConfig config;
  auto previousElement = Supla::Element::last();

  EXPECT_FALSE(config.addVirtualRelay(YAML::Load("icon_id: -1\n"), 0));
  deleteCreatedElement(previousElement);

  previousElement = Supla::Element::last();
  EXPECT_FALSE(config.addVirtualRelay(YAML::Load("icon_id: 256\n"), 0));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, RejectsParsedIconIdOutsideAllowedRange) {
  TestLinuxYamlConfig config;
  FakeYamlSource source;
  FakeYamlParser parser(&source);
  auto previousElement = Supla::Element::last();

  EXPECT_FALSE(config.addRgbCctParsed(YAML::Load("icon_id: 256\n"),
                                      0,
                                      &parser));
  deleteCreatedElement(previousElement);
}

TEST(Sd4linuxYamlConfigTests, RejectsInvalidMultiDocumentConfiguration) {
  for (const char* fields : {
           "sub_topics: [a, a]",
           "expiration_time_sec: -1",
           "expiration_time_sec: 4294967296",
           "expiration_time_sec: 99999999999999999999999",
           "expiration_time_sec: broken",
           "expiration_time_sec: 1.5",
           "expiration_time_sec: []"}) {
    SCOPED_TRACE(fields);
    TestLinuxYamlConfig config;
    config.config = YAML::Load(std::string(
        "mqtt: {host: localhost, use_ssl: false}\n"
        "sources:\n  data:\n    type: MQTT\n    state_topic: yaml-docs\n    ") +
        fields + "\nchannels: [{type: VirtualRelay}]\n");
    EXPECT_FALSE(config.loadChannels());
  }
  for (const char* mapping : {"unknown:/data/val", "a:data/val",
                               "a:/bad~2escape", ":/data/val"}) {
    SCOPED_TRACE(mapping);
    TestLinuxYamlConfig config;
    config.config = YAML::Load(std::string(
        "mqtt: {host: localhost, use_ssl: false}\n"
        "channels:\n- type: ElectricityMeterParsed\n"
        "  source: {type: MQTT, state_topic: yaml-docs, sub_topics: [a]}\n"
        "  parser: {type: Json, multi_document: true}\n"
        "  phase_1: [{voltage: '") + mapping + "'}]\n");
    auto previous = Supla::Element::last();
    EXPECT_FALSE(config.loadChannels());
    deleteCreatedElement(previous);
  }
}

TEST(Sd4linuxYamlConfigTests, NamedMultiDocumentSourceAndParsersCanBeShared) {
  for (int mode : {-1, 0, 1}) {
    TestLinuxYamlConfig config;
    config.config = YAML::Load(R"(
mqtt: {host: localhost, use_ssl: false}
sources:
  docs: {type: MQTT, state_topic: shared-docs, sub_topics: ["0505", "0508"]}
parsers:
  first: {source: docs, type: Json}
  second: {source: docs, type: Json}
channels:
- type: ElectricityMeterParsed
  parser: first
  phase_1: [{voltage: /data/val}]
- type: ElectricityMeterParsed
  parser: {use: first}
  phase_1: [{current: /data/val}]
- type: ElectricityMeterParsed
  parser: second
  phase_1: [{voltage: /data/val}]
)");
    if (mode >= 0) {
      config.config["parsers"]["first"]["multi_document"] = mode == 1;
      config.config["parsers"]["second"]["multi_document"] = mode == 1;
    }
    if (mode == 1) {
      config.config["channels"][0]["phase_1"][0]["voltage"] = "0505:/data/val";
      config.config["channels"][1]["phase_1"][0]["current"] = "0508:/data/val";
      config.config["channels"][2]["phase_1"][0]["voltage"] = "0505:/data/val";
    }
    auto previous = Supla::Element::last();
    EXPECT_TRUE(config.loadChannels());
    while (Supla::Element::last() != previous) {
      delete Supla::Element::last();
    }
  }
}

TEST(Sd4linuxYamlConfigTests, LegacyMqttSimpleYamlRemainsSupported) {
  TestLinuxYamlConfig config;
  config.config = YAML::Load(R"(
mqtt: {host: localhost, use_ssl: false}
channels:
- type: ThermHygroMeterParsed
  source: {type: MQTT, state_topic: device, sub_topics: [temperature, humidity]}
  parser: {type: Simple}
  temperature: 0
  humidity: 1
)");
  auto previous = Supla::Element::last();
  EXPECT_TRUE(config.loadChannels());
  deleteCreatedElement(previous);
}
