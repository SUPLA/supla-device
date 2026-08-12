// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <config_mock.h>
#include <arduino_mock.h>

#include <supla/channel.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/sensor/multi_ds_handler_base.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>

#include <array>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

class TestMultiDsHandler : public Supla::Sensor::MultiDsHandlerBase {
 public:
  explicit TestMultiDsHandler(SuplaDeviceClass *sdc = nullptr)
      : MultiDsHandlerBase(sdc, 0) {}

  Supla::Sensor::MultiDsSensor *add(uint8_t addressByte,
                                    int channelNumber = -1,
                                    int subDeviceId = -1) {
    uint8_t address[8] = {0x28, addressByte, 2, 3, 4, 5, 6, addressByte};
    return addDevice(address, channelNumber, subDeviceId);
  }

  Supla::Sensor::MultiDsSensor *slot(int index) const {
    return index >= 0 && index < MULTI_DS_MAX_DEVICES_COUNT ? sensors[index]
                                                             : nullptr;
  }

  void removeSlot(int index) {
    if (index >= 0 && index < MULTI_DS_MAX_DEVICES_COUNT &&
        sensors[index] != nullptr) {
      delete sensors[index];
      sensors[index] = nullptr;
    }
  }

  void setDiscoveredSensor(uint8_t addressByte) {
    discoveredAddress = {0x28, addressByte, 2, 3, 4, 5, 6, addressByte};
    sensorDiscovered = true;
  }

 protected:
  int refreshSensorsCount() override { return sensorDiscovered ? 1 : 0; }
  void requestTemperatures() override {}
  bool getSensorAddress(uint8_t *address, int index) override {
    if (!sensorDiscovered || address == nullptr || index != 0) {
      return false;
    }
    memcpy(address, discoveredAddress.data(), discoveredAddress.size());
    return true;
  }
  double getTemperature(const uint8_t *) override { return 20.0; }

 private:
  std::array<uint8_t, 8> discoveredAddress = {};
  bool sensorDiscovered = false;
};

class PairingObserver : public Supla::Device::SubdevicePairingObserver {
 public:
  void onSubdevicePairingStarted(uint16_t maximumDurationSec) override {
    started = true;
    this->maximumDurationSec = maximumDurationSec;
  }

  void onSubdevicePairingFinished(
      const TCalCfg_SubdevicePairingResult &result) override {
    finished = true;
    pairingResult = result;
  }

  bool started = false;
  bool finished = false;
  uint16_t maximumDurationSec = 0;
  TCalCfg_SubdevicePairingResult pairingResult = {};
};

class ConflictObserver : public Supla::Device::ChannelConflictObserver {
 public:
  void onChannelConflictResolution(bool handled) override {
    calls++;
    lastHandled = handled;
  }

  int calls = 0;
  bool lastHandled = false;
};

class CapturingSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  CapturingSrpc() : Supla::Protocol::SuplaSrpc(nullptr) {}

  void sendSubdeviceDetails(TDS_SubdeviceDetails *details) override {
    if (details != nullptr) {
      lastDetails = *details;
      detailsSent = true;
    }
  }

  TDS_SubdeviceDetails lastDetails = {};
  bool detailsSent = false;
};

class MultiDsHandlerTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Storage::SetConfigInstance(nullptr);
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Storage::SetConfigInstance(nullptr);
    Supla::Channel::resetToDefaults();
  }
};

std::string configKey(int subDeviceId) {
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(key, subDeviceId,
                             Supla::ConfigTag::DsSensorConfig);
  return key;
}

Supla::Sensor::DsSensorConfig makeConfig(uint8_t channelNumber,
                                          uint8_t addressByte) {
  Supla::Sensor::DsSensorConfig config = {};
  config.channelNumber = channelNumber;
  config.address[0] = 0x28;
  config.address[1] = addressByte;
  config.address[2] = 2;
  config.address[3] = 3;
  config.address[4] = 4;
  config.address[5] = 5;
  config.address[6] = 6;
  config.address[7] = addressByte;
  return config;
}

TEST_F(MultiDsHandlerTests, NewSensorsUseGlobalSubdeviceIdsNotSlots) {
  std::array<Supla::Channel, 9> occupiedChannels;
  const uint8_t occupiedIds[] = {1, 2, 4, 5, 6, 7, 9, 10, 11};
  for (size_t i = 0; i < sizeof(occupiedIds); i++) {
    occupiedChannels[i].setSubDeviceId(occupiedIds[i]);
  }

  TestMultiDsHandler handler;
  auto first = handler.add(1);
  auto second = handler.add(8);
  auto third = handler.add(12);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(third, nullptr);
  EXPECT_EQ(handler.slot(0), first);
  EXPECT_EQ(handler.slot(1), second);
  EXPECT_EQ(handler.slot(2), third);
  EXPECT_EQ(first->getSubDeviceId(), 3);
  EXPECT_EQ(second->getSubDeviceId(), 8);
  EXPECT_EQ(third->getSubDeviceId(), 12);
}

