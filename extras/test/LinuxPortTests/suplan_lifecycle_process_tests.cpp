// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <gtest/gtest.h>
#include <linux_yaml_config.h>
#include <network_mock.h>
#include <simple_time.h>
#include <supla/clock/clock.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/sensor/container.h>
#include <supla/sensor/virtual_binary.h>
#include <timer_mock.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {
using Location = Supla::Device::ServerChannelLocation;

class DynamicTopology : public Supla::Element {
 public:
  void onLoadTopology(SuplaDeviceClass *) override {
    ++calls;
    container.reset(new Supla::Sensor::Container);
    EXPECT_TRUE(container->getChannel()->restoreChannelNumber(7));
    binary.reset(new Supla::Sensor::VirtualBinary);
    EXPECT_TRUE(binary->getChannel()->restoreChannelNumber(3));
  }
  int calls = 0;
  std::unique_ptr<Supla::Sensor::Container> container;
  std::unique_ptr<Supla::Sensor::VirtualBinary> binary;
};

// Uses the production Linux persistence and the complete SDK begin lifecycle.
// Only authoritative Server inputs are injected; M3/M4 Server conversion and
// RemoteChannel dataplane are deliberately absent from this Device fixture.
class ProcessDevice {
 public:
  explicit ProcessDevice(const std::string &yaml)
      : config(yaml), protocol(&device, 29) {}
  ~ProcessDevice() {
    protocol.serverIdentity().load(nullptr);
    topology.binary.reset();
    topology.container.reset();
    if (SuplaDevice.getClock()) {
      delete SuplaDevice.getClock();
    }
  }
  void start() {
    ASSERT_TRUE(device.begin(29));
    ASSERT_EQ(topology.calls, 1);
    ASSERT_NE(topology.container, nullptr);
    ASSERT_EQ(topology.container->getChannelNumber(), 7);
    ASSERT_EQ(topology.binary->getChannelNumber(), 3);
  }
  void identity(int deviceId, int firstId) {
    auto &state = protocol.serverIdentity();
    ASSERT_TRUE(state.registrationStarted());
    state.registrationSucceeded();
    TSD_SuplaDeviceIdentities snapshot = {};
    snapshot.DeviceId = deviceId;
    snapshot.ChannelCount = 2;
    snapshot.ChannelId[0] = firstId;
    snapshot.ChannelId[1] = firstId + 1;
    ASSERT_EQ(state.accept(snapshot).Result, SUPLA_SUPLAN_RESULT_OK);
  }
  Supla::LinuxYamlConfig config;
  testing::NiceMock<NetworkMock> network;
  testing::NiceMock<TimerMock> timer;
  SimpleTime time;
  SuplaDeviceClass device;
  Supla::Protocol::SuplaSrpc protocol;
  DynamicTopology topology;
};

class SupLanLifecycleProcessTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    Supla::RegisterDevice::resetToDefaults();
    const auto directoryTemplate =
        std::filesystem::temp_directory_path() / "suplan-process-XXXXXX";
    const std::string pattern = directoryTemplate.string();
    std::vector<char> writablePattern(pattern.begin(), pattern.end());
    writablePattern.push_back('\0');
    ASSERT_NE(mkdtemp(writablePattern.data()), nullptr) << strerror(errno);
    directory = writablePattern.data();
    yaml = directory + "/device.yaml";
    std::ofstream file(yaml);
    file << "name: M1B lifecycle process fixture\nstate_files_path: "
         << directory << "\nsecurity_level: 0\nsupla:\n  proto: 29\n"
         << "  server: 127.0.0.2\n  mail: fixture@example.invalid\n"
         << "channels: []\n";
    ASSERT_TRUE(file.good());
  }
  void prepareIdentity() {
    ProcessDevice seed(yaml);
    seed.start();
    seed.identity(100, 501);
    seed.protocol.serverIdentity().syncDone();
    ASSERT_TRUE(seed.protocol.serverIdentity().serverSyncComplete());
  }
  std::string directory;
  std::string yaml;
};

TEST_F(SupLanLifecycleProcessTests,
       AbruptExitRestoresIdentityAndLegacyLocalConfig) {
  EXPECT_EXIT(
      {
        ProcessDevice child(yaml);
        child.start();
        Supla::Sensor::ContainerConfig legacy;
        legacy.sensorData[0].channelNumber = 3;
        legacy.sensorData[0].fillLevel = 80;
        if (!child.config.setBlob("7_container",
                                  reinterpret_cast<const char *>(&legacy),
                                  sizeof(legacy)) ||
            !child.config.commit()) {
          _exit(1);
        }
        child.identity(100, 501);
        _exit(testing::Test::HasFailure() ? 1 : 73);
      },
      testing::ExitedWithCode(73), "");
  ProcessDevice rebooted(yaml);
  rebooted.start();
  auto &state = rebooted.protocol.serverIdentity();
  EXPECT_TRUE(state.identityAvailable());
  EXPECT_EQ(state.resolve(502).channelNumber, 3);
  auto owner = rebooted.topology.container.get();
  EXPECT_EQ(owner->getFillLevelForSensor(3), 80);
}

// Abrupt exit bypasses all destructors and delayed saves. Cold start must
// reconstruct Channels before restoring the persisted transition marker.
TEST_F(SupLanLifecycleProcessTests,
       TransitionSurvivesCrashThenResyncCompletes) {
  prepareIdentity();
  EXPECT_EXIT(
      {
        ProcessDevice child(yaml);
        child.start();
        child.identity(101, 901);
        EXPECT_TRUE(child.protocol.serverIdentity().identityTransition());
        _exit(testing::Test::HasFailure() ? 1 : 73);
      },
      testing::ExitedWithCode(73), "");
  ProcessDevice rebooted(yaml);
  rebooted.start();
  auto &state = rebooted.protocol.serverIdentity();
  EXPECT_EQ(state.serverDeviceId(), 101);
  EXPECT_TRUE(state.identityTransition());
  EXPECT_EQ(state.resolve(502).location, Location::kUnresolved);
  rebooted.identity(101, 901);
  state.syncDone();
  EXPECT_FALSE(state.identityTransition());
  EXPECT_EQ(state.resolve(902).channelNumber, 3);
}
}  // namespace
