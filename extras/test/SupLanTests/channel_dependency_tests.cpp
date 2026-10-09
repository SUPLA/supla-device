// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <SuplaDevice.h>
#include <output_mock.h>
#include <srpc_mock.h>
#include <simple_time.h>
#include <supla/channels/binary_sensor_channel.h>
#include <supla/channels/channel_reference.h>
#include <supla/channels/channel_state.h>
#include <supla/control/hvac_base.h>
#include <supla/control/hvac_config.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/sensor/general_purpose_measurement.h>
#include <supla/sensor/general_purpose_meter.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/storage/key_value.h>
#include <supla/storage/storage.h>
#include <supla/suplan/remote_resource_manager.h>
#include <supla/suplan/suplan_server_identity.h>

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace {
using namespace Supla;          // NOLINT(build/namespaces)
using namespace Supla::Device;  // NOLINT(build/namespaces)
using namespace Supla::SupLan;  // NOLINT(build/namespaces)

class Access : public RemoteAccessPort {
 public:
  bool ensure(const TDS_SuplaEnsureResourceAccess &request) override {
    last = request;
    ++requests;
    return true;
  }
  TDS_SuplaEnsureResourceAccess last = {};
  int requests = 0;
  void cancelEnsure() override { ++cancellations; }
  int cancellations = 0;
};

TEST(ChannelDependency, ExplicitTagsNeverInferIdentityFromNumbers) {
  Channel::resetToDefaults();
  Channel local(7);
  EXPECT_NE(ChannelReference::local(7), ChannelReference::server(7));
  EXPECT_EQ(resolveChannelReference(ChannelReference::local(7)).channel,
            &local);
  EXPECT_EQ(resolveChannelReference(ChannelReference::server(7)).kind,
            ChannelResolutionKind::kUnresolved);
  EXPECT_EQ(resolveChannelReference(ChannelReference::local(8)).kind,
            ChannelResolutionKind::kUnresolved);
}

TEST(ChannelDependency, CommonStateDecodesTemperatureHumidityAndAvailability) {
  Channel::resetToDefaults();
  Channel channel(7);
  channel.setType(SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE);
  channel.setNewValue(-12.125, 45.0);
  TDS_SuplaDeviceChannel_E snapshot;
  ASSERT_TRUE(buildChannelSnapshot(&channel, &snapshot));
  ChannelState remote(&snapshot, true);
  EXPECT_TRUE(remote.available());
  EXPECT_DOUBLE_EQ(remote.temperature(), -12.125);
  snapshot.Offline = SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE;
  EXPECT_FALSE(remote.available());
  EXPECT_EQ(remote.temperature(), TEMPERATURE_NOT_AVAILABLE);
  channel.setStateOnlineAndNotAvailable();
  EXPECT_FALSE(ChannelState(&channel).available());
}

TEST(ChannelDependency, AllSixReferencesRoundTripThroughV2) {
  TChannelConfig_HVAC wire = {};
  wire.MainThermometerChannelId = 1001;
  wire.AuxThermometerChannelId = 1002;
  wire.BinarySensorChannelId = 1003;
  wire.MasterThermostatChannelId = 1004;
  wire.PumpSwitchChannelId = 1005;
  wire.HeatOrColdSourceSwitchChannelId = 1006;
  wire.MinOnTimeS = 37;
  const auto config = Control::HvacConfiguration::fromServer(wire);
  const auto record = Control::storeHvacConfig(config);
  Control::HvacConfiguration restored;
  ASSERT_TRUE(Control::restoreHvacConfig(record, &restored));
  TChannelConfig_HVAC encoded;
  ASSERT_TRUE(restored.serverWire(nullptr, &encoded));
  EXPECT_EQ(encoded.MainThermometerChannelId, 1001);
  EXPECT_EQ(encoded.AuxThermometerChannelId, 1002);
  EXPECT_EQ(encoded.BinarySensorChannelId, 1003);
  EXPECT_EQ(encoded.MasterThermostatChannelId, 1004);
  EXPECT_EQ(encoded.PumpSwitchChannelId, 1005);
  EXPECT_EQ(encoded.HeatOrColdSourceSwitchChannelId, 1006);
  EXPECT_EQ(encoded.MinOnTimeS, 37);
  EXPECT_EQ(record.referenceNamespace,
            Control::HvacReferenceNamespace::SERVER_CHANNEL_ID);
}

TEST(ChannelDependency, LegacyReferencesRemainLocalWithoutIdentity) {
  TChannelConfig_HVAC wire = {};
  wire.MainThermometerChannelNo = 7;
  wire.AuxThermometerChannelNo = 8;
  wire.BinarySensorChannelNo = 9;
  wire.MasterThermostatChannelNo = 10;
  wire.PumpSwitchChannelNo = 11;
  wire.HeatOrColdSourceSwitchChannelNo = 12;
  const auto record =
      Control::storeHvacConfig(Control::HvacConfiguration::fromLegacy(wire));
  for (const auto &reference : record.references) {
    EXPECT_EQ(reference.local.reserved[0], 0);
    EXPECT_EQ(reference.local.reserved[1], 0);
  }
  Control::HvacConfiguration restored;
  ASSERT_TRUE(Control::restoreHvacConfig(record, &restored));
  EXPECT_EQ(restored.MainThermometer.local.channelNo, 7u);
  auto bad = record;
  bad.version = 3;
  EXPECT_FALSE(Control::restoreHvacConfig(bad, &restored));
  bad = record;
  bad.referenceNamespace =
      static_cast<Control::HvacReferenceNamespace>(99);
  EXPECT_FALSE(Control::restoreHvacConfig(bad, &restored));
}

TEST(ChannelDependency, DisabledLegacyReferencesEncodeAsUnsetServerIds) {
  Control::HvacConfiguration config;
  TChannelConfig_HVAC wire;
  ASSERT_TRUE(config.serverWire(nullptr, &wire));
  EXPECT_EQ(wire.MainThermometerChannelId, 0);
  EXPECT_EQ(wire.AuxThermometerChannelId, 0);
  EXPECT_EQ(wire.BinarySensorChannelId, 0);
  EXPECT_EQ(wire.MasterThermostatChannelId, 0);
  EXPECT_EQ(wire.PumpSwitchChannelId, 0);
  EXPECT_EQ(wire.HeatOrColdSourceSwitchChannelId, 0);
  ASSERT_TRUE(config.setLocalReference(&config.PumpSwitch, 0));
  EXPECT_FALSE(config.serverWire(nullptr, &wire));
}

TEST(ChannelDependency, SharedDemandIsSeparateFromGrantedAccess) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  const ResourceId id = {kResourceTypeChannel, 501};
  ASSERT_TRUE(manager.consume({1, id, kPermissionRead}));
  ASSERT_TRUE(manager.consume({2, id, kPermissionRead}));
  EXPECT_EQ(manager.consumerCount(), 2);
  EXPECT_EQ(manager.resourceCount(), 1);
  manager.iterate(0);
  ASSERT_EQ(port.requests, 1);
  EXPECT_EQ(port.last.DeliveryMode, SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER);
  EXPECT_EQ(port.last.Flags, 0);
  EXPECT_EQ(port.last.Permissions, kPermissionRead);
  manager.ensureResult({SUPLA_SUPLAN_RESULT_OK,
                        SUPLA_SUPLAN_ACCESS_STATUS_GRANTED,
                        SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER});
  EXPECT_EQ(manager.access(id), RemoteAccess::PENDING);
  EXPECT_FALSE(manager.state(id, 0).available());
  manager.remove(1);
  EXPECT_EQ(manager.resourceCount(), 1);
  manager.remove(2);
  EXPECT_EQ(manager.resourceCount(), 0);
}

