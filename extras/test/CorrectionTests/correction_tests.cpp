// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_simulator.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/channels/channel.h>
#include <supla/correction.h>
#include <supla/sensor/virtual_therm_hygro_meter.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla_srpc_layer_mock.h>

#include <array>
#include <cstdio>

#include "supla/sensor/therm_hygro_meter.h"

using Supla::Correction;

namespace {

class PersistenceConfigSimulator : public ConfigSimulator {
 public:
  void saveWithDelay(uint16_t delayMs) override {
    ++saveWithDelayCalls;
    lastSaveDelayMs = delayMs;
    Supla::Config::saveWithDelay(delayMs);
  }

  bool commit() override {
    ++commitCalls;
    snapshotSize = serializeToMemory(snapshot.data(), snapshot.size());
    return snapshotSize != SIZE_MAX;
  }

  void resetPersistenceCounters() {
    saveWithDelayCalls = 0;
    commitCalls = 0;
    lastSaveDelayMs = 0;
  }

  std::array<uint8_t, 512> snapshot = {};
  size_t snapshotSize = 0;
  int saveWithDelayCalls = 0;
  int commitCalls = 0;
  uint16_t lastSaveDelayMs = 0;
};

class InspectableVirtualThermHygroMeter
    : public Supla::Sensor::VirtualThermHygroMeter {
 public:
  bool isDefaultConfigPending() const {
    return isLocalChannelConfigChangePending(SUPLA_CONFIG_TYPE_DEFAULT);
  }

  Supla::ChannelConfigState getConfigState() const {
    return channelConfigState;
  }
};

void fillRemoteThermHygroConfig(TSD_ChannelConfig *config, int channelNumber) {
  *config = {};
  config->ChannelNumber = channelNumber;
  config->Func = SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE;
  config->ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config->ConfigSize = sizeof(TChannelConfig_TemperatureAndHumidity);

  auto payload =
      reinterpret_cast<TChannelConfig_TemperatureAndHumidity *>(config->Config);
  payload->TemperatureAdjustment = 120;
  payload->HumidityAdjustment = -230;
  payload->AdjustmentAppliedByDevice = 1;
  payload->MinTemperatureAdjustment = 0;
  payload->MaxTemperatureAdjustment = 0;
  payload->MinHumidityAdjustment = 0;
  payload->MaxHumidityAdjustment = 0;
}

void getCorrectionKeys(int channelNumber,
                       char temperatureKey[SUPLA_CONFIG_MAX_KEY_SIZE],
                       char humidityKey[SUPLA_CONFIG_MAX_KEY_SIZE]) {
  snprintf(
      temperatureKey, SUPLA_CONFIG_MAX_KEY_SIZE, "corr_%d_0", channelNumber);
  snprintf(humidityKey, SUPLA_CONFIG_MAX_KEY_SIZE, "corr_%d_1", channelNumber);
}

}  // namespace

TEST(CorrectionTests, CorrectionGetCheck) {
  Correction::add(5, 2.5);

  EXPECT_EQ(Correction::get(5), 2.5);
  EXPECT_EQ(Correction::get(5, true), 0);
  EXPECT_EQ(Correction::get(1), 0);

  Correction::add(1, 3.14);
  EXPECT_EQ(Correction::get(1), 3.14);
  EXPECT_EQ(Correction::get(5), 2.5);

  Correction::clear();

  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 0);
}

TEST(CorrectionTests, CorrectionGetCheckForSecondary) {
  Correction::add(5, 2.5, true);

  EXPECT_EQ(Correction::get(5), 0);
  EXPECT_EQ(Correction::get(5, true), 2.5);
  EXPECT_EQ(Correction::get(1), 0);

  Correction::add(1, 3.14);
  EXPECT_EQ(Correction::get(1), 3.14);
  EXPECT_EQ(Correction::get(5, true), 2.5);

  Correction::clear();

  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 0);
}

TEST(CorrectionTests, CorrectionChangeTest) {
  Correction::add(5, 2.5);

  EXPECT_EQ(Correction::get(5), 2.5);
  EXPECT_EQ(Correction::get(5, true), 0);
  EXPECT_EQ(Correction::get(1), 0);

  Correction::add(5, 3.14);
  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 3.14);

  Correction::clear();

  EXPECT_EQ(Correction::get(1), 0);
  EXPECT_EQ(Correction::get(5), 0);
}

