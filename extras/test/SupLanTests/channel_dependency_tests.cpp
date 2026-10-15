// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <math.h>
#include <gtest/gtest.h>
#include <SuplaDevice.h>
#include <output_mock.h>
#include <srpc_mock.h>
#include <simple_time.h>
#include <supla/channels/binary_sensor_channel.h>
#include <supla/channels/channel_reference.h>
#include <supla/channels/channel_handle.h>
#include <supla/condition.h>
#include <supla/events.h>
#include <supla/control/relay.h>
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
#include <new>
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
  RemoteResourceManager *remoteResources() override { return resources; }
  RemoteResourceManager *resources = nullptr;
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

TEST(ChannelDependency, EnsureEnqueueFailureAndCancelledReplyDrain) {
  resetChannels();
  SimpleTime time;
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
      .WillOnce(testing::Return(91))
      .WillOnce(testing::Return(92));
  EXPECT_FALSE(srpc.ensureResourceAccess(request));
  EXPECT_FALSE(srpc.acceptEnsureResult());
  ASSERT_TRUE(srpc.ensureResourceAccess(request));
  EXPECT_FALSE(srpc.ensureResourceAccess(request));
  srpc.cancelEnsureResourceAccess();
  request.Resource.ResourceId = 502;
  EXPECT_FALSE(srpc.ensureResourceAccess(request));
  // Drain the cancelled operation before allowing a different resource.
  EXPECT_FALSE(srpc.acceptEnsureResult());
  ASSERT_TRUE(srpc.ensureResourceAccess(request));
  EXPECT_TRUE(srpc.acceptEnsureResult());
  EXPECT_FALSE(srpc.acceptEnsureResult());
  request.Flags = 2;
  EXPECT_FALSE(srpc.ensureResourceAccess(request));
}

TEST(ChannelDependency, IndependentServerSequenceDispatchesAccessAndShare) {
  resetChannels();
  SimpleTime time;
  Channel hvac(0);
  Channel thermometer(1);
  DurableHvacConfig storage;
  testing::NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  DependencySrpc srpc(&device);
  class Results : public DependencySrpc {
   public:
    explicit Results(SuplaDeviceClass *device) : DependencySrpc(device) {}
    void ensureResourceAccessResult(
        const TSD_SuplaEnsureResourceAccessResult &) override { ++access; }
    void ensureResourceShareResult(
        const TSD_SuplaEnsureResourceShareResult &) override { ++share; }
    int access = 0;
    int share = 0;
  } results(&device);
  int handle = 0;
  srpc.prepare(&handle);
  bootstrapIdentity(&srpc.serverIdentity(), &storage);
  TDS_SuplaEnsureResourceAccess access = {};
  access.DeliveryMode = SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER;
  TDS_SuplaEnsureResourceShare share = {};
  EXPECT_CALL(wire, ensureResourceAccess(&handle, testing::_))
      .WillOnce(testing::Return(91));
  EXPECT_CALL(wire, ensureResourceShare(&handle, testing::_))
      .WillOnce(testing::Return(92));
  ASSERT_TRUE(srpc.ensureResourceAccess(access));
  ASSERT_TRUE(srpc.ensureResourceShare(share));
  TSD_SuplaEnsureResourceAccessResult accessResult = {};
  TSD_SuplaEnsureResourceShareResult shareResult = {};
  EXPECT_CALL(wire, srpc_getdata(&handle, testing::_, 0))
      .WillOnce([&](void *, TsrpcReceivedData *rd, unsigned int) {
        *rd = {};
        rd->rr_id = 701;
        rd->call_id = SUPLA_SD_CALL_ENSURE_SUPLAN_RESOURCE_ACCESS_RESULT;
        rd->data.sd_ensure_suplan_resource_access_result = &accessResult;
        return SUPLA_RESULT_TRUE;
      })
      .WillOnce([&](void *, TsrpcReceivedData *rd, unsigned int) {
        *rd = {};
        rd->rr_id = 702;
        rd->call_id = SUPLA_SD_CALL_ENSURE_SUPLAN_RESOURCE_SHARE_RESULT;
        rd->data.sd_ensure_suplan_resource_share_result = &shareResult;
        return SUPLA_RESULT_TRUE;
      });
  Supla::messageReceived(&handle, 701, 0, &srpc, 29);
  Supla::messageReceived(&handle, 702, 0, &srpc, 29);
  EXPECT_EQ(1, results.access);
  EXPECT_EQ(1, results.share);
}