TEST(ChannelDependency, DeniedDemandIsBlockedWithoutEnsureLoop) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  const ResourceId id = {kResourceTypeChannel, 501};
  ASSERT_TRUE(manager.consume({1, id, kPermissionRead}));
  manager.iterate(0);
  manager.ensureResult({SUPLA_SUPLAN_RESULT_NOT_AUTHORIZED, 0,
                        SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER});
  EXPECT_EQ(manager.access(id), RemoteAccess::BLOCKED);
  manager.iterate(100000);
  EXPECT_EQ(port.requests, 1);
  manager.serverReconnected();
  manager.iterate(100001);
  EXPECT_EQ(port.requests, 2);
}

TEST(ChannelDependency, RemovingPendingEnsureAllowsNewDependencyImmediately) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  ASSERT_TRUE(manager.consume(
      {1, {kResourceTypeChannel, 501}, kPermissionRead}));
  manager.iterate(0);
  ASSERT_EQ(port.requests, 1);
  manager.remove(1);
  EXPECT_EQ(port.cancellations, 1);
  // A reply delivered after cancellation has no resource to affect.
  manager.ensureResult({SUPLA_SUPLAN_RESULT_NOT_AUTHORIZED, 0,
                        SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER});
  ASSERT_TRUE(manager.consume(
      {1, {kResourceTypeChannel, 502}, kPermissionRead}));
  manager.iterate(1);
  EXPECT_EQ(port.requests, 2);
  EXPECT_EQ(port.last.Resource.ResourceId, 502);
  EXPECT_EQ(manager.access({kResourceTypeChannel, 502}), RemoteAccess::PENDING);
}

TEST(ChannelDependency, PendingEnsureSurvivesWhileAnotherConsumerRemains) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  const ResourceId id = {kResourceTypeChannel, 501};
  ASSERT_TRUE(manager.consume({1, id, kPermissionRead}));
  ASSERT_TRUE(manager.consume({2, id, kPermissionRead}));
  manager.iterate(0);
  manager.remove(1);
  manager.iterate(1);
  EXPECT_EQ(port.cancellations, 0);
  EXPECT_EQ(port.requests, 1);
  manager.ensureResult({SUPLA_SUPLAN_RESULT_OK,
                        SUPLA_SUPLAN_ACCESS_STATUS_GRANTED,
                        SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER});
  EXPECT_EQ(manager.consumerCount(), 1);
  EXPECT_EQ(manager.access(id), RemoteAccess::PENDING);
}

TEST(ChannelDependency, EnsureTimeoutDoesNotStarveAnotherConsumer) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  ASSERT_TRUE(manager.consume(
      {1, {kResourceTypeChannel, 501}, kPermissionRead}));
  manager.iterate(0);
  ASSERT_EQ(port.last.Resource.ResourceId, 501);
  ASSERT_TRUE(manager.consume(
      {2, {kResourceTypeChannel, 502}, kPermissionRead}));
  manager.iterate(9999);
  EXPECT_EQ(port.requests, 1);
  manager.iterate(10000);
  EXPECT_EQ(port.cancellations, 1);
  EXPECT_EQ(port.requests, 1);
  manager.iterate(11000);
  EXPECT_EQ(port.requests, 2);
  EXPECT_EQ(port.last.Resource.ResourceId, 502);
  manager.ensureResult({SUPLA_SUPLAN_RESULT_OK,
                        SUPLA_SUPLAN_ACCESS_STATUS_GRANTED,
                        SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER});
  manager.iterate(11001);
  EXPECT_EQ(port.requests, 3);
  EXPECT_EQ(port.last.Resource.ResourceId, 501);
}

TEST(ChannelDependency, EnsureTimeoutRetriesAcrossClockWrap) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  ASSERT_TRUE(manager.consume(
      {1, {kResourceTypeChannel, 501}, kPermissionRead}));
  manager.iterate(UINT32_MAX - 4999);
  manager.iterate(4999);
  EXPECT_EQ(port.cancellations, 0);
  manager.iterate(5000);
  EXPECT_EQ(port.cancellations, 1);
  manager.iterate(5999);
  EXPECT_EQ(port.requests, 1);
  manager.iterate(6000);
  EXPECT_EQ(port.requests, 2);
}

TEST(ChannelDependency, AuthorizationLossOverridesTtlButEndpointLossDoesNot) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  const ResourceId id = {kResourceTypeChannel, 501};
  PeerContext context = {kAuthorityServer,     0, {kNodeIdDevice, 101},
                         {kNodeIdDevice, 202}, 7, 1};
  uint8_t key[32] = {1};
  AclEntry acl = {id, kPermissionRead};
  uint8_t index;
  ASSERT_TRUE(peers.addPeer(&context, key, 1, &acl, 1, &index, true));
  ASSERT_TRUE(manager.consume({1, id, kPermissionRead}));
  EXPECT_EQ(manager.access(id), RemoteAccess::READY);
  manager.iterate(0);
  EXPECT_EQ(port.requests, 0);
  TDS_SuplaDeviceChannel_E snapshot = {};
  snapshot.Number = 0xff;
  snapshot.Type = SUPLA_CHANNELTYPE_THERMOMETER;
  snapshot.Default = SUPLA_CHANNELFNC_THERMOMETER;
  snapshot.ValueValidityTimeSec = 60;
  double temperature = 21.75;
  std::memcpy(snapshot.value, &temperature, 8);
  manager.receive(index, id, reinterpret_cast<uint8_t *>(&snapshot),
                  sizeof(snapshot), 0);
  EXPECT_DOUBLE_EQ(manager.state(id, 1000).temperature(), temperature);
  peers.get(index)->endpoint = {};
  peers.get(index)->endpointState = kPeerEndpointNone;
  EXPECT_TRUE(manager.state(id, 59000).available());
  EXPECT_FALSE(manager.state(id, 60000).available());
  ASSERT_TRUE(peers.replaceAcl(index, 2, nullptr, 0));
  EXPECT_FALSE(manager.state(id, 1000).available());
  EXPECT_EQ(manager.access(id), RemoteAccess::PENDING);
}