TEST(CorrectionTests, ThermometerCorrectionTest) {
  ConfigSimulator config;
  SimpleTime time;
  Supla::Sensor::VirtualThermometer temp;

  temp.onLoadConfig(nullptr);
  temp.setValue(23.0);

  time.advance(20000);
  temp.iterateAlways();

  EXPECT_EQ(temp.getChannel()->getValueDouble(), 23.0);

  temp.applyCorrectionsAndStoreIt(-50, 0);

  EXPECT_EQ(temp.getChannel()->getValueDouble(), 23.0);

  time.advance(20000);
  temp.iterateAlways();
  EXPECT_EQ(temp.getChannel()->getValueDouble(), 18.0);

  temp.applyCorrectionsAndStoreIt(0, 50);
  time.advance(20000);
  temp.iterateAlways();
  EXPECT_EQ(temp.getChannel()->getValueDouble(), 23.0);

  temp.applyCorrectionsAndStoreIt(-30, 50);
  time.advance(20000);
  temp.iterateAlways();
  EXPECT_EQ(temp.getChannel()->getValueDouble(), 20.0);

  Supla::Correction::clear();  // cleanup
}

TEST(CorrectionTests, ThermHygroMeterCorrectionTest) {
  ConfigSimulator config;
  SimpleTime time;
  Supla::Sensor::VirtualThermHygroMeter th;

  th.onLoadConfig(nullptr);
  th.setTemp(23.0);
  th.setHumi(55.0);

  time.advance(20000);
  th.iterateAlways();

  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 23.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 55.0);

  th.applyCorrectionsAndStoreIt(-50, 0);

  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 23.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 55.0);

  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 18.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 55.0);

  th.applyCorrectionsAndStoreIt(0, 50);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 23.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 60.0);

  th.applyCorrectionsAndStoreIt(-30, 50);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 60.0);

  th.setHumi(100.0);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 100.0);

  th.setHumi(98.0);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 100.0);

  th.applyCorrectionsAndStoreIt(-30, -100);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 88.0);

  th.setHumi(5.0);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), 20.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 0.0);

  th.setTemp(-271.0);
  time.advance(20000);
  th.iterateAlways();
  EXPECT_EQ(th.getChannel()->getValueDoubleFirst(), -273.0);
  EXPECT_EQ(th.getChannel()->getValueDoubleSecond(), 0.0);

  Supla::Correction::clear();  // cleanup
}

TEST(CorrectionTests, RemoteThermHygroConfigSchedulesAndSurvivesPersistence) {
  Supla::Channel::resetToDefaults();
  Supla::Correction::clear();

  std::array<uint8_t, 512> committedSnapshot = {};
  size_t committedSnapshotSize = 0;
  {
    PersistenceConfigSimulator config;
    SimpleTime time;
    InspectableVirtualThermHygroMeter th;

    char temperatureKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    char humidityKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    getCorrectionKeys(th.getChannelNumber(), temperatureKey, humidityKey);
    ASSERT_TRUE(config.setInt32(temperatureKey, -5));
    ASSERT_TRUE(config.setInt32(humidityKey, 6));
    config.commit();
    config.resetPersistenceCounters();

    th.onLoadConfig(nullptr);
    TSD_ChannelConfig remoteConfig = {};
    fillRemoteThermHygroConfig(&remoteConfig, th.getChannelNumber());

    EXPECT_EQ(th.handleChannelConfig(&remoteConfig, false),
              SUPLA_CONFIG_RESULT_TRUE);

    int32_t temperatureCorrection = 0;
    int32_t humidityCorrection = 0;
    ASSERT_TRUE(config.getInt32(temperatureKey, &temperatureCorrection));
    ASSERT_TRUE(config.getInt32(humidityKey, &humidityCorrection));
    EXPECT_EQ(temperatureCorrection, 12);
    EXPECT_EQ(humidityCorrection, -23);

    EXPECT_EQ(config.saveWithDelayCalls, 1);
    EXPECT_EQ(config.lastSaveDelayMs, 5000);
    EXPECT_FALSE(th.isDefaultConfigPending());
    EXPECT_EQ(th.getConfigState(), Supla::ChannelConfigState::None);

    char changedTypesKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(changedTypesKey,
                               th.getChannelNumber(),
                               Supla::ConfigTag::ChannelConfigChangedTypesTag);
    uint32_t changedTypes = UINT32_MAX;
    ASSERT_TRUE(config.getUInt32(changedTypesKey, &changedTypes));
    EXPECT_EQ(changedTypes, 0);

    time.advance(5001);
    config.saveIfNeeded();
    ASSERT_EQ(config.commitCalls, 1);
    committedSnapshot = config.snapshot;
    committedSnapshotSize = config.snapshotSize;
  }

  ASSERT_GT(committedSnapshotSize, 0);
  {
    ConfigSimulator restartedConfig;
    ASSERT_TRUE(restartedConfig.initFromMemory(committedSnapshot.data(),
                                               committedSnapshotSize));

    int32_t temperatureCorrection = 0;
    int32_t humidityCorrection = 0;
    EXPECT_TRUE(restartedConfig.getInt32("corr_0_0", &temperatureCorrection));
    EXPECT_TRUE(restartedConfig.getInt32("corr_0_1", &humidityCorrection));
    EXPECT_EQ(temperatureCorrection, 12);
    EXPECT_EQ(humidityCorrection, -23);
  }

  Supla::Correction::clear();
  Supla::Channel::resetToDefaults();
}