TEST(ChannelDependency, AnonymousShareCancellationAndTimeoutCloseConnection) {
  resetChannels();
  SimpleTime time;
  time.value = UINT32_MAX - 5000;
  Channel hvac(0);
  Channel thermometer(1);
  DurableHvacConfig storage;
  testing::NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  DependencySrpc srpc(&device);
  int handle = 0;
  srpc.prepare(&handle);
  bootstrapIdentity(&srpc.serverIdentity(), &storage);
  TDS_SuplaEnsureResourceShare request = {};
  EXPECT_CALL(wire, ensureResourceShare(&handle, testing::_))
      .WillOnce(testing::Return(31))
      .WillOnce(testing::Return(32));
  const uint32_t loopMillis = time.value;
  time.advance(1);
  ASSERT_TRUE(srpc.ensureResourceShare(request));
  // A previous layer may enqueue after the device sampled this loop's time.
  // Keep the ordinary transport path idle, but run its production deadline.
  RegisterDevice::setEmail("");
  RegisterDevice::setServerName("");
  EXPECT_CALL(wire, srpc_free(&handle)).Times(0);
  EXPECT_FALSE(srpc.iterate(loopMillis));
  EXPECT_FALSE(srpc.ensureResourceShare(request));
  srpc.cancelEnsureResourceShare();
  EXPECT_FALSE(srpc.ensureResourceShare(request));
  EXPECT_FALSE(srpc.acceptShareResult());
  ASSERT_TRUE(srpc.ensureResourceShare(request));
  time.advance(10000);
  EXPECT_CALL(wire, srpc_free(&handle)).Times(1);
  // Production iterate closes the old transport before it can read/reassign
  // a late reply; timeout arithmetic also covers millis wraparound.
  EXPECT_FALSE(srpc.iterate(time.value));
  EXPECT_FALSE(srpc.acceptShareResult());
  EXPECT_FALSE(srpc.ensureResourceShare(request));
  srpc.prepare(&handle);
  bootstrapIdentity(&srpc.serverIdentity(), &storage);
  EXPECT_CALL(wire, ensureResourceShare(&handle, testing::_))
      .WillOnce(testing::Return(1));
  ASSERT_TRUE(srpc.ensureResourceShare(request));
  EXPECT_TRUE(srpc.acceptShareResult());
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

TEST(ChannelDependency, ManualPermissionExtensionKeepsExistingReadCache) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  const ResourceId id = {kResourceTypeChannel, 501};
  PeerContext context = {kAuthorityServer, 0, {kNodeIdDevice, 101},
                         {kNodeIdDevice, 202}, 7, 1};
  uint8_t key[32] = {1};
  AclEntry acl = {id, kPermissionRead};
  uint8_t index;
  ASSERT_TRUE(peers.addPeer(&context, key, 1, &acl, 1, &index, true));
  ASSERT_TRUE(manager.consume({1, id, kPermissionRead, true}));
  TDS_SuplaDeviceChannel_E snapshot = {};
  snapshot.Number = 0xff;
  snapshot.Type = SUPLA_CHANNELTYPE_THERMOMETER;
  snapshot.Default = SUPLA_CHANNELFNC_THERMOMETER;
  snapshot.ValueValidityTimeSec = 60;
  double temperature = 22;
  memcpy(snapshot.value, &temperature, 8);
  manager.receive(index, id, reinterpret_cast<uint8_t *>(&snapshot),
                  sizeof(snapshot), 0);
  ASSERT_TRUE(manager.consume({1, id, kPermissionControl, true}));
  manager.iterate(1);
  EXPECT_TRUE(manager.state(id, 1).available());
  EXPECT_EQ(manager.effectivePermissions(id), kPermissionRead);
  ASSERT_EQ(port.requests, 1);
  EXPECT_EQ(port.last.Flags, SUPLA_SUPLAN_ENSURE_ACCESS_FLAG_ALLOW_APPROVAL);
  EXPECT_EQ(port.last.Permissions, kPermissionRead | kPermissionControl);
  manager.ensureResult({SUPLA_SUPLAN_RESULT_OK,
                        SUPLA_SUPLAN_ACCESS_STATUS_REJECTED,
                        SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER});
  EXPECT_TRUE(manager.state(id, 1).available());
  EXPECT_EQ(manager.approvalStatus(id), SUPLA_SUPLAN_ACCESS_STATUS_REJECTED);
  manager.iterate(1000);
  EXPECT_EQ(port.requests, 1);
}
TEST(ChannelDependency, StartupManualEnsureAlsoReconcilesGrantedResource) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  const ResourceId id = {kResourceTypeChannel, 501};
  PeerContext context = {kAuthorityServer, 0, {kNodeIdDevice, 101},
                         {kNodeIdDevice, 202}, 7, 1};
  uint8_t key[32] = {1};
  AclEntry acl = {id, kPermissionRead};
  uint8_t index;
  ASSERT_TRUE(peers.addPeer(&context, key, 1, &acl, 1, &index, true));
  ASSERT_TRUE(manager.consume({1, id, kPermissionRead, true}));
  manager.iterate(0);
  EXPECT_EQ(port.requests, 1);
  manager.ensureResult({SUPLA_SUPLAN_RESULT_OK,
                        SUPLA_SUPLAN_ACCESS_STATUS_GRANTED,
                        SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER});
  manager.iterate(1000);
  EXPECT_EQ(port.requests, 1);
  manager.remove(1);
  EXPECT_TRUE(peers.authorize(index, id, kPermissionRead));
}
TEST(ChannelDependency, ManualApprovalDoesNotPromoteConfigOnlyRights) {
  PeerTable peers;
  Access port;
  RemoteResourceManager manager(&peers, nullptr, &port);
  const ResourceId id = {kResourceTypeChannel, 501};
  ASSERT_TRUE(manager.consume({1, id, kPermissionRead, true}));
  ASSERT_TRUE(manager.consume({2, id, kPermissionAction}));
  manager.iterate(0);
  ASSERT_EQ(port.requests, 1);
  EXPECT_EQ(port.last.Permissions, kPermissionRead);
  EXPECT_EQ(port.last.Flags, SUPLA_SUPLAN_ENSURE_ACCESS_FLAG_ALLOW_APPROVAL);
  manager.ensureResult({SUPLA_SUPLAN_RESULT_OK,
                        SUPLA_SUPLAN_ACCESS_STATUS_PENDING_APPROVAL,
                        SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER});
  manager.iterate(1);
  ASSERT_EQ(port.requests, 2);
  EXPECT_EQ(port.last.Flags, 0);
}