class DurableHvacConfig : public KeyValue {
 public:
  using KeyValue::getBlobSize;
  bool init() override { return true; }
  bool commit() override {
    events.push_back("commit");
    ++commits;
    if (failCommit) return false;
    std::array<uint8_t, 8192> bytes = {};
    const auto size = serializeToMemory(bytes.data(), bytes.size());
    if (size == SIZE_MAX) return false;
    disk.assign(bytes.begin(), bytes.begin() + size);
    return true;
  }
  bool eraseKey(const char *key) override {
    events.push_back(std::string("erase:") + key);
    if (failErase) return false;
    return KeyValue::eraseKey(key);
  }
  bool setBlob(const char *key, const char *data, size_t size) override {
    events.push_back(std::string("write:") + key);
    return KeyValue::setBlob(key, data, size);
  }
  void reboot() {
    removeAllMemory();
    if (!disk.empty()) {
      ASSERT_TRUE(initFromMemory(disk.data(), disk.size()));
    }
  }
  bool failCommit = false;
  bool failErase = false;
  int commits = 0;
  std::vector<uint8_t> disk;
  std::vector<std::string> events;
};

void resetChannels() {
  Channel::resetToDefaults();
  RegisterDevice::resetToDefaults();
}
void bootstrapIdentity(ServerIdentity *identity, DurableHvacConfig *config) {
  identity->load(config);
  ASSERT_TRUE(identity->registrationStarted());
  identity->registrationSucceeded();
  TSD_SuplaDeviceIdentities identities = {};
  identities.DeviceId = 202;
  identities.ChannelCount = 2;
  identities.ChannelId[0] = 601;
  identities.ChannelId[1] = 602;
  ASSERT_EQ(identity->accept(identities).Result, SUPLA_SUPLAN_RESULT_OK);
  identity->syncDone();
}

class DependencySrpc : public Protocol::SuplaSrpc {
 public:
  explicit DependencySrpc(SuplaDeviceClass *device) : SuplaSrpc(device, 29) {}
  void prepare(void *handle) {
    srpc = handle;
    registered = 1;
  }
  ~DependencySrpc() { srpc = nullptr; }
};

class ReferenceHvac : public Control::HvacBase {
 public:
  explicit ReferenceHvac(Control::OutputInterface *output) : HvacBase(output) {}
  Control::HvacConfiguration configuration() const {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "hvac_cfg2");
    Control::HvacStoredConfigV2 record;
    Control::HvacConfiguration result;
    EXPECT_TRUE(Storage::ConfigInstance()->getBlob(key,
        reinterpret_cast<char *>(&record), sizeof(record)));
    EXPECT_TRUE(Control::restoreHvacConfig(record, &result));
    return result;
  }
};

TEST(ChannelDependency, NeverRegisteredChannelZeroPersistsAndRestartsLocal) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  {
    Sensor::VirtualThermometer thermometer;
    OutputSimulator output;
    ReferenceHvac hvac(&output);
    hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
    hvac.onInit();
    ASSERT_TRUE(hvac.setMainThermometerChannelNo(0));
    thermometer.getChannel()->setNewValue(21.5);
    EXPECT_EQ(hvac.getPrimaryTemp(), 2150);
    hvac.saveConfig();
    Control::HvacStoredConfigV2 record;
    ASSERT_TRUE(storage.getBlob("1_hvac_cfg2",
        reinterpret_cast<char *>(&record), sizeof(record)));
    EXPECT_EQ(record.referenceNamespace,
              Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER);
    EXPECT_EQ(record.references[0].local.channelNo, 0);
    EXPECT_EQ(record.references[0].local.isSet, 1);
  }
  storage.reboot();
  resetChannels();
  Sensor::VirtualThermometer thermometer;
  OutputSimulator output;
  ReferenceHvac hvac(&output);
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onLoadConfig(nullptr);
  hvac.onInit();
  thermometer.getChannel()->setNewValue(19.25);
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 0);
  EXPECT_EQ(hvac.getPrimaryTemp(), 1925);
}

TEST(ChannelDependency, LocalBootstrapMaterializesIdsWithoutRewritingV2) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer thermometer;
  OutputSimulator output;
  ReferenceHvac hvac(&output);
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onInit();
  ASSERT_TRUE(hvac.setMainThermometerChannelNo(0));
  hvac.saveConfig();
  Control::HvacStoredConfigV2 before, after;
  ASSERT_TRUE(storage.getBlob("1_hvac_cfg2",
      reinterpret_cast<char *>(&before), sizeof(before)));
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  const auto events = storage.events.size();
  TChannelConfig_HVAC wire;
  int size = 0;
  hvac.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  ASSERT_EQ(size, sizeof(wire));
  EXPECT_EQ(wire.MainThermometerChannelId, 601);
  EXPECT_EQ(wire.AuxThermometerChannelId, 0);
  EXPECT_EQ(storage.events.size(), events);
  ASSERT_TRUE(storage.getBlob("1_hvac_cfg2",
      reinterpret_cast<char *>(&after), sizeof(after)));
  EXPECT_EQ(memcmp(&before, &after, sizeof(before)), 0);
  storage.reboot();
  identity.load(&storage);
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(hvac.configuration().referenceNamespace,
            Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER);
  thermometer.getChannel()->setNewValue(20.75);
  EXPECT_EQ(hvac.getPrimaryTemp(), 2075);
}

TEST(ChannelDependency, MissingIdentityDefersWireButKeepsLocalValue) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer thermometer;
  OutputSimulator output;
  ReferenceHvac hvac(&output);
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onInit();
  ASSERT_TRUE(hvac.setMainThermometerChannelNo(0));
  ServerIdentity identity;
  identity.load(&storage);
  thermometer.getChannel()->setNewValue(23.125);
  EXPECT_EQ(hvac.getPrimaryTemp(), 2312);
  TChannelConfig_HVAC wire;
  memset(&wire, 0xff, sizeof(wire));
  int size = 123;
  hvac.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(size, 0);
  EXPECT_EQ(wire.MainThermometerChannelId, 0);
  EXPECT_EQ(hvac.configuration().MainThermometer.local.isSet, 1);
  EXPECT_EQ(hvac.configuration().referenceNamespace,
            Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER);
}

