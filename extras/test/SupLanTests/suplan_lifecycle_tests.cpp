// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <supla/control/relay.h>
#include <gtest/gtest.h>
#include <network_mock.h>
#include <output_mock.h>
#include <simple_time.h>
#include <timer_mock.h>
#include <supla/channel_element.h>
#include <supla/clock/clock.h>
#include <supla/control/hvac_base.h>
#include <supla/control/valve_base.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/sensor/container.h>
#include <supla/storage/key_value.h>
#include <supla/storage/storage.h>
#include <supla/suplan/suplan_server_identity.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
using Identity = Supla::Device::ServerIdentity;
using Location = Supla::Device::ServerChannelLocation;

// NVS-like shared staged backend. A cut drops only uncommitted memory.
class PowerCutConfig : public Supla::KeyValue {
 public:
  bool init() override { return true; }
  using Supla::KeyValue::getBlobSize;
  bool commit() override {
    std::array<uint8_t, 8192> bytes;
    auto size = serializeToMemory(bytes.data(), bytes.size());
    if (size == SIZE_MAX) {
      return false;
    }
    durable.assign(bytes.begin(), bytes.begin() + size);
    return true;
  }
  void cut() {
    removeAllMemory();
    ASSERT_TRUE(durable.empty() ||
                initFromMemory(durable.data(), durable.size()));
  }
  std::vector<uint8_t> blob(const char *key) {
    int size = getBlobSize(key);
    if (size < 0) {
      return {};
    }
    std::vector<uint8_t> result(size);
    EXPECT_TRUE(getBlob(key, reinterpret_cast<char *>(result.data()), size));
    return result;
  }
  std::vector<uint8_t> durable;
};

class SupLanLifecycleTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    Supla::RegisterDevice::resetToDefaults();
    Supla::Storage::SetConfigInstance(&config);
  }
  void TearDown() override {
    identity.load(nullptr);
    Supla::Storage::SetConfigInstance(nullptr);
    if (SuplaDevice.getClock()) {
      delete SuplaDevice.getClock();
    }
  }
  void accept(int deviceId = 100, int firstId = 501) {
    identity.load(&config);
    registerAndAccept(deviceId, firstId);
  }
  void registerAndAccept(int deviceId = 100, int firstId = 501) {
    ASSERT_TRUE(identity.registrationStarted());
    identity.registrationSucceeded();
    TSD_SuplaDeviceIdentities snapshot = {};
    snapshot.DeviceId = deviceId;
    for (auto ch = Supla::Channel::Begin(); ch; ch = ch->next()) {
      snapshot.ChannelId[snapshot.ChannelCount] =
          firstId + snapshot.ChannelCount;
      ++snapshot.ChannelCount;
    }
    ASSERT_EQ(identity.accept(snapshot).Result, SUPLA_SUPLAN_RESULT_OK);
  }
  template <typename T>
  void install(const char *key, const T &value) {
    ASSERT_TRUE(config.setBlob(key, reinterpret_cast<const char *>(&value),
                               sizeof(value)));
    ASSERT_TRUE(config.commit());
  }
  PowerCutConfig config;
  Identity identity;
  SimpleTime time;
};

class TopologyChild : public Supla::ChannelElement {
 public:
  TopologyChild(Identity *identity, int number, uint32_t id)
      : identity(identity), number(number), id(id) {}
  void onLoadTopology(SuplaDeviceClass *) override {
    ++topologyCalls;
    ASSERT_TRUE(getChannel()->restoreChannelNumber(number));
    EXPECT_EQ(getChannel()->getServerChannelId(), 0u);
  }
  void onLoadConfig(SuplaDeviceClass *) override {
    ++configCalls;
    EXPECT_EQ(identity->resolve(id).location, Location::kLocal);
    EXPECT_EQ(identity->resolve(id).channelNumber, number);
  }
  int topologyCalls = 0;
  int configCalls = 0;

 private:
  Identity *identity;
  int number;
  uint32_t id;
};

class TopologyParent : public Supla::Element {
 public:
  explicit TopologyParent(Identity *identity) : identity(identity) {}
  void onLoadTopology(SuplaDeviceClass *) override {
    ++calls;
    first.reset(new TopologyChild(identity, 7, 501));
    second.reset(new TopologyChild(identity, 3, 502));
  }
  std::unique_ptr<TopologyChild> first;
  std::unique_ptr<TopologyChild> second;
  int calls = 0;

 private:
  Identity *identity;
};

class StartupSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit StartupSrpc(SuplaDeviceClass *device) : SuplaSrpc(device, 29) {}
  bool onLoadConfig() override {
    ++configCalls;
    return SuplaSrpc::onLoadConfig();
  }
  void restoreServerIdentity() override {
    EXPECT_EQ(Supla::Channel::Begin()->getChannelNumber(), 7);
    EXPECT_EQ(Supla::Channel::Begin()->next()->getChannelNumber(), 3);
    ++restoreCalls;
    SuplaSrpc::restoreServerIdentity();
  }
  int configCalls = 0;
  int restoreCalls = 0;
};

TEST_F(SupLanLifecycleTests, BeginReconstructsTopologyBeforeIdentityAndConfig) {
  {
    Supla::Channel first(7), second(3);
    accept();
    identity.load(nullptr);
  }
  auto before = config.blob("sl-identity");
  SuplaDeviceClass device;
  testing::NiceMock<NetworkMock> network;
  testing::NiceMock<TimerMock> timer;
  StartupSrpc protocol(&device);
  TopologyParent parent(&protocol.serverIdentity());
  device.begin(29);
  ASSERT_NE(parent.first, nullptr);
  EXPECT_EQ(parent.calls, 1);
  EXPECT_EQ(parent.first->topologyCalls, 1);
  EXPECT_EQ(parent.second->topologyCalls, 1);
  EXPECT_EQ(parent.first->configCalls, 1);
  EXPECT_EQ(parent.second->configCalls, 1);
  EXPECT_EQ(protocol.configCalls, 1);
  EXPECT_EQ(protocol.restoreCalls, 1);
  EXPECT_EQ(config.blob("sl-identity"), before);
  protocol.serverIdentity().load(nullptr);
}

TEST_F(SupLanLifecycleTests, RestoreNumbersDoesNotForgetIdentityRuntimeDoes) {
  Supla::Channel first(7), second(3);
  accept();
  auto before = config.durable;
  identity.load(nullptr);
  ASSERT_TRUE(first.restoreChannelNumber(3));
  EXPECT_EQ(second.getChannelNumber(), 7);
  ASSERT_TRUE(first.restoreChannelNumber(7));
  EXPECT_EQ(second.getChannelNumber(), 3);
  EXPECT_EQ(config.durable, before);
  identity.load(&config);
  EXPECT_EQ(identity.resolve(501).channelNumber, 7);
  ASSERT_TRUE(first.setChannelNumber(5));
  EXPECT_NE(config.durable, before);
  EXPECT_EQ(identity.resolve(501).location, Location::kUnresolved);
  identity.load(nullptr);
}

TEST_F(SupLanLifecycleTests, LegacyHvacRemainsLocalAfterFirstIdentityCrash) {
  OutputSimulator output;
  Supla::Control::HvacBase hvac(&output);
  hvac.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  Supla::ChannelElement thermometer(7);
  thermometer.getChannel()->setType(SUPLA_CHANNELTYPE_THERMOMETER);
  hvac.onLoadConfig(nullptr);
  TChannelConfig_HVAC legacy = {};
  int size;
  hvac.fillChannelConfig(&legacy, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  legacy.MainThermometerChannelNo = 7;
  install("0_hvac_cfg", legacy);
  accept();
  config.cut();
  identity.load(&config);
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 7);
  EXPECT_EQ(identity.resolve(502).channelNumber, 7);
  identity.load(nullptr);
}

TEST_F(SupLanLifecycleTests, LegacyContainerAndValveStayExplicitlyLocal) {
  Supla::Sensor::Container container;
  Supla::Control::ValveBase valve;
  Supla::Sensor::ContainerConfig oldContainer;
  oldContainer.sensorData[0].channelNumber = 7;
  oldContainer.sensorData[0].fillLevel = 80;
  Supla::Control::ValveConfig oldValve;
  oldValve.sensorData[0] = 7;
  install("0_container", oldContainer);
  install("1_valve_cfg", oldValve);
  accept();
  container.onLoadConfig(nullptr);
  valve.onLoadConfig(nullptr);
  EXPECT_EQ(container.getFillLevelForSensor(7), 80);
  TChannelConfig_Valve outgoing = {};
  int size;
  valve.fillChannelConfig(&outgoing, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(outgoing.SensorInfo[0].ChannelNo, 7);
  identity.load(nullptr);
}

TEST_F(SupLanLifecycleTests, IntrinsicRelatedMeterRemainsLocalWithIdentity) {
  Supla::Control::Relay relay(1);
  relay.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  relay.setDefaultRelatedMeterChannelNo(7);
  accept();
  TChannelConfig_PowerSwitch outgoing = {};
  int size;
  relay.fillChannelConfig(&outgoing, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(outgoing.DefaultRelatedMeterIsSet, 1);
  EXPECT_EQ(outgoing.DefaultRelatedMeterChannelNo, 7);
  identity.load(nullptr);
}
}  // namespace