class HandleCounter : public ActionHandler {
 public:
  void handleAction(int event, int action) override {
    ++count;
    lastEvent = event;
    lastAction = action;
  }
  int count = 0;
  int lastEvent = -1;
  int lastAction = -1;
};
TEST(ChannelHandle, BeforeIdentityNoSlotsOrSpeculativeLocalChannel) {
  resetChannels();
  SuplaDeviceClass device;
  DependencySrpc protocol(&device);
  PeerTable peers;
  Access port;
  RemoteResourceManager resources(&peers, nullptr, &port);
  protocol.resources = &resources;
  ChannelHandle handle(601, kPermissionRead);
  EXPECT_EQ(handle.accessStatus(), ChannelAccessStatus::IDENTITY_UNRESOLVED);
  EXPECT_FALSE(handle.canRead());
  EXPECT_FALSE(handle.state().available());
  EXPECT_EQ(resources.consumerCount(), 0);
  EXPECT_EQ(port.requests, 0);
}
TEST(ChannelHandle, FutureStreamSelectorCannotConsumePeerOrLocalCache) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer first, second;
  SuplaDeviceClass device;
  DependencySrpc protocol(&device);
  bootstrapIdentity(&protocol.serverIdentity(), &storage);
  PeerTable peers;
  Access port;
  RemoteResourceManager resources(&peers, nullptr, &port);
  protocol.resources = &resources;
  ChannelHandle invalid(1001, kPermissionRead,
                        static_cast<RemoteDelivery>(5));
  EXPECT_EQ(invalid.accessStatus(), ChannelAccessStatus::INVALID);
  EXPECT_FALSE(invalid.canRead());
  ChannelHandle future(1001, kPermissionRead, RemoteDelivery::ServerStream);
  EXPECT_FALSE(future.canRead());
  EXPECT_FALSE(future.state().available());
  EXPECT_EQ(future.accessStatus(), ChannelAccessStatus::INVALID);
  EXPECT_FALSE(future.configure(1001, kPermissionControl));
  ChannelHandle sameDevice(601, kPermissionRead, RemoteDelivery::ServerStream);
  EXPECT_EQ(sameDevice.localChannel(), nullptr);
  EXPECT_EQ(sameDevice.accessStatus(), ChannelAccessStatus::INVALID);
  resources.iterate(0);
  EXPECT_EQ(resources.consumerCount(), 0);
  EXPECT_EQ(resources.resourceCount(), 0);
  EXPECT_EQ(port.requests, 0);
  ChannelHandle local(601, kPermissionRead);
  EXPECT_EQ(local.localChannel(), first.getChannel());
  EXPECT_TRUE(local.canRead());
}
TEST(ChannelHandle, LocalFacadeOwnsOnlyItsTrampolineAndNeverEnsures) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer first, second;
  first.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
  second.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
  ServerIdentity identity;
  bootstrapIdentity(&identity, &storage);
  HandleCounter direct, throughHandle;
  first.getChannel()->addAction(1, direct, ON_CHANGE);
  first.getChannel()->setNewValue(20.0);
  const int before = direct.count;
  {
    ChannelHandle handle(601, kPermissionRead);
    handle.addAction(2, throughHandle, ON_CHANGE);
    ChannelHandle::iterateAll();
    EXPECT_EQ(handle.accessStatus(), ChannelAccessStatus::LOCAL_CHANNEL);
    EXPECT_TRUE(handle.canRead());
    EXPECT_DOUBLE_EQ(handle.state().temperature(), 20.0);
    first.getChannel()->setNewValue(21.0);
    EXPECT_EQ(throughHandle.count, 1);
  }
  first.getChannel()->setNewValue(22.0);
  EXPECT_EQ(throughHandle.count, 1);
  EXPECT_EQ(direct.count, before + 2);
  EXPECT_EQ(Channel::GetByChannelNumber(0), first.getChannel());
}
TEST(ChannelHandle, SharedRemoteLifetimeReconfigureAndConditionBaseline) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer first, second;
  first.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
  second.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
  SuplaDeviceClass device;
  DependencySrpc protocol(&device);
  bootstrapIdentity(&protocol.serverIdentity(), &storage);
  PeerTable peers;
  Access port;
  RemoteResourceManager resources(&peers, nullptr, &port);
  protocol.resources = &resources;
  const ResourceId id = {kResourceTypeChannel, 1001};
  PeerContext context = {kAuthorityServer, 0, {kNodeIdDevice, 101},
                         {kNodeIdDevice, 202}, 7, 1};
  uint8_t key[32] = {1};
  AclEntry acl = {id, kPermissionRead};
  uint8_t index;
  ASSERT_TRUE(peers.addPeer(&context, key, 1, &acl, 1, &index, true));
  ChannelHandle shared(1001, kPermissionRead);
  HandleCounter invalid;
  shared.addAction(6, invalid, ON_CHANGE, OnInvalid());
  auto handle = new ChannelHandle(1001, kPermissionRead);
  HandleCounter counter;
  handle->addAction(9, counter, ON_CHANGE, OnGreater(20));
  ChannelHandle::iterateAll();
  ASSERT_EQ(resources.consumerCount(), 2);
  ASSERT_EQ(resources.resourceCount(), 1);
  TDS_SuplaDeviceChannel_E snapshot = {};
  snapshot.Number = 0xff;
  snapshot.Type = SUPLA_CHANNELTYPE_THERMOMETER;
  snapshot.Default = SUPLA_CHANNELFNC_THERMOMETER;
  const auto publish = [&](double temperature) {
    memcpy(snapshot.value, &temperature, 8);
    resources.receive(index, id, reinterpret_cast<uint8_t *>(&snapshot),
                       sizeof(snapshot), 0);
    ChannelHandle::iterateAll();
  };
  publish(22);
  EXPECT_EQ(counter.count, 0);
  publish(23);
  EXPECT_EQ(counter.count, 0);
  publish(NAN);
  EXPECT_EQ(invalid.count, 1);
  publish(NAN);
  EXPECT_EQ(invalid.count, 1);
  snapshot.Offline = SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE;
  publish(25);
  EXPECT_EQ(invalid.count, 1);
  snapshot.Offline = SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE;
  publish(NAN);
  EXPECT_EQ(invalid.count, 1);
  publish(19);
  publish(21);
  EXPECT_EQ(counter.count, 1);
  ASSERT_TRUE(handle->configure(601, kPermissionRead));
  ChannelHandle::iterateAll();
  EXPECT_EQ(handle->accessStatus(), ChannelAccessStatus::LOCAL_CHANNEL);
  EXPECT_EQ(resources.consumerCount(), 1);
  publish(19);
  EXPECT_EQ(counter.count, 1);
  delete handle;
  EXPECT_EQ(resources.consumerCount(), 1);
  for (int i = 0; i < 300; ++i) {
    auto temporary = new RemoteChannel(1001, kPermissionRead);
    EXPECT_TRUE(temporary->canRead());
    delete temporary;
  }
  EXPECT_EQ(resources.consumerCount(), 1);
  EXPECT_EQ(resources.resourceCount(), 1);
  EXPECT_TRUE(peers.authorize(index, id, kPermissionRead));
}
TEST(ChannelHandle, ActionOnlyReceivesGrantDrivenEventWithoutReadDemand) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer first, second;
  SuplaDeviceClass device;
  DependencySrpc protocol(&device);
  bootstrapIdentity(&protocol.serverIdentity(), &storage);
  PeerTable peers;
  Access port;
  RemoteResourceManager resources(&peers, nullptr, &port);
  protocol.resources = &resources;
  const ResourceId id = {kResourceTypeChannel, 1001};
  PeerContext context = {kAuthorityServer, 0, {kNodeIdDevice, 101},
                         {kNodeIdDevice, 202}, 7, 1};
  uint8_t key[32] = {1};
  AclEntry acl = {id, kPermissionAction};
  uint8_t index;
  ASSERT_TRUE(peers.addPeer(&context, key, 1, &acl, 1, &index, true));
  ChannelHandle handle(1001, kPermissionAction);
  HandleCounter counter;
  handle.addAction(7, counter, ON_CLICK_2);
  ChannelHandle::iterateAll();
  EXPECT_FALSE(handle.canRead());
  ChannelHandle::receiveAction(1001, SUPLA_ACTION_CAP_SHORT_PRESS_x1);
  EXPECT_EQ(counter.count, 0);
  ChannelHandle::receiveAction(1001, SUPLA_ACTION_CAP_SHORT_PRESS_x2);
  EXPECT_EQ(counter.count, 1);
  ASSERT_TRUE(peers.replaceAcl(index, 2, nullptr, 0));
  ChannelHandle::receiveAction(1001, SUPLA_ACTION_CAP_SHORT_PRESS_x2);
  EXPECT_EQ(counter.count, 1);
}