TEST(ChannelDependency, AllSixLocalValuesMaterializeWithoutChangingNamespace) {
  resetChannels();
  DurableHvacConfig storage;
  Channel first(0), second(1);
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  Control::HvacConfiguration config;
  Control::HvacReferenceValue *values[] = {&config.MainThermometer,
      &config.AuxThermometer, &config.BinarySensor, &config.MasterThermostat,
      &config.PumpSwitch, &config.HeatOrColdSourceSwitch};
  for (auto value : values) ASSERT_TRUE(config.setLocalReference(value, 0));
  const auto before = Control::storeHvacConfig(config);
  TChannelConfig_HVAC wire;
  ASSERT_TRUE(config.serverWire(&identity, &wire));
  EXPECT_EQ(wire.MainThermometerChannelId, 601);
  EXPECT_EQ(wire.AuxThermometerChannelId, 601);
  EXPECT_EQ(wire.BinarySensorChannelId, 601);
  EXPECT_EQ(wire.MasterThermostatChannelId, 601);
  EXPECT_EQ(wire.PumpSwitchChannelId, 601);
  EXPECT_EQ(wire.HeatOrColdSourceSwitchChannelId, 601);
  const auto after = Control::storeHvacConfig(config);
  EXPECT_EQ(memcmp(&before, &after, sizeof(before)), 0);
  ASSERT_TRUE(config.setLocalReference(&config.MainThermometer, 2));
  EXPECT_FALSE(config.serverWire(&identity, &wire));
  EXPECT_EQ(wire.PumpSwitchChannelId, 0);
}

TEST(ChannelDependency, ServerMixedDependenciesAndSettersKeepWholeNamespace) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer thermometer;
  OutputSimulator output;
  ReferenceHvac hvac(&output);
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onInit();
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  TChannelConfig_HVAC wire;
  hvac.copyFullChannelConfigTo(&wire);
  wire.MainThermometerChannelId = 1001;
  wire.AuxThermometerChannelId = 601;
  wire.AuxThermometerType = SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED;
  wire.BinarySensorChannelId = 0;
  wire.MasterThermostatChannelId = 0;
  wire.PumpSwitchChannelId = 0;
  wire.HeatOrColdSourceSwitchChannelId = 0;
  TSD_ChannelConfig request = {};
  request.Func = hvac.getChannel()->getDefaultFunction();
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  request.ConfigSize = sizeof(wire);
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  Control::HvacStoredConfigV2 record;
  ASSERT_TRUE(storage.getBlob("1_hvac_cfg2",
      reinterpret_cast<char *>(&record), sizeof(record)));
  EXPECT_EQ(record.referenceNamespace,
            Control::HvacReferenceNamespace::SERVER_CHANNEL_ID);
  EXPECT_EQ(record.references[0].channelId, 1001u);
  EXPECT_EQ(record.references[1].channelId, 601u);
  const auto &config = hvac.configuration();
  EXPECT_EQ(resolveChannelReference(config.reference(config.MainThermometer),
                                   &identity).kind,
            ChannelResolutionKind::kRemote);
  EXPECT_EQ(hvac.getAuxThermometerChannelNo(), 0);
  EXPECT_EQ(resolveChannelReference(config.reference(config.AuxThermometer),
                                   &identity).channel,
            thermometer.getChannel());
  storage.reboot();
  identity.load(&storage);
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(hvac.configuration().MainThermometer.channelId, 1001u);
  EXPECT_EQ(hvac.getAuxThermometerChannelNo(), 0);
  ASSERT_TRUE(hvac.setAuxThermometerChannelNo(-1));
  ASSERT_TRUE(hvac.setMainThermometerChannelNo(0));
  EXPECT_EQ(hvac.configuration().MainThermometer.channelId, 601u);
  EXPECT_EQ(hvac.configuration().referenceNamespace,
            Control::HvacReferenceNamespace::SERVER_CHANNEL_ID);
  identity.factoryReset();
  const auto before = Control::storeHvacConfig(hvac.configuration());
  const int commits = storage.commits;
  EXPECT_FALSE(hvac.setMainThermometerChannelNo(0));
  EXPECT_FALSE(hvac.setAuxThermometerChannelNo(0));
  const auto after = Control::storeHvacConfig(hvac.configuration());
  EXPECT_EQ(memcmp(&before, &after, sizeof(before)), 0);
  EXPECT_EQ(storage.commits, commits);
}

TEST(ChannelDependency, LegacySentinelsNormalizeWhileChannelZeroStaysSet) {
  TChannelConfig_HVAC wire = {};
  wire.MainThermometerChannelNo = 0;
  wire.AuxThermometerChannelNo = 1;  // HVAC self, historical unset.
  wire.BinarySensorChannelNo = 255;
  wire.MasterThermostatChannelNo = 0;
  wire.MasterThermostatIsSet = 1;
  wire.PumpSwitchChannelNo = 0;
  wire.PumpSwitchIsSet = 0;
  wire.HeatOrColdSourceSwitchChannelNo = 0;
  wire.HeatOrColdSourceSwitchIsSet = 1;
  const auto config = Control::HvacConfiguration::fromLegacy(wire, 1);
  const auto record = Control::storeHvacConfig(config);
  const int expectedSet[] = {1, 0, 0, 1, 0, 1};
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(record.references[i].local.isSet, expectedSet[i]);
    EXPECT_EQ(record.references[i].local.channelNo, 0);
    EXPECT_EQ(record.references[i].local.reserved[0], 0);
    EXPECT_EQ(record.references[i].local.reserved[1], 0);
  }
  EXPECT_EQ(config.reference(config.MainThermometer),
            ChannelReference::local(0));
  EXPECT_EQ(config.reference(config.AuxThermometer), ChannelReference());
}

TEST(ChannelDependency, LegacyChannelZeroMigrationConfirmsBeforeCleanup) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer thermometer;
  OutputSimulator output;
  ReferenceHvac hvac(&output);
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onInit();
  TChannelConfig_HVAC legacy;
  hvac.copyFullChannelConfigTo(&legacy);
  legacy.MainThermometerChannelNo = 0;
  legacy.AuxThermometerChannelNo = 1;
  legacy.BinarySensorChannelNo = 1;
  legacy.PumpSwitchChannelNo = 0;
  legacy.PumpSwitchIsSet = 0;
  ASSERT_TRUE(storage.setBlob("1_hvac_cfg",
      reinterpret_cast<const char *>(&legacy), sizeof(legacy)));
  ASSERT_TRUE(storage.commit());
  storage.events.clear();
  storage.failCommit = true;
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(storage.getBlobSize("1_hvac_cfg2"), -1);
  EXPECT_EQ(storage.getBlobSize("1_hvac_cfg"), sizeof(legacy));
  for (const auto &event : storage.events)
    EXPECT_NE(event, "erase:1_hvac_cfg");
  storage.reboot();
  storage.events.clear();
  storage.failCommit = false;
  hvac.onLoadConfig(nullptr);
  ASSERT_GE(storage.events.size(), 4u);
  EXPECT_EQ(storage.events[0], "write:1_hvac_cfg2");
  EXPECT_EQ(storage.events[1], "commit");
  EXPECT_EQ(storage.events[2], "erase:1_hvac_cfg");
  EXPECT_EQ(storage.events[3], "commit");
  storage.reboot();
  hvac.onLoadConfig(nullptr);
  const auto config = hvac.configuration();
  EXPECT_EQ(config.referenceNamespace,
            Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER);
  EXPECT_EQ(config.MainThermometer.local.channelNo, 0);
  EXPECT_EQ(config.MainThermometer.local.isSet, 1);
  EXPECT_EQ(config.AuxThermometer.local.isSet, 0);
  EXPECT_EQ(config.BinarySensor.local.isSet, 0);
  EXPECT_EQ(config.PumpSwitch.local.isSet, 0);
  thermometer.getChannel()->setNewValue(18.5);
  EXPECT_EQ(hvac.getPrimaryTemp(), 1850);
}