TEST_F(MultiDsHandlerTests, RestoreScansIdsAboveMaxDeviceCount) {
  NiceMock<ConfigMock> config;
  std::map<std::string, Supla::Sensor::DsSensorConfig> blobs = {
      {configKey(3), makeConfig(4, 1)},
      {configKey(8), makeConfig(8, 2)},
      {configKey(12), makeConfig(9, 3)},
  };
  ON_CALL(config, init()).WillByDefault(Return(true));
  ON_CALL(config, getBlob(_, _, _)).WillByDefault(Invoke(
      [&blobs](const char *key, char *value, size_t size) {
        auto it = blobs.find(key);
        if (it == blobs.end() || size != sizeof(it->second)) {
          return false;
        }
        memcpy(value, &it->second, sizeof(it->second));
        return true;
      }));
  Supla::Storage::SetConfigInstance(&config);

  TestMultiDsHandler handler;
  handler.setMaxDeviceCount(5);
  handler.onLoadConfig(nullptr);

  ASSERT_NE(handler.slot(0), nullptr);
  ASSERT_NE(handler.slot(1), nullptr);
  ASSERT_NE(handler.slot(2), nullptr);
  EXPECT_EQ(handler.slot(0)->getSubDeviceId(), 3);
  EXPECT_EQ(handler.slot(1)->getSubDeviceId(), 8);
  EXPECT_EQ(handler.slot(2)->getSubDeviceId(), 12);
  EXPECT_EQ(handler.slot(0)->getChannel()->getChannelNumber(), 4);
  EXPECT_EQ(handler.slot(1)->getChannel()->getChannelNumber(), 8);
  EXPECT_EQ(handler.slot(2)->getChannel()->getChannelNumber(), 9);
  EXPECT_EQ(memcmp(handler.slot(0)->getAddress(),
                   blobs[configKey(3)].address, 8),
            0);
  EXPECT_EQ(memcmp(handler.slot(1)->getAddress(),
                   blobs[configKey(8)].address, 8),
            0);
  EXPECT_EQ(memcmp(handler.slot(2)->getAddress(),
                   blobs[configKey(12)].address, 8),
            0);
}

TEST_F(MultiDsHandlerTests, RemovingSensorDoesNotChangeOtherIds) {
  Supla::Channel occupied;
  occupied.setSubDeviceId(1);

  TestMultiDsHandler handler;
  auto first = handler.add(1);
  auto second = handler.add(2);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_EQ(first->getSubDeviceId(), 2);
  ASSERT_EQ(second->getSubDeviceId(), 3);

  handler.removeSlot(0);
  auto replacement = handler.add(3);

  ASSERT_NE(replacement, nullptr);
  EXPECT_EQ(replacement->getSubDeviceId(), 2);
  EXPECT_EQ(handler.slot(1), second);
  EXPECT_EQ(second->getSubDeviceId(), 3);
}

TEST_F(MultiDsHandlerTests, OffsetUsesRuntimeSlotAndNotSubdeviceId) {
  TestMultiDsHandler handler;
  handler.setChannelNumberOffset(20);

  auto first = handler.add(1, -1, 17);
  auto second = handler.add(2, -1, 23);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->getChannel()->getChannelNumber(), 20);
  EXPECT_EQ(second->getChannel()->getChannelNumber(), 21);
}

TEST_F(MultiDsHandlerTests, PurgeUsesActualSubdeviceIdInConfigKey) {
  NiceMock<ConfigMock> config;
  std::map<std::string, std::vector<uint8_t>> blobs;
  ON_CALL(config, init()).WillByDefault(Return(true));
  ON_CALL(config, setBlob(_, _, _)).WillByDefault(Invoke(
      [&blobs](const char *key, const char *value, size_t size) {
        blobs[key] = std::vector<uint8_t>(value, value + size);
        return true;
      }));
  ON_CALL(config, eraseKey(_)).WillByDefault(Invoke(
      [&blobs](const char *key) { return blobs.erase(key) != 0; }));
  Supla::Storage::SetConfigInstance(&config);

  Supla::Channel first;
  first.setSubDeviceId(1);
  Supla::Channel second;
  second.setSubDeviceId(2);

  TestMultiDsHandler handler;
  auto sensor = handler.add(1);
  ASSERT_NE(sensor, nullptr);
  ASSERT_NE(blobs.find(configKey(3)), blobs.end());

  sensor->purgeConfig();
  EXPECT_EQ(blobs.find(configKey(3)), blobs.end());
}

