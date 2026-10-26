// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <output_mock.h>
#include <simple_time.h>
#include <supla/channels/channel.h>
#include <supla/control/hvac_base.h>
#include <supla/control/relay.h>
#include <supla/control/virtual_valve.h>
#include <supla/device/register_device.h>
#include <supla/network/html/channel_function_parameters.h>
#include <supla/network/html/container_parameters.h>
#include <supla/network/html/hvac_parameters.h>
#include <supla/network/web_sender.h>
#include <supla/sensor/container.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/storage/key_value.h>
#include <supla/storage/storage.h>
#include <supla/storage/config_tags.h>
#include <supla/suplan/suplan_server_identity.h>

#include <array>
#include <string>
#include <vector>

namespace {
using Supla::ChannelReference;
using Field = Supla::Control::HvacBase::ReferenceField;

class MemoryConfig : public Supla::KeyValue {
 public:
  bool init() override { return true; }
  bool commit() override {
    std::array<uint8_t, 8192> bytes = {};
    const auto size = serializeToMemory(bytes.data(), bytes.size());
    if (size == SIZE_MAX) return false;
    disk.assign(bytes.begin(), bytes.begin() + size);
    return true;
  }
  void reboot() {
    removeAllMemory();
    ASSERT_FALSE(disk.empty());
    ASSERT_TRUE(initFromMemory(disk.data(), disk.size()));
  }
  std::vector<uint8_t> disk;
};
class Capture : public Supla::WebSender {
 public:
  void send(const char *data, int size = -1) override {
    html.append(data, size < 0 ? std::char_traits<char>::length(data) : size);
  }
  std::string html;
};
class Hvac : public Supla::Control::HvacBase {
 public:
  explicit Hvac(Supla::Control::OutputInterface *out) : HvacBase(out) {}
  void seed(uint32_t base = 12345) {
    TChannelConfig_HVAC wire;
    copyFullChannelConfigTo(&wire);
    wire.MainThermometerChannelId = base + 0;
    wire.AuxThermometerChannelId = base + 1;
    wire.BinarySensorChannelId = base + 2;
    wire.MasterThermostatChannelId = base + 3;
    wire.PumpSwitchChannelId = base + 4;
    wire.HeatOrColdSourceSwitchChannelId = base + 5;
    const auto record = Supla::Control::storeHvacConfig(
        Supla::Control::HvacConfiguration::fromServer(wire),
        getChannel()->getDefaultFunction());
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "hvac_cfg2");
    auto storage = Supla::Storage::ConfigInstance();
    ASSERT_TRUE(storage->setBlob(key, reinterpret_cast<const char *>(&record),
                                 sizeof(record)));
    ASSERT_TRUE(storage->setChannelFunction(
        getChannelNumber(), getChannel()->getDefaultFunction()));
    ASSERT_TRUE(storage->commit());
    onLoadConfig(nullptr);
  }
};
class Container : public Supla::Sensor::Container {
 public:
  void seed() {
    for (int i = 0; i < 2; ++i) {
      config.sensorData[i].source.assign(ChannelReference::server(12345 + i));
      config.sensorData[i].fillLevel = 50 + i;
    }
    saveConfig();
  }
  int level(int slot) const { return config.sensorData[slot].fillLevel; }
};
class Valve : public Supla::Control::VirtualValve {
 public:
  void seed() {
    config.sensorData[0].assign(ChannelReference::server(12345));
    saveConfig();
  }
};
class RemoteReferenceForms : public testing::Test {
 public:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    Supla::RegisterDevice::resetToDefaults();
  }
  void TearDown() override { Supla::Channel::resetToDefaults(); }
  template <typename Element, typename Form>
  void post(Element &element, Form &form, const char *suffix,
            const char *value) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    element.generateKey(key, suffix);
    ASSERT_TRUE(form.handleResponse(key, value));
  }
  void checkAll(const Hvac &hvac) {
    for (int i = 0; i < 6; ++i)
      EXPECT_EQ(hvac.channelReference(static_cast<Field>(i)),
                ChannelReference::server(12345 + i)) << i;
  }
  SimpleTime time;
  MemoryConfig storage;
};

TEST_F(RemoteReferenceForms, HvacRemoteOptionsWithoutLocalCandidatesOrPeers) {
  OutputSimulator output;
  Hvac hvac(&output);
  hvac.onInit();
  hvac.seed();
  Supla::Html::HvacParameters form(&hvac);
  Capture sender;
  form.send(&sender);
  for (int i = 0; i < 6; ++i) {
    EXPECT_NE(sender.html.find("Remote channel #" + std::to_string(12345 + i)),
              std::string::npos);
  }
  EXPECT_NE(sender.html.find("value=\"keep\" selected"), std::string::npos);
  checkAll(hvac);
}