TEST(ChannelDependency, CorruptLocalValuesAndNamespaceAreRejected) {
  static_assert(sizeof(Control::HvacReferenceValue) == 4, "4-byte reference");
  auto record = Control::storeHvacConfig(Control::HvacConfiguration());
  Control::HvacConfiguration restored;
  auto bad = record;
  bad.references[0].local.isSet = 2;
  EXPECT_FALSE(Control::restoreHvacConfig(bad, &restored));
  bad = record;
  bad.references[0].local.reserved[0] = 1;
  EXPECT_FALSE(Control::restoreHvacConfig(bad, &restored));
  bad = record;
  bad.references[0].local.channelNo = 1;
  EXPECT_FALSE(Control::restoreHvacConfig(bad, &restored));
  bad = record;
  bad.referenceNamespace = static_cast<Control::HvacReferenceNamespace>(0);
  EXPECT_FALSE(Control::restoreHvacConfig(bad, &restored));
  record.references[0].local.isSet = 1;
  ASSERT_TRUE(Control::restoreHvacConfig(record, &restored));
  EXPECT_EQ(restored.reference(restored.MainThermometer),
            ChannelReference::local(0));
  record.referenceNamespace =
      Control::HvacReferenceNamespace::SERVER_CHANNEL_ID;
  ASSERT_TRUE(Control::restoreHvacConfig(record, &restored));
  EXPECT_EQ(restored.reference(restored.MainThermometer),
            ChannelReference::server(record.references[0].channelId));
}

TEST(ChannelDependency, EnsureEnqueueFailureAndReplyCorrelation) {
  resetChannels();
  Channel hvac(0);
  Channel thermometer(1);
  DurableHvacConfig storage;
  testing::NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  DependencySrpc srpc(&device);
  int handle = 0;
  srpc.prepare(&handle);
  bootstrapIdentity(&srpc.serverIdentity(), &storage);
  TDS_SuplaEnsureResourceAccess request = {};
  request.Resource = {SUPLA_SUPLAN_RESOURCE_TYPE_CHANNEL, 501};
  request.Permissions = SUPLA_SUPLAN_PERMISSION_READ;
  request.DeliveryMode = SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER;
  EXPECT_CALL(wire, ensureResourceAccess(&handle, testing::_))
      .WillOnce(testing::Return(-1))
      .WillOnce(testing::Return(91));
  EXPECT_FALSE(srpc.ensureResourceAccess(request));
  EXPECT_FALSE(srpc.acceptEnsureResult(UINT32_MAX));
  EXPECT_TRUE(srpc.ensureResourceAccess(request));
  EXPECT_FALSE(srpc.acceptEnsureResult(90));
  EXPECT_TRUE(srpc.acceptEnsureResult(91));
  EXPECT_FALSE(srpc.acceptEnsureResult(91));
  EXPECT_CALL(wire, ensureResourceAccess(&handle, testing::_))
      .WillOnce(testing::Return(92))
      .WillOnce(testing::Return(93));
  ASSERT_TRUE(srpc.ensureResourceAccess(request));
  srpc.cancelEnsureResourceAccess();
  EXPECT_FALSE(srpc.acceptEnsureResult(92));
  ASSERT_TRUE(srpc.ensureResourceAccess(request));
  EXPECT_FALSE(srpc.acceptEnsureResult(92));
  EXPECT_TRUE(srpc.acceptEnsureResult(93));
  request.Flags = 1;
  EXPECT_FALSE(srpc.ensureResourceAccess(request));
}

TEST(ChannelDependency, FailedDurableConfigCannotReleaseIdentityBarrier) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  OutputSimulator output;
  Control::HvacBase hvac(&output);
  Sensor::VirtualThermometer thermometer;
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onInit();
  SuplaDeviceClass device;
  DependencySrpc srpc(&device);
  int handle = 0;
  srpc.prepare(&handle);
  bootstrapIdentity(&srpc.serverIdentity(), &storage);
  auto &identity = srpc.serverIdentity();
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities identities = {};
  identities.DeviceId = 202;
  identities.ChannelCount = 2;
  identities.ChannelId[0] = 603;
  identities.ChannelId[1] = 602;
  ASSERT_EQ(identity.accept(identities).Result, SUPLA_SUPLAN_RESULT_OK);
  ASSERT_TRUE(identity.identityTransition());
  hvac.onServerIdentityTransition();
  TChannelConfig_HVAC config;
  hvac.copyFullChannelConfigTo(&config);
  config.MainThermometerChannelId = 501;
  config.AuxThermometerChannelId = 0;
  config.BinarySensorChannelId = 0;
  config.MasterThermostatChannelId = 0;
  config.PumpSwitchChannelId = 0;
  config.HeatOrColdSourceSwitchChannelId = 0;
  TSD_ChannelConfig request = {};
  request.Func = hvac.getChannel()->getDefaultFunction();
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  request.ConfigSize = sizeof(config);
  std::memcpy(request.Config, &config, sizeof(config));
  storage.failCommit = true;
  EXPECT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  srpc.onDeviceSyncDone();
  EXPECT_TRUE(identity.identityTransition());
  EXPECT_FALSE(identity.serverSyncComplete());
  storage.failCommit = false;
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  srpc.onDeviceSyncDone();
  EXPECT_FALSE(identity.identityTransition());
  EXPECT_TRUE(identity.serverSyncComplete());
}