TEST(ChannelHandle, CapacityDeferredHandleAutomaticallyAdmitsWhenSlotFrees) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer first, second;
  SuplaDeviceClass device;
  DependencySrpc protocol(&device);
  bootstrapIdentity(&protocol.serverIdentity(), &storage);
  PeerTable peers;
  Access port;
  RemoteResourceManager resources(&peers, nullptr, &port);
  protocol.resources = &resources;
  std::vector<ChannelHandle *> handles;
  for (int i = 0; i <= RemoteResourceManager::kCapacity; ++i)
    handles.push_back(new ChannelHandle(1000 + i, kPermissionRead));
  ChannelHandle::iterateAll();
  EXPECT_EQ(handles.back()->accessStatus(),
            ChannelAccessStatus::RESOURCE_LIMIT);
  EXPECT_EQ(resources.resourceCount(),
            static_cast<unsigned>(RemoteResourceManager::kCapacity));
  delete handles.front();
  handles.front() = nullptr;
  ChannelHandle::iterateAll();
  EXPECT_NE(handles.back()->accessStatus(),
            ChannelAccessStatus::RESOURCE_LIMIT);
  EXPECT_EQ(resources.resourceCount(),
            static_cast<unsigned>(RemoteResourceManager::kCapacity));
  for (auto handle : handles) delete handle;
  EXPECT_EQ(resources.consumerCount(), 0);
  EXPECT_EQ(resources.resourceCount(), 0);
}
namespace {
struct GlobalDiyFixture {
  GlobalDiyFixture() { handle.deactivate(); }
  ChannelHandle handle{601, kPermissionRead};
} globalDiyFixture;
ChannelHandle &globalDiyHandle = globalDiyFixture.handle;
}
TEST(ChannelHandle, GlobalDeclarationIsResolvedAfterIdentityBootstrap) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer first, second;
  SuplaDeviceClass device;
  DependencySrpc protocol(&device);
  ASSERT_TRUE(globalDiyHandle.configure(601, kPermissionRead));
  EXPECT_EQ(globalDiyHandle.accessStatus(),
            ChannelAccessStatus::IDENTITY_UNRESOLVED);
  bootstrapIdentity(&protocol.serverIdentity(), &storage);
  EXPECT_EQ(globalDiyHandle.localChannel(), first.getChannel());
  EXPECT_TRUE(globalDiyHandle.canRead());
  globalDiyHandle.deactivate();
}
TEST(ChannelHandle, SessionLossKeepsValidCacheButRecoveryRebuildsBaseline) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer first, second;
  SuplaDeviceClass device;
  DependencySrpc protocol(&device);
  bootstrapIdentity(&protocol.serverIdentity(), &storage);
  PeerTable peers;
  Access port;
  RemoteResourceManager resources(&peers, nullptr, &port);
  protocol.resources = &resources;
  const ResourceId id = {kResourceTypeChannel, 1001};
  PeerContext context = {kAuthorityServer, 0, {kNodeIdDevice, 101},
                         {kNodeIdDevice, 202}, 7, 1};
  uint8_t key[32] = {1};
  AclEntry acl = {id, kPermissionRead};
  uint8_t index;
  ASSERT_TRUE(peers.addPeer(&context, key, 1, &acl, 1, &index, true));
  ChannelHandle handle(1001, kPermissionRead);
  HandleCounter counter;
  handle.addAction(1, counter, ON_CHANGE);
  ChannelHandle::iterateAll();
  TDS_SuplaDeviceChannel_E snapshot = {};
  snapshot.Number = 0xff;
  snapshot.Type = SUPLA_CHANNELTYPE_THERMOMETER;
  snapshot.Default = SUPLA_CHANNELFNC_THERMOMETER;
  snapshot.ValueValidityTimeSec = 60;
  double value = 20;
  memcpy(snapshot.value, &value, 8);
  resources.receive(index, id, reinterpret_cast<uint8_t *>(&snapshot),
                     sizeof(snapshot), 0);
  ChannelHandle::iterateAll();
  resources.sessionChanged(index);
  EXPECT_TRUE(handle.state().available());
  ChannelHandle::iterateAll();
  value = 21;
  time.advance(1);
  memcpy(snapshot.value, &value, 8);
  resources.receive(index, id, reinterpret_cast<uint8_t *>(&snapshot),
                     sizeof(snapshot), 1);
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 0);
  value = 22;
  time.advance(1);
  memcpy(snapshot.value, &value, 8);
  resources.receive(index, id, reinterpret_cast<uint8_t *>(&snapshot),
                     sizeof(snapshot), 2);
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 1);
  ASSERT_TRUE(handle.configure(601, kPermissionRead));
  ChannelHandle::iterateAll();
  ASSERT_TRUE(handle.configure(1001, kPermissionRead));
  time.value = UINT32_MAX - 999;
  snapshot.ValueValidityTimeSec = 1;
  resources.receive(index, id, reinterpret_cast<uint8_t *>(&snapshot),
                     sizeof(snapshot), time.value);
  ChannelHandle::iterateAll();  // TTL deadline is exactly zero after wrap.
  time.advance(1001);
  value = 23;
  memcpy(snapshot.value, &value, 8);
  resources.receive(index, id, reinterpret_cast<uint8_t *>(&snapshot),
                     sizeof(snapshot), time.value);
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 1);  // Expired baseline must not fire a transition.
  time.advance(1);
  value = 24;
  memcpy(snapshot.value, &value, 8);
  resources.receive(index, id, reinterpret_cast<uint8_t *>(&snapshot),
                     sizeof(snapshot), time.value);
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 2);
}

// Only test setup storage owns these objects; handles remain caller-owned.
struct HandleLifecycleEnvironment {
  struct Reset { Reset() { resetChannels(); } } reset;
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer first, second;
  SuplaDeviceClass device;
  DependencySrpc protocol{&device};
  PeerTable peers;
  Access access;
  RemoteResourceManager resources{&peers, nullptr, &access};
  uint8_t peer = 0xff;
  HandleLifecycleEnvironment() {
    first.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
    second.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
    bootstrapIdentity(&protocol.serverIdentity(), &storage);
    protocol.resources = &resources;
    PeerContext context = {kAuthorityServer, 0, {kNodeIdDevice, 101},
                           {kNodeIdDevice, 202}, 7, 1};
    uint8_t key[32] = {1};
    AclEntry acl[] = {{{kResourceTypeChannel, 1001}, kPermissionRead},
                     {{kResourceTypeChannel, 1002}, kPermissionRead},
                     {{kResourceTypeChannel, 1003}, kPermissionAction},
                     {{kResourceTypeChannel, 1004}, kPermissionAction}};
    EXPECT_TRUE(peers.addPeer(&context, key, 1, acl, 4, &peer, true));
  }
  void publish(uint32_t id, double value) {
    TDS_SuplaDeviceChannel_E snapshot = {};
    snapshot.Number = 0xff;
    snapshot.Type = SUPLA_CHANNELTYPE_THERMOMETER;
    snapshot.Default = SUPLA_CHANNELFNC_THERMOMETER;
    memcpy(snapshot.value, &value, 8);
    resources.receive(peer, {kResourceTypeChannel, id},
                      reinterpret_cast<const uint8_t *>(&snapshot),
                      sizeof(snapshot), time.value);
    ChannelHandle::iterateAll();
  }
};