TEST_F(RemoteReferenceForms,
       HvacUnchangedAndUnrelatedEditSurviveStorageReload) {
  OutputSimulator output;
  Hvac hvac(&output);
  hvac.onInit();
  hvac.seed();
  Supla::Html::HvacParameters form(&hvac);
  for (auto key : {"t_main", "t_aux", "sensor", "master", "pump", "hocs"})
    post(hvac, form, key, "keep");
  form.onProcessingEnd();
  checkAll(hvac);
  post(hvac, form, "min_on_s", "37");
  post(hvac, form, "t_hister", "0.7");
  form.onProcessingEnd();
  EXPECT_EQ(hvac.getMinOnTimeS(), 37);
  EXPECT_EQ(hvac.getTemperatureHisteresis(), 70);
  checkAll(hvac);
  storage.reboot();
  hvac.onLoadConfig(nullptr);
  checkAll(hvac);
  EXPECT_EQ(hvac.getMinOnTimeS(), 37);
  EXPECT_EQ(hvac.getTemperatureHisteresis(), 70);
}

TEST_F(RemoteReferenceForms, HvacMissingDisabledHiddenAndForgedFieldsPreserve) {
  OutputSimulator output;
  Hvac hvac(&output);
  hvac.onInit();
  hvac.seed();
  Supla::Sensor::VirtualThermometer local;
  Supla::Html::HvacParameters form(&hvac);
  hvac.parameterFlags.MainThermometerChannelNoReadonly = true;
  hvac.parameterFlags.AuxThermometerChannelNoHidden = true;
  const auto number = std::to_string(local.getChannelNumber());
  post(hvac, form, "t_main", number.c_str());
  post(hvac, form, "t_aux", number.c_str());
  for (auto key : {"sensor", "master", "pump", "hocs"}) {
    for (auto bad : {"12345", "server:12345", "keep:12345", "-2", "256",
                     "1junk", "999999999999999999999999"})
      post(hvac, form, key, bad);
    // A local thermometer is also an invalid binary/HVAC/relay selection.
    post(hvac, form, key, number.c_str());
  }
  form.onProcessingEnd();
  checkAll(hvac);
}

TEST_F(RemoteReferenceForms, HvacExplicitUnsetOnlyChangesSelectedReference) {
  OutputSimulator output;
  Hvac hvac(&output);
  hvac.onInit();
  hvac.seed();
  Supla::Html::HvacParameters form(&hvac);
  post(hvac, form, "t_aux", "0");
  post(hvac, form, "pump", "-1");
  form.onProcessingEnd();
  EXPECT_EQ(hvac.channelReference(Field::AuxThermometer), ChannelReference{});
  EXPECT_EQ(hvac.channelReference(Field::PumpSwitch), ChannelReference{});
  EXPECT_EQ(hvac.channelReference(Field::MainThermometer),
            ChannelReference::server(12345));
  EXPECT_EQ(hvac.channelReference(Field::HeatOrColdSourceSwitch),
            ChannelReference::server(12350));
  storage.reboot();
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(hvac.channelReference(Field::AuxThermometer), ChannelReference{});
}

TEST_F(RemoteReferenceForms, HvacLocalReplacementUsesOwnServerMap) {
  OutputSimulator output;
  Hvac hvac(&output);
  Supla::Sensor::VirtualThermometer local;
  hvac.onInit();
  Supla::Device::ServerIdentity identity;
  identity.load(&storage);
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 77;
  ids.ChannelCount = 2;
  ids.ChannelId[0] = 501;
  ids.ChannelId[1] = 502;
  ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  identity.syncDone();
  hvac.seed();
  Supla::Html::HvacParameters form(&hvac);
  post(hvac, form, "t_main", "1");
  form.onProcessingEnd();
  EXPECT_EQ(hvac.channelReference(Field::MainThermometer),
            ChannelReference::server(502));
  EXPECT_EQ(hvac.channelReference(Field::AuxThermometer),
            ChannelReference::server(12346));
  storage.reboot();
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(hvac.channelReference(Field::MainThermometer),
            ChannelReference::server(502));
}