TEST(ChannelDependency, ReadonlyLocalSyncUsesIdsWithoutReleasingBarrier) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  OutputSimulator output;
  ReferenceHvac hvac(&output);
  Sensor::VirtualThermometer thermometer;
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  ASSERT_TRUE(hvac.setMainThermometerChannelNo(1));
  hvac.parameterFlags.MainThermometerChannelNoReadonly = true;
  hvac.onInit();
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 202;
  ids.ChannelCount = 2;
  ids.ChannelId[0] = 603;
  ids.ChannelId[1] = 604;
  ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  ASSERT_TRUE(identity.identityTransition());
  TChannelConfig_HVAC wire;
  hvac.copyFullChannelConfigTo(&wire);
  wire.MainThermometerChannelId = 1001;
  wire.AuxThermometerChannelId = 0;
  wire.BinarySensorChannelId = 0;
  wire.MasterThermostatChannelId = 0;
  wire.PumpSwitchChannelId = 0;
  wire.HeatOrColdSourceSwitchChannelId = 0;
  TSD_ChannelConfig request = {};
  request.Func = hvac.getChannel()->getDefaultFunction();
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  request.ConfigSize = sizeof(wire);
  memcpy(request.Config, &wire, sizeof(wire));
  storage.failCommit = true;
  EXPECT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_TRUE(identity.identityTransition());
  storage.failCommit = false;
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac.configuration().MainThermometer.channelId, 604u);
  EXPECT_EQ(hvac.configuration().referenceNamespace,
            Control::HvacReferenceNamespace::SERVER_CHANNEL_ID);
  thermometer.getChannel()->setNewValue(20.5);
  EXPECT_EQ(hvac.getPrimaryTemp(), INT16_MIN);
  EXPECT_FALSE(hvac.setMainThermometerChannelNo(1));
  EXPECT_TRUE(identity.identityTransition());
  identity.syncDone();
  EXPECT_FALSE(identity.identityTransition());
  EXPECT_EQ(hvac.getPrimaryTemp(), 2050);
}

TEST(ChannelDependency, AcceptedMapAndTransitionBarrierResolveExplicitly) {
  resetChannels();
  DurableHvacConfig config;
  Channel local(0), thermometer(1);
  ServerIdentity identity;
  bootstrapIdentity(&identity, &config);
  EXPECT_EQ(
      resolveChannelReference(ChannelReference::server(602), &identity).channel,
      &thermometer);
  EXPECT_EQ(
      resolveChannelReference(ChannelReference::server(501), &identity).kind,
      ChannelResolutionKind::kRemote);
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities identities = {};
  identities.DeviceId = 202;
  identities.ChannelCount = 2;
  identities.ChannelId[0] = 603;
  identities.ChannelId[1] = 602;
  ASSERT_EQ(identity.accept(identities).Result, SUPLA_SUPLAN_RESULT_OK);
  ASSERT_TRUE(identity.identityTransition());
  EXPECT_EQ(
      resolveChannelReference(ChannelReference::server(501), &identity).kind,
      ChannelResolutionKind::kUnresolved);
  EXPECT_EQ(
      resolveChannelReference(ChannelReference::server(602), &identity).kind,
      ChannelResolutionKind::kUnresolved);
  EXPECT_EQ(
      resolveChannelReference(ChannelReference::local(1), &identity).channel,
      &thermometer);
}

TEST(ChannelDependency, LegacyMigrationCommitsV2BeforeErasingAndRestarts) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  OutputSimulator output;
  Control::HvacBase hvac(&output);
  Sensor::VirtualThermometer thermometer;
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onInit();
  TChannelConfig_HVAC legacy;
  hvac.copyFullChannelConfigTo(&legacy);
  legacy.MainThermometerChannelNo = 1;
  ASSERT_TRUE(storage.setBlob(
      "0_hvac_cfg", reinterpret_cast<const char *>(&legacy), sizeof(legacy)));
  ASSERT_TRUE(storage.commit());
  storage.events.clear();
  hvac.onLoadConfig(nullptr);
  ASSERT_EQ(storage.getBlobSize("0_hvac_cfg2"),
            sizeof(Control::HvacStoredConfigV2));
  ASSERT_EQ(storage.getBlobSize("0_hvac_cfg"), -1);
  ASSERT_GE(storage.events.size(), 4u);
  EXPECT_EQ(storage.events[0], "write:0_hvac_cfg2");
  EXPECT_EQ(storage.events[1], "commit");
  EXPECT_EQ(storage.events[2], "erase:0_hvac_cfg");
  EXPECT_EQ(storage.events[3], "commit");
  storage.reboot();
  Control::HvacStoredConfigV2 record;
  ASSERT_TRUE(storage.getBlob("0_hvac_cfg2", reinterpret_cast<char *>(&record),
                              sizeof(record)));
  EXPECT_EQ(record.references[0].local.channelNo, 1u);
  EXPECT_EQ(record.referenceNamespace,
            Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER);
  EXPECT_EQ(record.references[0].local.isSet, 1);
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 1);
}

TEST(ChannelDependency, FailedMigrationKeepsLegacyAndV2WinsAfterCrash) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  OutputSimulator output;
  Control::HvacBase hvac(&output);
  Sensor::VirtualThermometer thermometer;
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onInit();
  TChannelConfig_HVAC legacy;
  hvac.copyFullChannelConfigTo(&legacy);
  legacy.MainThermometerChannelNo = 1;
  ASSERT_TRUE(storage.setBlob(
      "0_hvac_cfg", reinterpret_cast<const char *>(&legacy), sizeof(legacy)));
  ASSERT_TRUE(storage.commit());
  storage.failCommit = true;
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(storage.getBlobSize("0_hvac_cfg"), sizeof(legacy));
  EXPECT_EQ(storage.getBlobSize("0_hvac_cfg2"), -1);
  storage.reboot();
  storage.failCommit = false;
  storage.failErase = true;
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(storage.getBlobSize("0_hvac_cfg2"),
            sizeof(Control::HvacStoredConfigV2));
  EXPECT_EQ(storage.getBlobSize("0_hvac_cfg"), sizeof(legacy));
  // Crash-equivalent: a stale legacy value cannot regain authority.
  legacy.MainThermometerChannelNo = 0;
  ASSERT_TRUE(storage.setBlob(
      "0_hvac_cfg", reinterpret_cast<const char *>(&legacy), sizeof(legacy)));
  ASSERT_TRUE(storage.commit());
  storage.reboot();
  storage.failErase = false;
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 1);
  EXPECT_EQ(storage.getBlobSize("0_hvac_cfg"), -1);
}