TEST(ChannelHandle, CallbackDroppingReadStopsRemainingValueEvents) {
  HandleLifecycleEnvironment env;
  ChannelHandle handle(1001, kPermissionRead);
  class DropRead : public ActionHandler {
   public:
    explicit DropRead(ChannelHandle *source) : source(source) {}
    void handleAction(int, int) override {
      ++calls;
      EXPECT_TRUE(source->configure(1001, kPermissionAction));
    }
    ChannelHandle *source;
    int calls = 0;
  } drop(&handle);
  HandleCounter turnedOn;
  handle.addAction(1, drop, ON_CHANGE);
  handle.addAction(2, turnedOn, ON_TURN_ON);
  ChannelHandle::iterateAll();
  TDS_SuplaDeviceChannel_E snapshot = {};
  snapshot.Number = 0xff;
  snapshot.Type = SUPLA_CHANNELTYPE_BINARYSENSOR;
  snapshot.Default = SUPLA_CHANNELFNC_BINARY_SENSOR;
  env.resources.receive(env.peer, {kResourceTypeChannel, 1001},
      reinterpret_cast<const uint8_t *>(&snapshot), sizeof(snapshot), 0);
  ChannelHandle::iterateAll();
  snapshot.value[0] = 1;
  env.resources.receive(env.peer, {kResourceTypeChannel, 1001},
      reinterpret_cast<const uint8_t *>(&snapshot), sizeof(snapshot), 0);
  ChannelHandle::iterateAll();
  EXPECT_EQ(drop.calls, 1);
  EXPECT_EQ(turnedOn.count, 0);
  EXPECT_FALSE(handle.canRead());
}

TEST(ChannelHandle, LocalRemoteLocalDropsBothOldTrampolinesAndLateSnapshots) {
  HandleLifecycleEnvironment env;
  env.first.getChannel()->setNewValue(20.0);
  ChannelHandle handle(601, kPermissionRead);
  HandleCounter counter;
  handle.addAction(1, counter, ON_CHANGE);
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 0);
  ASSERT_TRUE(handle.configure(1001, kPermissionRead));
  ChannelHandle::iterateAll();
  env.first.getChannel()->setNewValue(21.0);
  EXPECT_EQ(counter.count, 0);
  EXPECT_FALSE(handle.state().available());
  env.publish(1001, 24);
  EXPECT_EQ(counter.count, 0);
  env.publish(1001, 25);
  EXPECT_EQ(counter.count, 1);
  ASSERT_TRUE(handle.configure(601, kPermissionRead));
  ChannelHandle::iterateAll();
  env.publish(1001, 99);  // Late READ from the abandoned remote Source.
  EXPECT_EQ(counter.count, 1);
  EXPECT_DOUBLE_EQ(handle.state().temperature(), 21);
  env.first.getChannel()->setNewValue(22.0);
  EXPECT_EQ(counter.count, 2);
  EXPECT_DOUBLE_EQ(handle.state().temperature(), 22);
  EXPECT_EQ(env.resources.consumerCount(), 0);
}

TEST(ChannelHandle, RemoteSourceSwitchNeverUsesOrFiresFromAbandonedCache) {
  HandleLifecycleEnvironment env;
  ChannelHandle handle(1001, kPermissionRead);
  HandleCounter counter;
  handle.addAction(1, counter, ON_CHANGE);
  ChannelHandle::iterateAll();
  env.publish(1001, 40);
  ASSERT_TRUE(handle.state().available());
  ASSERT_TRUE(handle.configure(1002, kPermissionRead));
  EXPECT_FALSE(handle.state().available());
  env.publish(1001, 99);
  EXPECT_FALSE(handle.state().available());
  EXPECT_EQ(counter.count, 0);
  env.publish(1002, 20);
  EXPECT_EQ(counter.count, 0);
  env.publish(1002, 21);
  EXPECT_EQ(counter.count, 1);
  EXPECT_DOUBLE_EQ(handle.state().temperature(), 21);
}

struct InPlaceHandle {
  alignas(ChannelHandle) uint8_t bytes[sizeof(ChannelHandle)];
  ChannelHandle *handle = nullptr;
  ~InPlaceHandle() { if (handle) handle->~ChannelHandle(); }
  void replace(uint32_t id, uint8_t rights) {
    if (handle) handle->~ChannelHandle();
    handle = new (bytes) ChannelHandle(id, rights);
  }
};

TEST(ChannelHandle, ReusedAddressCannotReceivePreviousReadOrActionCallbacks) {
  HandleLifecycleEnvironment env;
  HandleCounter old, current, other;
  InPlaceHandle slot;
  slot.replace(1001, kPermissionRead);
  slot.handle->addAction(1, old, ON_CHANGE);
  ChannelHandle::iterateAll();
  env.publish(1001, 20);
  env.publish(1001, 21);
  EXPECT_EQ(old.count, 1);
  const auto address = slot.handle;
  slot.replace(1002, kPermissionRead);
  EXPECT_EQ(slot.handle, address);
  slot.handle->addAction(2, current, ON_CHANGE);
  ChannelHandle::iterateAll();
  env.publish(1001, 99);
  EXPECT_FALSE(slot.handle->state().available());
  env.publish(1002, 30);
  EXPECT_EQ(current.count, 0);
  env.publish(1002, 31);
  EXPECT_EQ(current.count, 1);
  EXPECT_EQ(old.count, 1);

  ChannelHandle shared(1003, kPermissionAction);
  shared.addAction(3, other, ON_CLICK_2);
  slot.replace(1003, kPermissionAction);
  slot.handle->addAction(4, old, ON_CLICK_2);
  ChannelHandle::iterateAll();
  slot.replace(1004, kPermissionAction);
  slot.handle->addAction(5, current, ON_CLICK_2);
  ChannelHandle::iterateAll();
  ChannelHandle::receiveAction(1003, SUPLA_ACTION_CAP_SHORT_PRESS_x2);
  EXPECT_EQ(other.count, 1);  // The other live consumer still gets the event.
  EXPECT_EQ(old.count, 1);
  EXPECT_EQ(current.count, 1);
  ChannelHandle::receiveAction(1004, SUPLA_ACTION_CAP_SHORT_PRESS_x2);
  EXPECT_EQ(current.count, 2);
  EXPECT_TRUE(env.peers.authorize(env.peer, {kResourceTypeChannel, 1003},
                                  kPermissionAction));
}