TEST_F(RemoteReferenceForms, HvacAllSixLocalChoicesUseMappedChannelIds) {
  OutputSimulator output;
  Hvac hvac(&output);
  Supla::Sensor::VirtualThermometer main;
  Supla::Sensor::VirtualThermometer aux;
  Supla::Sensor::VirtualBinary binary;
  OutputSimulator masterOutput;
  Hvac master(&masterOutput);
  Supla::Control::Relay pump(-1);
  Supla::Control::Relay source(-1);
  pump.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_PUMPSWITCH);
  source.getChannel()->setDefaultFunction(
      SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH);
  hvac.onInit();
  master.onInit();
  Supla::Device::ServerIdentity identity;
  identity.load(&storage);
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 77;
  ids.ChannelCount = 7;
  for (int i = 0; i < 7; ++i) ids.ChannelId[i] = 501 + i;
  ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  identity.syncDone();
  hvac.seed();
  Supla::Html::HvacParameters form(&hvac);
  const char *keys[] = {"t_main", "t_aux", "sensor", "master", "pump", "hocs"};
  for (int i = 0; i < 6; ++i) {
    const auto number = std::to_string(i + 1);
    post(hvac, form, keys[i], number.c_str());
  }
  form.onProcessingEnd();
  for (int i = 0; i < 6; ++i)
    EXPECT_EQ(hvac.channelReference(static_cast<Field>(i)),
              ChannelReference::server(502 + i));
  storage.reboot();
  hvac.onLoadConfig(nullptr);
  for (int i = 0; i < 6; ++i)
    EXPECT_EQ(hvac.channelReference(static_cast<Field>(i)),
              ChannelReference::server(502 + i));
}

TEST_F(RemoteReferenceForms, HvacInvalidPostedFunctionKeepsReferencesOnReload) {
  OutputSimulator output;
  Hvac hvac(&output);
  hvac.onInit();
  hvac.seed();
  const auto function = hvac.getChannelFunction();
  Supla::Html::HvacParameters form(&hvac);
  for (const char *value : {"0", "", "invalid", "9999", "4294967296"}) {
    post(hvac, form, "fnc", value);
    form.onProcessingEnd();
    EXPECT_EQ(hvac.getChannelFunction(), function) << value;
    checkAll(hvac);
    storage.reboot();
    hvac.onLoadConfig(nullptr);
    EXPECT_EQ(hvac.getChannelFunction(), function) << value;
    checkAll(hvac);
  }
}