TEST_F(MultiDsHandlerTests, IterateConnectedSendsActualSubdeviceId) {
  Supla::Channel first;
  first.setSubDeviceId(1);
  Supla::Channel second;
  second.setSubDeviceId(2);

  TestMultiDsHandler handler;
  auto sensor = handler.add(1, 4);
  ASSERT_NE(sensor, nullptr);
  ASSERT_EQ(sensor->getSubDeviceId(), 3);

  CapturingSrpc srpc;
  handler.onRegistered(&srpc);

  EXPECT_FALSE(handler.iterateConnected());
  ASSERT_TRUE(srpc.detailsSent);
  EXPECT_EQ(srpc.lastDetails.SubDeviceId, sensor->getSubDeviceId());
  EXPECT_EQ(srpc.lastDetails.SubDeviceId, 3);
}

TEST_F(MultiDsHandlerTests, DisabledSubdevicesKeepProtocolIdZero) {
  TestMultiDsHandler handler;
  handler.setUseSubDevices(false);

  auto first = handler.add(1);
  auto second = handler.add(2);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->getSubDeviceId(), 1);
  EXPECT_EQ(second->getSubDeviceId(), 2);
  EXPECT_EQ(first->getChannel()->getSubDeviceId(), 0);
  EXPECT_EQ(second->getChannel()->getSubDeviceId(), 0);
}

TEST_F(MultiDsHandlerTests, MaxDeviceCountIsClampedAndZeroIsSafe) {
  TestMultiDsHandler handler;
  handler.setMaxDeviceCount(0);
  EXPECT_EQ(handler.add(1), nullptr);

  handler.setMaxDeviceCount(MULTI_DS_MAX_DEVICES_COUNT + 1);
  EXPECT_EQ(handler.add(2), nullptr);
}

TEST_F(MultiDsHandlerTests, PairingObserverReceivesStartAndFinishedResult) {
  NiceMock<TimeInterfaceMock> time;
  ON_CALL(time, millis()).WillByDefault(Return(100));
  TestMultiDsHandler handler;
  PairingObserver observer;
  handler.setMaxDeviceCount(0);
  handler.setDiscoveredSensor(1);
  handler.setPairingObserver(&observer);

  TCalCfg_SubdevicePairingResult startResult = {};
  EXPECT_TRUE(handler.startPairing(nullptr, &startResult));
  EXPECT_TRUE(observer.started);
  EXPECT_EQ(observer.maximumDurationSec, 5);
  EXPECT_EQ(startResult.PairingResult,
            SUPLA_CALCFG_PAIRINGRESULT_PROCEDURE_STARTED);

  handler.iterateAlways();

  EXPECT_TRUE(observer.finished);
  EXPECT_EQ(observer.pairingResult.PairingResult,
            SUPLA_CALCFG_PAIRINGRESULT_RESOURCES_LIMIT_EXCEEDED);
  EXPECT_EQ(observer.pairingResult.MaximumDurationSec, 5);
}

TEST_F(MultiDsHandlerTests, ConflictObserverReportsUnresolvedConflict) {
  TestMultiDsHandler handler;
  ConflictObserver observer;
  handler.setChannelConflictObserver(&observer);

  EXPECT_FALSE(handler.onChannelConflictReport(
      nullptr, 0, true, false, false));
  EXPECT_EQ(observer.calls, 1);
  EXPECT_FALSE(observer.lastHandled);
}

TEST_F(MultiDsHandlerTests, ConflictObserverReportsRemovedSensor) {
  NiceMock<ConfigMock> config;
  ON_CALL(config, init()).WillByDefault(Return(true));
  ON_CALL(config, setBlob(_, _, _)).WillByDefault(Return(true));
  ON_CALL(config, eraseKey(_)).WillByDefault(Return(true));
  Supla::Storage::SetConfigInstance(&config);

  SuplaDeviceClass sdc;
  TestMultiDsHandler handler(&sdc);
  ConflictObserver observer;
  handler.setChannelConflictObserver(&observer);
  ASSERT_NE(handler.add(1, 4, 1), nullptr);

  uint8_t report[5] = {};
  EXPECT_TRUE(handler.onChannelConflictReport(
      report, sizeof(report), false, true, false));
  EXPECT_EQ(handler.slot(0), nullptr);
  EXPECT_EQ(observer.calls, 1);
  EXPECT_TRUE(observer.lastHandled);
}

}  // namespace