TEST(CorrectionTests,
     RemoteCorrectionApplicationClearsPersistedLocalDefaultFlag) {
  Supla::Channel::resetToDefaults();
  Supla::Correction::clear();

  {
    PersistenceConfigSimulator config;
    SimpleTime time;
    InspectableVirtualThermHygroMeter th;

    char changedTypesKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(changedTypesKey,
                               th.getChannelNumber(),
                               Supla::ConfigTag::ChannelConfigChangedTypesTag);
    ASSERT_TRUE(config.setUInt32(changedTypesKey, 1));
    th.onLoadConfig(nullptr);
    ASSERT_TRUE(th.isDefaultConfigPending());
    ASSERT_EQ(th.getConfigState(),
              Supla::ChannelConfigState::LocalChangePending);

    TSD_ChannelConfig remoteConfig = {};
    fillRemoteThermHygroConfig(&remoteConfig, th.getChannelNumber());
    // The common handler guards a pending local change. Call the application
    // layer directly here to verify that an accepted server value clears and
    // persists the DEFAULT bit when that cleanup path is reached.
    EXPECT_EQ(th.applyChannelConfig(&remoteConfig, false),
              Supla::ApplyConfigResult::Success);

    EXPECT_FALSE(th.isDefaultConfigPending());
    EXPECT_EQ(th.getConfigState(), Supla::ChannelConfigState::None);
    uint32_t changedTypes = UINT32_MAX;
    ASSERT_TRUE(config.getUInt32(changedTypesKey, &changedTypes));
    EXPECT_EQ(changedTypes, 0);

    th.onLoadConfig(nullptr);
    EXPECT_FALSE(th.isDefaultConfigPending());
    EXPECT_EQ(th.getConfigState(), Supla::ChannelConfigState::None);
    EXPECT_EQ(config.saveWithDelayCalls, 1);
  }

  Supla::Correction::clear();
  Supla::Channel::resetToDefaults();
}

TEST(CorrectionTests, LocalThermHygroCorrectionsRemainPendingForServerSync) {
  using ::testing::_;
  using ::testing::Return;

  Supla::Channel::resetToDefaults();
  Supla::Correction::clear();

  {
    PersistenceConfigSimulator config;
    SimpleTime time;
    SuplaSrpcLayerMock srpc;
    InspectableVirtualThermHygroMeter th;

    th.applyCorrectionsAndStoreIt(12, -23, true);

    EXPECT_TRUE(th.isDefaultConfigPending());
    EXPECT_EQ(th.getConfigState(),
              Supla::ChannelConfigState::LocalChangePending);
    EXPECT_EQ(config.saveWithDelayCalls, 1);
    EXPECT_EQ(config.lastSaveDelayMs, 5000);

    th.onRegistered(&srpc);
    th.handleChannelConfigFinished();
    EXPECT_CALL(srpc,
                setChannelConfig(th.getChannelNumber(),
                                 SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE,
                                 _,
                                 sizeof(TChannelConfig_TemperatureAndHumidity),
                                 SUPLA_CONFIG_TYPE_DEFAULT))
        .WillOnce(Return(true));

    EXPECT_FALSE(th.iterateConnected());
    EXPECT_EQ(th.getConfigState(), Supla::ChannelConfigState::LocalChangeSent);
    EXPECT_TRUE(th.isDefaultConfigPending());
  }

  Supla::Correction::clear();
  Supla::Channel::resetToDefaults();
}