TEST_F(RemoteReferenceForms, HvacLocalConfigRejectsUnsupportedFunction) {
  OutputSimulator output;
  Hvac hvac(&output);
  hvac.onInit();
  hvac.seed();
  TSD_ChannelConfig request = {};
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  EXPECT_EQ(hvac.handleLocalChannelConfig(nullptr, 0),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_EQ(hvac.handleLocalChannelConfig(&request, 0),
            SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED);
  checkAll(hvac);
  request.Func = 9999;
  EXPECT_EQ(hvac.handleLocalChannelConfig(&request, 0),
            SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED);
  checkAll(hvac);
}

TEST_F(RemoteReferenceForms, HvacServerFunctionZeroStillResetsReferences) {
  OutputSimulator output;
  Hvac hvac(&output);
  hvac.onInit();
  Supla::Device::ServerIdentity identity;
  identity.load(&storage);
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 77;
  ids.ChannelCount = 1;
  ids.ChannelId[0] = 501;
  ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  identity.syncDone();
  hvac.seed();
  TSD_ChannelConfig request = {};
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  for (int i = 0; i < 6; ++i)
    EXPECT_EQ(hvac.channelReference(static_cast<Field>(i)), ChannelReference{});
  storage.reboot();
  hvac.onLoadConfig(nullptr);
  for (int i = 0; i < 6; ++i)
    EXPECT_EQ(hvac.channelReference(static_cast<Field>(i)), ChannelReference{});
}

TEST_F(RemoteReferenceForms, HvacFunctionEditPreservesCanonicalAssignments) {
  OutputSimulator output;
  Hvac hvac(&output);
  hvac.enableDomesticHotWaterFunctionSupport();
  hvac.onInit();
  hvac.seed();
  Supla::Html::HvacParameters form(&hvac);
  const auto function =
      std::to_string(SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER);
  post(hvac, form, "fnc", function.c_str());
  form.onProcessingEnd();
  checkAll(hvac);
  EXPECT_EQ(hvac.getChannelFunction(),
            SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER);
  storage.reboot();
  hvac.onLoadConfig(nullptr);
  checkAll(hvac);
}

TEST_F(RemoteReferenceForms, HvacKeepUsesCurrentReferenceNotBrowserSnapshot) {
  OutputSimulator output;
  Hvac hvac(&output);
  hvac.onInit();
  hvac.seed();
  Supla::Html::HvacParameters form(&hvac);
  Capture sender;
  form.send(&sender);
  post(hvac, form, "min_on_s", "37");
  hvac.seed(22345);
  post(hvac, form, "t_main", "keep");
  form.onProcessingEnd();
  for (int i = 0; i < 6; ++i)
    EXPECT_EQ(hvac.channelReference(static_cast<Field>(i)),
              ChannelReference::server(22345 + i));
  EXPECT_EQ(hvac.getMinOnTimeS(), 37);
}

TEST_F(RemoteReferenceForms, ContainerTypedReplacementKeepsLegacyGuard) {
  Container container;
  container.seed();
  EXPECT_FALSE(container.setSensorSlot(0, -1, 0));
  EXPECT_EQ(container.sensorReference(0), ChannelReference::server(12345));
  EXPECT_FALSE(container.setSensorSlotReference(
      10, ChannelReference::local(1), 50));
  EXPECT_FALSE(container.setSensorSlotReference(
      0, ChannelReference::local(255), 50));
  EXPECT_EQ(container.sensorReference(0), ChannelReference::server(12345));
  EXPECT_TRUE(container.setSensorSlotReference(0, ChannelReference{}, 0));
  EXPECT_EQ(container.sensorReference(0), ChannelReference{});
}

TEST_F(RemoteReferenceForms, ContainerShowsMultipleRemotesWithoutLocalSensors) {
  Container container;
  container.seed();
  Supla::Html::ContainerParameters form(true, &container);
  Capture sender;
  form.send(&sender);
  EXPECT_NE(sender.html.find("Remote channel #12345"), std::string::npos);
  EXPECT_NE(sender.html.find("Remote channel #12346"), std::string::npos);
  EXPECT_NE(sender.html.find("value !== 'keep'"), std::string::npos);
  post(container, form, "sns_id_0", "keep");
  post(container, form, "sns_id_1", "keep");
  form.onProcessingEnd();
  EXPECT_EQ(container.sensorReference(0), ChannelReference::server(12345));
  EXPECT_EQ(container.sensorReference(1), ChannelReference::server(12346));
}

TEST_F(RemoteReferenceForms,
       ContainerLevelAndOtherEditsKeepRemoteAcrossReload) {
  Container container;
  container.seed();
  Supla::Html::ContainerParameters form(true, &container);
  post(container, form, "sns_lvl_0", "75");
  post(container, form, "wrn_above", "90");
  form.onProcessingEnd();
  EXPECT_EQ(container.sensorReference(0), ChannelReference::server(12345));
  EXPECT_EQ(container.level(0), 75);
  storage.reboot();
  container.onLoadConfig(nullptr);
  EXPECT_EQ(container.sensorReference(0), ChannelReference::server(12345));
  EXPECT_EQ(container.sensorReference(1), ChannelReference::server(12346));
  EXPECT_EQ(container.level(0), 75);
}

TEST_F(RemoteReferenceForms, ContainerRejectsForgedAndWrongTypeLocalIds) {
  Container container;
  container.seed();
  Supla::Sensor::VirtualThermometer thermometer;
  Supla::Html::ContainerParameters form(true, &container);
  for (auto bad : {"12345", "server:12345", "keep:12345", "-1", "255",
                   "1junk", "999999999999999999999999", "1"})
    post(container, form, "sns_id_0", bad);
  form.onProcessingEnd();
  EXPECT_EQ(container.sensorReference(0), ChannelReference::server(12345));
}

TEST_F(RemoteReferenceForms, ContainerExplicitLocalReplacementAndUnset) {
  Container container;
  container.seed();
  Supla::Sensor::VirtualBinary sensor;
  Supla::Html::ContainerParameters form(true, &container);
  post(container, form, "sns_id_0", "1");
  post(container, form, "sns_id_1", "");
  form.onProcessingEnd();
  EXPECT_EQ(container.sensorReference(0), ChannelReference::local(1));
  EXPECT_EQ(container.level(0), 50);
  EXPECT_EQ(container.sensorReference(1), ChannelReference{});
  storage.reboot();
  container.onLoadConfig(nullptr);
  EXPECT_EQ(container.sensorReference(0), ChannelReference::local(1));
  EXPECT_EQ(container.sensorReference(1), ChannelReference{});
}

TEST_F(RemoteReferenceForms, ContainerDisabledSensorsIgnorePostedEdits) {
  Container container;
  container.seed();
  Supla::Html::ContainerParameters form(false, &container);
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  container.generateKey(key, "sns_id_0");
  EXPECT_FALSE(form.handleResponse(key, ""));
  post(container, form, "wrn_above", "90");
  form.onProcessingEnd();
  EXPECT_EQ(container.sensorReference(0), ChannelReference::server(12345));
}

TEST_F(RemoteReferenceForms, ValveGeneralFunctionFormDoesNotProjectSensorIds) {
  Valve valve;
  valve.seed();
  Supla::Html::ChannelFunctionParameters form(&valve);
  const auto function =
      std::to_string(valve.getChannel()->getDefaultFunction());
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(key, valve.getChannelNumber(),
                            Supla::ConfigTag::ChannelFunctionTag);
  ASSERT_TRUE(form.handleResponse(key, function.c_str()));
  form.onProcessingEnd();
  EXPECT_EQ(valve.sensorReference(0), ChannelReference::server(12345));
  storage.reboot();
  valve.onLoadConfig(nullptr);
  EXPECT_EQ(valve.sensorReference(0), ChannelReference::server(12345));
}
}  // namespace