TEST(ChannelDependency, V2AndLegacyRestoreHonorFirmwareReadonlyScalars) {
  for (bool v2 : {false, true}) {
    SCOPED_TRACE(v2);
    resetChannels();
    SimpleTime time;
    DurableHvacConfig storage;
    OutputSimulator output;
    Control::HvacBase hvac(&output);
    hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
    hvac.onInit();
    TChannelConfig_HVAC wire;
    hvac.copyFullChannelConfigTo(&wire);
    wire.MinOnTimeS = 10;
    if (v2) {
      const auto record = Control::storeHvacConfig(
          Control::HvacConfiguration::fromLegacy(wire),
          SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
      ASSERT_TRUE(storage.setBlob("0_hvac_cfg2",
          reinterpret_cast<const char *>(&record), sizeof(record)));
    } else {
      ASSERT_TRUE(storage.setBlob("0_hvac_cfg",
          reinterpret_cast<const char *>(&wire), sizeof(wire)));
    }
    ASSERT_TRUE(storage.commit());
    storage.reboot();
    hvac.parameterFlags.MinOnTimeSReadonly = true;
    hvac.onLoadConfig(nullptr);
    EXPECT_EQ(hvac.getMinOnTimeS(), 0);
  }
}

class FirmwareValidatedHvac : public Control::HvacBase {
 public:
  explicit FirmwareValidatedHvac(Control::OutputInterface *output)
      : HvacBase(output) {}
  bool correctMain = false;
 protected:
  bool applyAdditionalValidation(TChannelConfig_HVAC *wire) override {
    wire->MinOffTimeS = 47;
    if (correctMain) wire->MainThermometerChannelNo = 1;
    return true;
  }
};

TEST(ChannelDependency, V2FirmwareValidationPreservesUnchangedServerTags) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  OutputSimulator output;
  FirmwareValidatedHvac hvac(&output);
  Sensor::VirtualThermometer thermometer;
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.setMainThermometerChannelNo(1);
  hvac.onInit();
  TChannelConfig_HVAC wire;
  hvac.copyFullChannelConfigTo(&wire);
  wire.MainThermometerChannelId = 1001;
  wire.AuxThermometerChannelId = 1002;
  wire.BinarySensorChannelId = 1003;
  wire.MasterThermostatChannelId = 1004;
  wire.PumpSwitchChannelId = 1005;
  wire.HeatOrColdSourceSwitchChannelId = 1006;
  const auto record = Control::storeHvacConfig(
      Control::HvacConfiguration::fromServer(wire),
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  ASSERT_TRUE(storage.setBlob("0_hvac_cfg2",
      reinterpret_cast<const char *>(&record), sizeof(record)));
  ASSERT_TRUE(storage.commit());
  storage.reboot();
  hvac.onLoadConfig(nullptr);
  EXPECT_EQ(hvac.getMinOffTimeS(), 47);
  int size = 0;
  hvac.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  ASSERT_EQ(size, sizeof(wire));
  const auto restored = Control::storeHvacConfig(
      Control::HvacConfiguration::fromServer(wire));
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(restored.referenceNamespace, record.referenceNamespace);
    EXPECT_EQ(restored.references[i].channelId, record.references[i].channelId);
  }
  EXPECT_TRUE(wire.MasterThermostatIsSet);
  EXPECT_TRUE(wire.PumpSwitchIsSet);
  EXPECT_TRUE(wire.HeatOrColdSourceSwitchIsSet);
  hvac.parameterFlags.MainThermometerChannelNoReadonly = true;
  hvac.onLoadConfig(nullptr);
  hvac.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  ASSERT_EQ(size, sizeof(wire));
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 1);
  EXPECT_EQ(wire.MainThermometerChannelId, 602);
  EXPECT_EQ(wire.AuxThermometerChannelId, 1002);
  hvac.parameterFlags.MainThermometerChannelNoReadonly = false;
  hvac.correctMain = true;
  hvac.onLoadConfig(nullptr);
  hvac.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  ASSERT_EQ(size, sizeof(wire));
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 1);
  EXPECT_EQ(wire.MainThermometerChannelId, 602);
  EXPECT_EQ(wire.PumpSwitchChannelId, 1005);
}

TEST(ChannelDependency, ServerConfigPersistenceFailurePreventsActivation) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  OutputSimulator output;
  Control::HvacBase hvac(&output);
  Sensor::VirtualThermometer thermometer;
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onInit();
  hvac.setMainThermometerChannelNo(1);
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  TChannelConfig_HVAC wire;
  hvac.copyFullChannelConfigTo(&wire);
  wire.MainThermometerChannelId = 501;
  wire.AuxThermometerChannelId = 0;
  wire.BinarySensorChannelId = 0;
  wire.MasterThermostatChannelId = 0;
  wire.PumpSwitchChannelId = 0;
  wire.HeatOrColdSourceSwitchChannelId = 0;
  TSD_ChannelConfig request = {};
  request.Func = hvac.getChannel()->getDefaultFunction();
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  request.ConfigSize = sizeof(wire);
  std::memcpy(request.Config, &wire, sizeof(wire));
  storage.failCommit = true;
  EXPECT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 1);
  EXPECT_FALSE(hvac.isChannelConfigDurable());
  storage.failCommit = false;
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_TRUE(hvac.isChannelConfigDurable());
  const int commits = storage.commits;
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(storage.commits, commits);
  Control::HvacStoredConfigV2 record;
  ASSERT_TRUE(storage.getBlob("0_hvac_cfg2", reinterpret_cast<char *>(&record),
                              sizeof(record)));
  EXPECT_EQ(record.references[0].channelId, 501u);
  EXPECT_EQ(record.referenceNamespace,
            Control::HvacReferenceNamespace::SERVER_CHANNEL_ID);
  // Disabled-function compatibility also acknowledges only durable reset.
  request.Func = 0;
  request.ConfigSize = 0;
  storage.failCommit = true;
  EXPECT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_FALSE(hvac.isChannelConfigDurable());
  storage.failCommit = false;
  EXPECT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_TRUE(hvac.isChannelConfigDurable());
  hvac.purgeConfig();
  EXPECT_EQ(storage.getBlobSize("0_hvac_cfg2"), -1);
}

TEST(ChannelDependency, EffectiveSnapshotsLeaveSourcePendingValueUntouched) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::GeneralPurposeMeasurement measurement;
  measurement.setDefaultValueDivider(1000);
  measurement.setDefaultValueMultiplier(2000);
  measurement.onLoadConfig(nullptr);
  measurement.onInit();
  measurement.getChannel()->setNewValue(12.5);
  const bool pending = measurement.getChannel()->isUpdateReady();
  TDS_SuplaDeviceChannel_E snapshot;
  ASSERT_TRUE(buildChannelSnapshot(measurement.getChannel(), &snapshot));
  double effective;
  std::memcpy(&effective, snapshot.value, sizeof(effective));
  EXPECT_DOUBLE_EQ(effective, measurement.getCalculatedValue());
  EXPECT_NE(effective, 12.5);
  EXPECT_EQ(measurement.getChannel()->isUpdateReady(), pending);
  uint8_t raw[8];
  measurement.getChannel()->fillRawValue(raw);
  double original;
  std::memcpy(&original, raw, sizeof(original));
  EXPECT_DOUBLE_EQ(original, 12.5);
  ASSERT_TRUE(buildChannelSnapshot(measurement.getChannel(), &snapshot));
  EXPECT_EQ(measurement.getChannel()->isUpdateReady(), pending);
}