TEST(ChannelHandle, IdentityReplacementBarrierPreventsOldLocalCallbacks) {
  HandleLifecycleEnvironment env;
  ChannelHandle old(601, kPermissionRead), replacement(801, kPermissionRead);
  HandleCounter counter;
  old.addAction(1, counter, ON_CHANGE);
  env.first.getChannel()->setNewValue(20.0);
  ChannelHandle::iterateAll();
  auto &identity = env.protocol.serverIdentity();
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities update = {};
  update.DeviceId = 303;  // Removal and registration under a new Server ID.
  update.ChannelCount = 2;
  update.ChannelId[0] = 801;
  update.ChannelId[1] = 802;
  ASSERT_EQ(identity.accept(update).Result, SUPLA_SUPLAN_RESULT_OK);
  ASSERT_TRUE(identity.identityTransition());
  EXPECT_EQ(old.accessStatus(), ChannelAccessStatus::IDENTITY_UNRESOLVED);
  EXPECT_EQ(replacement.accessStatus(),
            ChannelAccessStatus::IDENTITY_UNRESOLVED);
  env.first.getChannel()->setNewValue(21.0);
  EXPECT_EQ(counter.count, 0);
  EXPECT_EQ(env.resources.consumerCount(), 0);
  identity.syncDone();
  EXPECT_EQ(replacement.localChannel(), env.first.getChannel());
  EXPECT_EQ(old.localChannel(), nullptr);
  EXPECT_FALSE(old.canRead());
  env.first.getChannel()->setNewValue(22.0);
  EXPECT_EQ(counter.count, 0);
}

TEST(ChannelHandle, CancelledEnsureReplyCannotRejectReconfiguredResource) {
  resetChannels();
  SimpleTime time;
  DurableHvacConfig storage;
  Sensor::VirtualThermometer first, second;
  SuplaDeviceClass device;
  class ForwardSrpc : public DependencySrpc {
   public:
    explicit ForwardSrpc(SuplaDeviceClass *device) : DependencySrpc(device) {}
    void ensureResourceAccessResult(
        const TSD_SuplaEnsureResourceAccessResult &result) override {
      if (resources) resources->ensureResult(result);
    }
  } protocol(&device);
  PeerTable peers;
  class ProtocolAccess : public RemoteAccessPort {
   public:
    explicit ProtocolAccess(DependencySrpc *protocol) : protocol(protocol) {}
    bool ensure(const TDS_SuplaEnsureResourceAccess &request) override {
      return protocol->ensureResourceAccess(request);
    }
    void cancelEnsure() override { protocol->cancelEnsureResourceAccess(); }
    DependencySrpc *protocol;
  } access(&protocol);
  RemoteResourceManager resources(&peers, nullptr, &access);
  protocol.resources = &resources;
  testing::NiceMock<SrpcMock> wire;
  int transport = 0;
  protocol.prepare(&transport);
  bootstrapIdentity(&protocol.serverIdentity(), &storage);
  EXPECT_CALL(wire, ensureResourceAccess(&transport, testing::_))
      .WillOnce(testing::Return(91)).WillOnce(testing::Return(92));
  ChannelHandle handle(1001, kPermissionRead);
  ChannelHandle::iterateAll();
  resources.iterate(0);
  ASSERT_TRUE(handle.configure(1002, kPermissionRead));
  ChannelHandle::iterateAll();
  time.advance(1);
  resources.iterate(time.value);  // Cannot overtake cancelled A on SRPC.
  TSD_SuplaEnsureResourceAccessResult result = {};
  result.Result = SUPLA_SUPLAN_RESULT_OK;
  result.DeliveryMode = SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER;
  result.AccessStatus = SUPLA_SUPLAN_ACCESS_STATUS_REJECTED;
  auto reply = [&](unsigned int senderSequence) {
    EXPECT_CALL(wire, srpc_getdata(&transport, testing::_, 0))
        .WillOnce([&](void *, TsrpcReceivedData *rd, unsigned int) {
          *rd = {};
          rd->rr_id = senderSequence;
          rd->call_id = SUPLA_SD_CALL_ENSURE_SUPLAN_RESOURCE_ACCESS_RESULT;
          rd->data.sd_ensure_suplan_resource_access_result = &result;
          return SUPLA_RESULT_TRUE;
        });
    Supla::messageReceived(&transport, senderSequence, 0, &protocol, 29);
  };
  reply(901);  // Late rejection for A must be drained, not assigned to B.
  EXPECT_EQ(resources.approvalStatus({kResourceTypeChannel, 1002}),
            SUPLA_SUPLAN_ACCESS_STATUS_INVALID);
  time.advance(1000);
  resources.iterate(time.value);
  result.AccessStatus = SUPLA_SUPLAN_ACCESS_STATUS_GRANTED;
  reply(902);
  EXPECT_EQ(handle.accessStatus(), ChannelAccessStatus::PROVISIONING);
  EXPECT_FALSE(handle.canRead());  // GRANTED alone is not provisioned access.
}

TEST(ChannelHandle, ReconfigureInsideCallbackStopsRemainingOldSourceEvents) {
  HandleLifecycleEnvironment env;
  ChannelHandle handle(1001, kPermissionRead);
  class SwitchSource : public ActionHandler {
   public:
    explicit SwitchSource(ChannelHandle *handle) : handle(handle) {}
    void handleAction(int, int) override {
      ++calls;
      EXPECT_TRUE(handle->configure(1002, kPermissionRead));
    }
    ChannelHandle *handle;
    int calls = 0;
  } change(&handle);
  HandleCounter on;
  handle.addAction(1, change, ON_CHANGE);
  handle.addAction(2, on, ON_TURN_ON);
  ChannelHandle::iterateAll();
  auto publish = [&](uint32_t id, uint8_t value) {
    TDS_SuplaDeviceChannel_E snapshot = {};
    snapshot.Number = 0xff;
    snapshot.Type = SUPLA_CHANNELTYPE_RELAY;
    snapshot.Default = SUPLA_CHANNELFNC_POWERSWITCH;
    snapshot.value[0] = value;
    env.resources.receive(env.peer, {kResourceTypeChannel, id},
                          reinterpret_cast<const uint8_t *>(&snapshot),
                          sizeof(snapshot), 0);
    ChannelHandle::iterateAll();
  };
  publish(1001, 0);
  publish(1001, 1);
  EXPECT_EQ(change.calls, 1);
  EXPECT_EQ(on.count, 0);  // ON_TURN_ON belonged to abandoned Source A.
  EXPECT_FALSE(handle.state().available());
  publish(1002, 0);
  publish(1002, 1);
  EXPECT_EQ(change.calls, 2);
  EXPECT_EQ(on.count, 1);  // Same-ID configure inside B's callback is a no-op.
}

namespace {
class R1PeerPresence : public testing::Test {
 protected:
  PeerTable peers;
  RemoteResourceManager resources{&peers, nullptr, nullptr};
  PeerContext context = {kAuthorityServer, 0, {kNodeIdDevice, 101},
                         {kNodeIdDevice, 202}, 7, 1};
  uint8_t key[32] = {1};
  AclEntry entries[3] = {{{kResourceTypeChannel, 1001}, kPermissionRead},
                         {{kResourceTypeChannel, 1002}, kPermissionRead},
                         {{kResourceTypeChannel, 1003}, kPermissionRead}};
  uint8_t add(bool destination = true) {
    uint8_t index = 0xff;
    EXPECT_TRUE(peers.addPeer(&context, key, 1, entries, 3, &index,
                              destination));
    return index;
  }
  void consume(uint32_t id, uint32_t owner = 0) {
    ASSERT_TRUE(resources.consume({owner ? owner : id,
        {kResourceTypeChannel, id}, kPermissionRead}));
  }
  void publish(uint8_t peer, uint32_t id, uint32_t now = 0,
               uint32_t ttl = 0) {
    TDS_SuplaDeviceChannel_E snapshot = {};
    snapshot.Number = 0xff;
    snapshot.Type = SUPLA_CHANNELTYPE_THERMOMETER;
    snapshot.Default = SUPLA_CHANNELFNC_THERMOMETER;
    snapshot.ValueValidityTimeSec = ttl;
    double value = 21;
    memcpy(snapshot.value, &value, 8);
    resources.receive(peer, {kResourceTypeChannel, id},
        reinterpret_cast<const uint8_t *>(&snapshot), sizeof(snapshot), now);
  }
  void hint(uint8_t peer, uint8_t token, uint32_t now, bool reset = false) {
    uint8_t nonce[16] = {token};
    resources.peerAwake(peer, reset ? 0 : 200, reset, nonce, now);
  }
};

TEST_F(R1PeerPresence, QuotaAndNonceBelongToAssociationAcrossConsumers) {
  const auto peer = add();
  ASSERT_NE(peer, 0xff);
  consume(1001);
  consume(1001, 2001);
  publish(peer, 1001);
  hint(peer, 1, 0);
  hint(peer, 1, 0);  // Duplicate cannot consume the second opportunity.
  EXPECT_TRUE(resources.allowPresence(peer, 0));
  consume(1002);  // A new Channel shares the existing nonce/quota history.
  resources.remove(1001);
  EXPECT_TRUE(resources.state({kResourceTypeChannel, 1001}, 0).available());
  hint(peer, 2, 0, true);
  EXPECT_FALSE(resources.allowPresence(peer, 0));
  resources.remove(2001);
  resources.remove(1002);
  consume(1003);
  EXPECT_FALSE(resources.allowPresence(peer, 0));
  resources.remove(1003);
  consume(1001);
  EXPECT_FALSE(resources.allowPresence(peer, 999));
  EXPECT_TRUE(resources.allowPresence(peer, 1000));
}

TEST_F(R1PeerPresence, PeerQuotasAreIndependentAndRejectOtherAuthorities) {
  const auto a = add();
  consume(1001);
  context.source.nodeId = 102;
  for (auto &entry : entries) entry.resource.id += 10;
  const auto b = add();
  consume(1011);
  hint(a, 1, 0);
  hint(a, 2, 0);
  EXPECT_FALSE(resources.allowPresence(a, 0));
  EXPECT_TRUE(resources.allowPresence(b, 0));
  hint(b, 1, 0);  // Same nonce in another association is independent.
  EXPECT_TRUE(resources.allowPresence(b, 0));
  hint(b, 2, 0);
  EXPECT_FALSE(resources.allowPresence(b, 0));
  context.authorityType = kAuthorityLocal;
  context.authorityId = 9;
  context.source = {kNodeIdLocal, 103};
  context.destination = {kNodeIdLocal, 202};
  for (auto &entry : entries) entry.resource.id += 10;
  const auto local = add();
  consume(1021);
  EXPECT_FALSE(resources.allowPresence(local, 1000));
  hint(local, 3, 1000);
  EXPECT_TRUE(resources.allowPresence(a, 1000));
}

TEST_F(R1PeerPresence, SlotReplacementFencesAllBorrowedViewsAndHintState) {
  for (int variant = 0; variant < 5; ++variant) {
    SCOPED_TRACE(variant);
    const auto peer = add();
    consume(1001);
    consume(1002);
    publish(peer, 1001);
    publish(peer, 1002);
    auto a = resources.state({kResourceTypeChannel, 1001}, 0);
    auto b = resources.state({kResourceTypeChannel, 1002}, 0);
    ASSERT_TRUE(a.available());
    ASSERT_TRUE(b.available());
    hint(peer, 1, 0);
    hint(peer, 2, 0);
    EXPECT_FALSE(resources.allowPresence(peer, 0));
    ASSERT_TRUE(peers.removePeer(peer));
    if (variant == 0) ++context.rootEpoch;
    if (variant == 1) ++context.peerGeneration;
    if (variant == 2) ++context.source.nodeId;
    if (variant == 3) ++context.destination.nodeId;
    if (variant == 4) ++key[0];
    ASSERT_EQ(add(), peer);
    // Reconciling only A must also fence B's already borrowed view.
    EXPECT_FALSE(resources.state({kResourceTypeChannel, 1001}, 0).available());
    EXPECT_FALSE(a.available());
    EXPECT_FALSE(b.available());
    EXPECT_TRUE(resources.allowPresence(peer, 0));
    hint(peer, 1, 0);
    EXPECT_TRUE(resources.allowPresence(peer, 0));
    hint(peer, 2, 0);
    EXPECT_FALSE(resources.allowPresence(peer, 0));
    publish(peer, 1001);
    publish(peer, 1002);
    EXPECT_TRUE(resources.state({kResourceTypeChannel, 1002}, 0).available());
    EXPECT_FALSE(b.available());
    resources.remove(1001);
    resources.remove(1002);
    ASSERT_TRUE(peers.removePeer(peer));
    resources.authorizationChanged();
  }
}

TEST_F(R1PeerPresence, ReversedRoleAndOwnershipBarrierCannotReuseOldState) {
  const auto peer = add();
  consume(1001);
  consume(1002);
  publish(peer, 1001);
  publish(peer, 1002);
  auto old = resources.state({kResourceTypeChannel, 1002}, 0);
  peers.get(peer)->destination = false;
  EXPECT_FALSE(resources.state({kResourceTypeChannel, 1001}, 0).available());
  EXPECT_FALSE(old.available());
  EXPECT_FALSE(resources.allowPresence(peer, 0));
  peers.get(peer)->destination = true;
  EXPECT_FALSE(resources.state({kResourceTypeChannel, 1002}, 0).available());
  publish(peer, 1001);
  old = resources.state({kResourceTypeChannel, 1001}, 0);
  peers.setOwnershipCheck([](void *, const PeerContext &, const ResourceId &) {
    return false;
  }, nullptr);
  resources.authorizationChanged();
  EXPECT_FALSE(old.available());
  EXPECT_FALSE(resources.allowPresence(peer, 0));
  peers.setOwnershipCheck(nullptr, nullptr);
  EXPECT_FALSE(resources.state({kResourceTypeChannel, 1001}, 0).available());
}

TEST_F(R1PeerPresence, RevokeRegrantDoesNotRestoreValuesOrResetPeerQuota) {
  const auto peer = add();
  consume(1001);
  consume(1002);
  publish(peer, 1001);
  publish(peer, 1002);
  auto old = resources.state({kResourceTypeChannel, 1001}, 0);
  hint(peer, 1, 0);
  hint(peer, 2, 0);
  ASSERT_TRUE(peers.replaceAcl(peer, 2, entries + 1, 2));
  resources.authorizationChanged();
  EXPECT_FALSE(old.available());
  EXPECT_FALSE(resources.allowPresence(peer, 0));
  ASSERT_TRUE(peers.replaceAcl(peer, 3, entries, 3));
  resources.authorizationChanged();
  EXPECT_FALSE(resources.state({kResourceTypeChannel, 1001}, 0).available());
  EXPECT_FALSE(resources.allowPresence(peer, 0));
  ASSERT_TRUE(peers.replaceAcl(peer, 4, nullptr, 0));
  resources.authorizationChanged();
  EXPECT_FALSE(resources.allowPresence(peer, 1000));
  ASSERT_TRUE(peers.replaceAcl(peer, 5, entries, 3));
  resources.authorizationChanged();
  hint(peer, 2, 1000);  // Same-identity nonce history survives ACL edits.
  hint(peer, 3, 1000);
  EXPECT_TRUE(resources.allowPresence(peer, 1000));
  hint(peer, 4, 1000);
  EXPECT_FALSE(resources.allowPresence(peer, 1000));
}

TEST_F(R1PeerPresence, QuotaClockWrapAndResourceTtlRemainIndependent) {
  const auto peer = add();
  consume(1001);
  consume(1002);
  const uint32_t start = UINT32_MAX - 500;
  publish(peer, 1001, start, 1);
  publish(peer, 1002, start, 0);
  hint(peer, 1, start);
  hint(peer, 2, start);
  EXPECT_FALSE(resources.allowPresence(peer, start + 999));
  EXPECT_TRUE(resources.allowPresence(peer, start + 1000));
  EXPECT_FALSE(resources.state({kResourceTypeChannel, 1001},
                               start + 1000).available());
  EXPECT_TRUE(resources.state({kResourceTypeChannel, 1002},
                              start + 1000).available());
  hint(peer, 3, start + 1000, true);
  EXPECT_TRUE(resources.allowPresence(peer, start + 1000));
  hint(peer, 4, start + 1000);
  EXPECT_FALSE(resources.allowPresence(peer, start + 1000));
}

TEST_F(R1PeerPresence, FullSnapshotRawValuesAndMetadataStayLossless) {
  const auto peer = add();
  consume(1001);
  const uint32_t types[] = {SUPLA_CHANNELTYPE_THERMOMETER,
      SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR, SUPLA_CHANNELTYPE_BINARYSENSOR,
      SUPLA_CHANNELTYPE_HVAC, SUPLA_CHANNELTYPE_IMPULSE_COUNTER,
      SUPLA_CHANNELTYPE_RELAY, UINT32_MAX};
  for (uint32_t type : types) {
    TDS_SuplaDeviceChannel_E snapshot = {};
    snapshot.Number = 0xff;
    snapshot.Type = type;
    snapshot.Default = INT32_MAX;
    snapshot.ValueValidityTimeSec = UINT32_MAX;
    snapshot.Flags = INT64_MAX;
    snapshot.FuncList = INT32_MAX;
    snapshot.DefaultIcon = 255;
    snapshot.SubDeviceId = 255;
    const uint8_t raw[8] = {0, 1, 0x7f, 0x80, 0xff, 4, 5, 6};
    memcpy(snapshot.value, raw, 8);
    resources.receive(peer, {kResourceTypeChannel, 1001},
        reinterpret_cast<const uint8_t *>(&snapshot), sizeof(snapshot), 0);
    auto view = resources.state({kResourceTypeChannel, 1001}, 0);
    uint8_t output[8] = {};
    ASSERT_TRUE(view.value(output));
    EXPECT_EQ(memcmp(output, raw, 8), 0);
    EXPECT_EQ(view.type(), type);
    EXPECT_EQ(view.function(), static_cast<uint32_t>(INT32_MAX));
    EXPECT_EQ(view.validityTimeSec(), UINT32_MAX);
    const uint8_t offlineFlags[] = {SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE,
        SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE,
        SUPLA_CHANNEL_OFFLINE_FLAG_FIRMWARE_UPDATE_ONGOING,
        SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE_REMOTE_WAKEUP_NOT_SUPPORTED};
    for (uint8_t offline : offlineFlags) {
      snapshot.Offline = offline;
      resources.receive(peer, {kResourceTypeChannel, 1001},
          reinterpret_cast<const uint8_t *>(&snapshot), sizeof(snapshot), 1);
      EXPECT_FALSE(view.available());
      EXPECT_FALSE(resources.state({kResourceTypeChannel, 1001}, 1)
                       .available());
    }
  }
}
}  // namespace