TEST(ChannelDependency, BinaryEffectiveAndMeterFamiliesRaw) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  BinarySensorChannel binary;
  binary.setType(SUPLA_CHANNELTYPE_BINARYSENSOR);
  binary.setNewValue(true);
  binary.setServerInvertLogic(true);
  TDS_SuplaDeviceChannel_E snapshot;
  const bool binaryPending = binary.isUpdateReady();
  ASSERT_TRUE(buildChannelSnapshot(&binary, &snapshot));
  EXPECT_EQ(snapshot.value[0], 0);
  EXPECT_EQ(binary.isUpdateReady(), binaryPending);
  uint8_t raw[8];
  binary.fillRawValue(raw);
  EXPECT_EQ(raw[0], 1);
  Sensor::GeneralPurposeMeter meter;
  meter.setDefaultValueDivider(1000);
  meter.setDefaultValueMultiplier(3000);
  meter.onLoadConfig(nullptr);
  meter.onInit();
  meter.getChannel()->setNewValue(12.5);
  ASSERT_TRUE(buildChannelSnapshot(meter.getChannel(), &snapshot));
  double effective;
  std::memcpy(&effective, snapshot.value, sizeof(effective));
  EXPECT_DOUBLE_EQ(effective, 37.5);
  EXPECT_DOUBLE_EQ(effective, meter.getCalculatedValue());
  for (uint32_t type : {SUPLA_CHANNELTYPE_ELECTRICITY_METER,
                        SUPLA_CHANNELTYPE_IMPULSE_COUNTER}) {
    Channel channel;
    channel.setType(type);
    channel.setNewValue(123.75);
    channel.fillRawValue(raw);
    const bool pending = channel.isUpdateReady();
    ASSERT_TRUE(buildChannelSnapshot(&channel, &snapshot));
    EXPECT_EQ(std::memcmp(raw, snapshot.value, 8), 0);
    EXPECT_EQ(channel.isUpdateReady(), pending);
  }
}
}  // namespace

namespace {
TEST(ChannelDependency, ReadonlyServerReferenceRebindsAfterIdentityRemap) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  OutputSimulator output;
  ReferenceHvac hvac(&output);
  Sensor::VirtualThermometer thermometer;
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  ASSERT_TRUE(hvac.setMainThermometerChannelNo(1));
  hvac.parameterFlags.MainThermometerChannelNoReadonly = true;
  hvac.onInit();
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  TChannelConfig_HVAC wire;
  hvac.copyFullChannelConfigTo(&wire);
  wire.MainThermometerChannelId = 602;
  wire.AuxThermometerChannelId = 0;
  wire.BinarySensorChannelId = 0;
  wire.MasterThermostatChannelId = 0;
  wire.PumpSwitchChannelId = 0;
  wire.HeatOrColdSourceSwitchChannelId = 0;
  TSD_ChannelConfig request = {};
  request.Func = hvac.getChannel()->getDefaultFunction();
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  request.ConfigSize = sizeof(wire);
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(hvac.configuration().MainThermometer.channelId, 602u);
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 303;
  ids.ChannelCount = 2;
  ids.ChannelId[0] = 603;
  ids.ChannelId[1] = 604;
  ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  ASSERT_TRUE(identity.identityTransition());
  hvac.onServerIdentityTransition();
  wire.MainThermometerChannelId = 604;
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac.configuration().MainThermometer.channelId, 604u);
  identity.syncDone();
  thermometer.getChannel()->setNewValue(20.5);
  EXPECT_EQ(hvac.getPrimaryTemp(), 2050);
}
TEST(ChannelDependency, ReadonlyRemoteReferenceIsPreservedAfterIdentityRemap) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  OutputSimulator output;
  ReferenceHvac hvac(&output);
  Sensor::VirtualThermometer thermometer;
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  ASSERT_TRUE(hvac.setMainThermometerChannelNo(1));
  hvac.onInit();
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  TChannelConfig_HVAC wire;
  hvac.copyFullChannelConfigTo(&wire);
  wire.MainThermometerChannelId = 1001;
  wire.AuxThermometerChannelId = 0;
  wire.BinarySensorChannelId = 0;
  wire.MasterThermostatChannelId = 0;
  wire.PumpSwitchChannelId = 0;
  wire.HeatOrColdSourceSwitchChannelId = 0;
  TSD_ChannelConfig request = {};
  request.Func = hvac.getChannel()->getDefaultFunction();
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  request.ConfigSize = sizeof(wire);
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_EQ(hvac.configuration().MainThermometer.channelId, 1001u);
  hvac.parameterFlags.MainThermometerChannelNoReadonly = true;
  hvac.iterateAlways();
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 303;
  ids.ChannelCount = 2;
  ids.ChannelId[0] = 603;
  ids.ChannelId[1] = 604;
  ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  ASSERT_TRUE(identity.identityTransition());
  hvac.onServerIdentityTransition();
  wire.MainThermometerChannelId = 604;
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac.configuration().MainThermometer.channelId, 1001u);
  identity.syncDone();
  thermometer.getChannel()->setNewValue(20.5);
  EXPECT_EQ(hvac.getPrimaryTemp(), INT16_MIN);
}
class AmbiguousCommitConfig : public DurableHvacConfig {
 public:
  bool commit() override {
    const bool success = DurableHvacConfig::commit();
    return success && !reportFailure;
  }
  bool reportFailure = false;
};
TEST(ChannelDependency,
     ReplayAfterFailedCommitMustConfirmDurablePreviousRecord) {
  resetChannels();
  SimpleTime time;
  AmbiguousCommitConfig storage;
  OutputSimulator output;
  ReferenceHvac hvac(&output);
  Sensor::VirtualThermometer thermometer;
  hvac.getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.onInit();
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  TChannelConfig_HVAC wire;
  hvac.copyFullChannelConfigTo(&wire);
  wire.MainThermometerChannelId = 1001;
  wire.AuxThermometerChannelId = 0;
  wire.BinarySensorChannelId = 0;
  wire.MasterThermostatChannelId = 0;
  wire.PumpSwitchChannelId = 0;
  wire.HeatOrColdSourceSwitchChannelId = 0;
  TSD_ChannelConfig request = {};
  request.Func = hvac.getChannel()->getDefaultFunction();
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  request.ConfigSize = sizeof(wire);
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  storage.reportFailure = true;
  wire.MainThermometerChannelId = 1002;
  memcpy(request.Config, &wire, sizeof(wire));
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  ASSERT_FALSE(hvac.isChannelConfigDurable());
  storage.reportFailure = false;
  wire.MainThermometerChannelId = 1001;
  memcpy(request.Config, &wire, sizeof(wire));
  // The attempted durability repair also fails. Success must remain blocked.
  storage.failCommit = true;
  EXPECT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_FALSE(hvac.isChannelConfigDurable());
  storage.reboot();
  // Failure was ambiguous: B really reached flash, although active config
  // stayed A. Failed replay must not pretend that A is durable again.
  EXPECT_EQ(hvac.configuration().MainThermometer.channelId, 1002u);
  storage.failCommit = false;
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_TRUE(hvac.isChannelConfigDurable());
  storage.reboot();
  EXPECT_EQ(hvac.configuration().MainThermometer.channelId, 1001u);
  const int commits = storage.commits;
  ASSERT_EQ(hvac.handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(storage.commits, commits);
}
}  // namespace