namespace {
TEST_F(R1PeerPresence, LargestCanonicalIdAndBinaryFalseRemainDistinct) {
  entries[0].resource.id = INT32_MAX;
  const auto peer = add();
  consume(INT32_MAX);
  EXPECT_FALSE(resources.consume({4, {kResourceTypeChannel, UINT32_MAX},
                                   kPermissionRead}));
  TDS_SuplaDeviceChannel_E snapshot = {};
  snapshot.Number = 0xff;
  snapshot.Type = SUPLA_CHANNELTYPE_BINARYSENSOR;
  snapshot.Default = SUPLA_CHANNELFNC_BINARY_SENSOR;
  for (uint8_t raw : {0, 1}) {
    snapshot.value[0] = raw;
    resources.receive(peer, {kResourceTypeChannel, INT32_MAX},
        reinterpret_cast<const uint8_t *>(&snapshot), sizeof(snapshot), 0);
    auto view = resources.state({kResourceTypeChannel, INT32_MAX}, 0);
    bool value = !raw;
    ASSERT_TRUE(view.binary(&value));
    EXPECT_EQ(value, raw != 0);
    snapshot.Default = SUPLA_CHANNELFNC_NONE;
    resources.receive(peer, {kResourceTypeChannel, INT32_MAX},
        reinterpret_cast<const uint8_t *>(&snapshot), sizeof(snapshot), 0);
    EXPECT_FALSE(view.available());
    snapshot.Default = SUPLA_CHANNELFNC_BINARY_SENSOR;
  }
}
}  // namespace
