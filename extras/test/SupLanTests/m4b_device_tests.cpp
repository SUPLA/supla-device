// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <SuplaDevice.h>
#include <arduino_mock.h>
#include <crypto_test_hooks.h>
#include <output_mock.h>
#include <simple_time.h>
#include <supla/channels/channel_state.h>
#include <supla/control/hvac_base.h>
#include <supla/control/hvac_config.h>
#include <supla/control/relay.h>
#include <supla/control/relay_hvac_aggregator.h>
#include <supla/control/virtual_valve.h>
#include <supla/device/register_device.h>
#include <supla/protocol/suplan_protocol.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/sensor/container.h>
#include <supla/sensor/thermometer.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/storage/key_value.h>
#include <supla/storage/storage.h>
#include <supla/suplan/resource_binding_manager.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/suplan/suplan_server_identity.h>
#include <suplan/suplan_wire.h>
#include <suplan_crypto_openssl.h>

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace {
using namespace Supla;          // NOLINT(build/namespaces)
using namespace Supla::Device;  // NOLINT(build/namespaces)
using namespace Supla::SupLan;  // NOLINT(build/namespaces)

class DurableConfig : public KeyValue {
 public:
  using KeyValue::getBlobSize;
  bool init() override { return true; }
  bool commit() override {
    events.push_back("commit");
    ++commits;
    if (failCommit || commits == failCommitNumber) return false;
    std::array<uint8_t, 65536> buffer = {};
    const auto count = serializeToMemory(buffer.data(), buffer.size());
    if (count == SIZE_MAX) return false;
    disk.assign(buffer.begin(), buffer.begin() + count);
    readbackArmed = failFunctionReadback || failBlobReadback;
    return true;
  }
  bool setBlob(const char *key, const char *data, size_t size) override {
    events.push_back(std::string("write:") + key);
    if (failWrite) return false;
    return KeyValue::setBlob(key, data, size);
  }
  bool setInt32(const char *key, int32_t value) override {
    events.push_back(std::string("function:") + key);
    return !failFunction && KeyValue::setInt32(key, value);
  }
  bool getInt32(const char *key, int32_t *value) override {
    if (readbackArmed && failFunctionReadback) return false;
    return KeyValue::getInt32(key, value);
  }
  bool getBlob(const char *key, char *data, size_t size) override {
    if (readbackArmed && failBlobReadback) return false;
    return KeyValue::getBlob(key, data, size);
  }
  bool eraseKey(const char *key) override {
    events.push_back(std::string("erase:") + key);
    return !failErase && KeyValue::eraseKey(key);
  }
  void reboot() {
    removeAllMemory();
    ASSERT_TRUE(initFromMemory(disk.data(), disk.size()));
  }
  bool failFunction = false;
  bool failFunctionReadback = false;
  bool failBlobReadback = false;
  bool readbackArmed = false;
  bool failCommit = false;
  int commits = 0;
  int failCommitNumber = 0;
  bool failWrite = false;
  bool failErase = false;
  std::vector<std::string> events;
  std::vector<uint8_t> disk;
};
class IdlePort : public DatagramPort {
 public:
  bool sendUnicast(const Endpoint &, const uint8_t *data,
                   size_t length) override {
    if (linked) linked->incoming.emplace_back(data, data + length);
    return true;
  }
  bool sendLocateMulticast(const uint8_t *data, size_t length) override {
    ++locates;
    if (linked) linked->incoming.emplace_back(data, data + length);
    return true;
  }
  int pollReceive(uint8_t *buffer, size_t capacity,
                  Endpoint *endpoint) override {
    if (incoming.empty() || incoming.front().size() > capacity) return 0;
    const int size = incoming.front().size();
    std::memcpy(buffer, incoming.front().data(), size);
    *endpoint = linked->address;
    incoming.erase(incoming.begin());
    return size;
  }
  size_t maxDatagramPayload() const override { return 250; }
  uint32_t nowMs() const override { return time->value; }
  SimpleTime *time = nullptr;
  int locates = 0;
  IdlePort *linked = nullptr;
  Endpoint address = {0x0100007f, 2016};
  std::vector<std::vector<uint8_t>> incoming;
};

class BindingReceiver : public ApplicationPort {
 public:
  bool fullChannelSnapshots() const override { return true; }
  uint8_t receiveBindings(uint8_t, const uint8_t *body,
                          size_t length) override {
    snapshots.emplace_back(body, body + length);
    return 3;
  }
  bool readResource(const ResourceId &, bool *, uint8_t *, size_t,
                    size_t *) override { return false; }
  uint8_t dispatchControl(const ResourceId &, uint32_t, const uint8_t *,
                          size_t) override { return 24; }
  void receiveState(uint8_t, const ResourceId &, uint32_t, const uint8_t *data,
                    size_t size) override {
    if (size == sizeof(TDS_SuplaDeviceChannel_E)) {
      std::memcpy(&state, data, size);
      ++states;
    }
  }
  void receiveAction(uint8_t, const ResourceId &, uint32_t, const uint8_t *,
                     size_t) override {}
  void operationAcknowledged(uint8_t, const ResourceId &, uint32_t,
                             uint8_t) override {}
  std::vector<std::vector<uint8_t>> snapshots;
  TDS_SuplaDeviceChannel_E state = {};
  int states = 0;
};

TEST(M4bFollowup, RemoteNoneIsUnavailableButLocalNoneRemainsCompatible) {
  Channel::resetToDefaults();
  Channel source(0);
  for (auto type : {SUPLA_CHANNELTYPE_BINARYSENSOR,
                    SUPLA_CHANNELTYPE_THERMOMETER, SUPLA_CHANNELTYPE_HVAC}) {
    source.setType(type);
    source.setDefaultFunction(0);
    const auto role = type == SUPLA_CHANNELTYPE_BINARYSENSOR
        ? ChannelCapability::BinaryState : type == SUPLA_CHANNELTYPE_THERMOMETER
        ? ChannelCapability::Temperature : ChannelCapability::HvacDemand;
    EXPECT_TRUE(ChannelState(&source).availableFor(role));
    TDS_SuplaDeviceChannel_E snapshot;
    ASSERT_TRUE(buildChannelSnapshot(&source, &snapshot));
    EXPECT_FALSE(ChannelState(&snapshot, true).availableFor(role));
  }
}

TEST(M4bFollowup, BroadActiveBinaryMatrixAndModernTemperatureTypes) {
  const uint32_t functions[] = {
      SUPLA_CHANNELFNC_BINARY_SENSOR, SUPLA_CHANNELFNC_FLOOD_SENSOR,
      SUPLA_CHANNELFNC_CONTAINER_LEVEL_SENSOR, SUPLA_CHANNELFNC_NOLIQUIDSENSOR,
      SUPLA_CHANNELFNC_OPENINGSENSOR_WINDOW,
      SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR,
      SUPLA_CHANNELFNC_HOTELCARDSENSOR, SUPLA_CHANNELFNC_ALARMARMAMENTSENSOR,
      SUPLA_CHANNELFNC_MAILSENSOR, SUPLA_CHANNELFNC_MOTION_SENSOR,
      SUPLA_CHANNELFNC_OPENINGSENSOR_ROLLERSHUTTER,
      SUPLA_CHANNELFNC_OPENINGSENSOR_ROOFWINDOW,
      SUPLA_CHANNELFNC_OPENINGSENSOR_GARAGEDOOR,
      SUPLA_CHANNELFNC_OPENINGSENSOR_GATE,
      SUPLA_CHANNELFNC_OPENINGSENSOR_GATEWAY};
  TDS_SuplaDeviceChannel_E snapshot = {};
  snapshot.Type = SUPLA_CHANNELTYPE_BINARYSENSOR;
  for (auto function : functions) {
    snapshot.Default = function;
    for (auto role : {ChannelCapability::BinaryState,
                      ChannelCapability::FloodDetection,
                      ChannelCapability::ContainerLevel})
      EXPECT_TRUE(ChannelState(&snapshot, true).availableFor(role)) << function;
    EXPECT_FALSE(ChannelState(&snapshot, true).availableFor(
        ChannelCapability::Temperature));
  }
  snapshot.Default = SUPLA_CHANNELFNC_POWERSWITCH;
  EXPECT_FALSE(ChannelState(&snapshot, true).availableFor(
      ChannelCapability::BinaryState));
  snapshot.Type = SUPLA_CHANNELTYPE_RELAY;
  snapshot.Default = SUPLA_CHANNELFNC_BINARY_SENSOR;
  EXPECT_FALSE(ChannelState(&snapshot, true).availableFor(
      ChannelCapability::BinaryState));
  snapshot.Default = SUPLA_CHANNELFNC_THERMOMETER;
  snapshot.Type = 3000;
  EXPECT_FALSE(ChannelState(&snapshot, true).availableFor(
      ChannelCapability::Temperature));
  EXPECT_EQ(ChannelState(&snapshot, true).temperature(),
            TEMPERATURE_NOT_AVAILABLE);
  for (auto type : {SUPLA_CHANNELTYPE_THERMOMETER,
                    SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR}) {
    snapshot.Type = type;
    EXPECT_TRUE(ChannelState(&snapshot, true).availableFor(
        ChannelCapability::Temperature));
  }
}

class BindingAdapter : public Protocol::SupLan {
 public:
  using Protocol::SupLan::SupLan;
  bool share(const TDS_SuplaEnsureResourceShare &request) override {
    lastShare = request;
    ++shares;
    return true;
  }
  TDS_SuplaEnsureResourceShare lastShare = {};
  int shares = 0;
};

class CoherenceSrpc : public Protocol::SuplaSrpc {
 public:
  explicit CoherenceSrpc(SuplaDeviceClass *device) : SuplaSrpc(device, 29) {}
  void prepare(void *handle) {
    srpc = handle;
    registered = 1;
  }
  ~CoherenceSrpc() { srpc = nullptr; }
};

class FollowupHvac : public Control::HvacBase {
 public:
  using Control::HvacBase::HvacBase;
  using Control::HvacBase::setOutput;
  void clearPending() { clearChannelConfigChangedFlag(); }
};

class M4bDevice : public ::testing::Test {
 protected:
  void SetUp() override {
    Channel::resetToDefaults();
    RegisterDevice::resetToDefaults();
    Storage::SetConfigInstance(&storage);
    sensor = new Sensor::VirtualBinary;
    container = new Sensor::Container;
    valve = new Control::VirtualValve;
    relay = new Control::Relay(1);
    relay->getChannel()->setDefault(SUPLA_CHANNELFNC_PUMPSWITCH);
    relay->onLoadConfig(nullptr);
    relay->onInit();
    hvac = new FollowupHvac(&output);
    hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
    hvac->onLoadConfig(nullptr);
    hvac->onInit();
    datagrams.time = &time;
    adapter.attachRuntime(&runtime);
    adapter.attachServerAssociations(&associations);
    identity.load(&storage);
    ASSERT_TRUE(identity.registrationStarted());
    identity.registrationSucceeded();
    TSD_SuplaDeviceIdentities ids = {};
    ids.DeviceId = 202;
    ids.ChannelCount = 5;
    for (int i = 0; i < 5; ++i) ids.ChannelId[i] = 601 + i;
    ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
    identity.syncDone();
    associations.load(&storage, &identity);
    request.PeerContext = {1, 0, 1, 101, 1, 202, 123, 1};
    request.AclRevision = 1;
    request.PeerKeySize = 32;
    std::memset(request.PeerKey, 0x42, 32);
    request.ResourceCount = 1;
    request.Resources[0] = {1, 1001, 1};
    ASSERT_EQ(associations.accept(request).Result, SUPLA_SUPLAN_RESULT_OK);
  }
  void TearDown() override {
    adapter.attachServerAssociations(nullptr);
    delete hvac;
    delete relay;
    delete valve;
    delete container;
    delete sensor;
    identity.load(nullptr);
    Storage::SetConfigInstance(nullptr);
    Channel::resetToDefaults();
    RegisterDevice::resetToDefaults();
  }
  void binary(bool value, uint32_t function = SUPLA_CHANNELFNC_FLOOD_SENSOR,
              uint32_t validity = 1) {
    TDS_SuplaDeviceChannel_E state = {};
    state.Number = 0xff;
    state.Type = SUPLA_CHANNELTYPE_BINARYSENSOR;
    state.Default = function;
    state.Offline = SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE;
    state.ValueValidityTimeSec = validity;
    state.value[0] = value;
    adapter.remoteResources()->receive(0, {1, 1001},
        reinterpret_cast<uint8_t *>(&state), sizeof(state), time.value);
  }
  void hvacSample(bool heating, uint32_t validity = 1) {
    TDS_SuplaDeviceChannel_E state = {};
    state.Number = 0xff;
    state.Type = SUPLA_CHANNELTYPE_HVAC;
    state.Default = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
    state.Offline = SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE;
    state.ValueValidityTimeSec = validity;
    THVACValue value = {};
    value.Mode = SUPLA_HVAC_MODE_HEAT;
    value.Flags = SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET |
        (heating ? SUPLA_HVAC_VALUE_FLAG_HEATING : 0);
    value.SetpointTemperatureHeat = 2150;
    std::memcpy(state.value, &value, sizeof(value));
    adapter.remoteResources()->receive(0, {1, 1001},
        reinterpret_cast<uint8_t *>(&state), sizeof(state), time.value);
  }
  TSD_ChannelConfig containerWire(uint32_t id = 1001) {
    TSD_ChannelConfig result = {};
    result.Func = SUPLA_CHANNELFNC_CONTAINER;
    result.ConfigSize = sizeof(TChannelConfig_Container);
    auto wire = reinterpret_cast<TChannelConfig_Container *>(result.Config);
    wire->SensorInfo[0].ChannelId = id;
    wire->SensorInfo[0].FillLevel = 80;
    return result;
  }
  TSD_ChannelConfig valveWire() {
    TSD_ChannelConfig result = {};
    result.Func = SUPLA_CHANNELFNC_VALVE_OPENCLOSE;
    result.ConfigSize = sizeof(TChannelConfig_Valve);
    auto wire = reinterpret_cast<TChannelConfig_Valve *>(result.Config);
    wire->SensorInfo[0].ChannelId = 1001;
    wire->CloseValveOnFloodType = SUPLA_VALVE_CLOSE_ON_FLOOD_TYPE_ON_CHANGE;
    return result;
  }
  TSD_ChannelConfig bindingConfig(uint32_t pump, uint32_t hocs) {
    TChannelConfig_HVAC wire;
    hvac->copyFullChannelConfigTo(&wire);
    wire.MainThermometerChannelId = wire.AuxThermometerChannelId = 0;
    wire.BinarySensorChannelId = wire.MasterThermostatChannelId = 0;
    wire.PumpSwitchChannelId = pump;
    wire.HeatOrColdSourceSwitchChannelId = hocs;
    TSD_ChannelConfig result = {};
    result.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
    result.ConfigSize = sizeof(wire);
    std::memcpy(result.Config, &wire, sizeof(wire));
    return result;
  }
  Control::HvacStoredConfigV2 storedHvac() {
    Control::HvacStoredConfigV2 record;
    EXPECT_TRUE(storage.getBlob("4_hvac_cfg2",
        reinterpret_cast<char *>(&record), sizeof(record)));
    return record;
  }
  std::array<uint8_t, 13> binding() {
    std::array<uint8_t, 13> body = {};
    body[0] = body[1] = body[2] = body[7] = 1;
    putUint32(body.data() + 3, 1001);
    putUint32(body.data() + 8, 604);
    return body;
  }
  Element *familyElement(int family) {
    return family == 0 ? static_cast<Element *>(container) :
        family == 1 ? static_cast<Element *>(valve) : hvac;
  }
  const char *familyKey(int family) {
    return family == 0 ? "1_cnt_cfg2" :
        family == 1 ? "2_valve_cfg2" : "4_hvac_cfg2";
  }
  TSD_ChannelConfig familyRequest(int family, bool changed = false) {
    auto result = family == 0 ? containerWire() :
        family == 1 ? valveWire() : bindingConfig(1001, 0);
    if (changed) {
      if (family == 0) result.Func = SUPLA_CHANNELFNC_SEPTIC_TANK;
      if (family == 1) result.Func = 0;
      if (family == 2) {
        hvac->enableDifferentialFunctionSupport();
        result.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL;
      }
      result.ConfigSize = 0;
    }
    return result;
  }
  ChannelReference familyReference(int family) {
    return family == 0 ? container->sensorReference(0) :
        family == 1 ? valve->sensorReference(0) :
        hvac->bindingTarget(0);
  }
  void loadFamily(int family) { familyElement(family)->onLoadConfig(nullptr); }
  SimpleTime time;
  ::testing::NiceMock<DigitalInterfaceMock> io;
  DurableConfig storage;
  OutputSimulator output;
  ServerIdentity identity;
  PeerTable peers;
  IdlePort datagrams;
  OpenSslCryptoPort crypto;
  Runtime runtime{&crypto, &datagrams, &adapter, &peers, {1, 202}, 29};
  ServerAssociations associations{&peers, &runtime};
  BindingAdapter adapter{nullptr, &peers, nullptr, 0};
  Sensor::VirtualBinary *sensor = nullptr;
  Sensor::Container *container = nullptr;
  Control::VirtualValve *valve = nullptr;
  Control::Relay *relay = nullptr;
  Control::HvacBase *hvac = nullptr;
  TSDS_SuplaSetSuplanDestinationAssociation request = {};
};

TEST(M4bState, CapabilityUsesSelectedMetadataAndPreservesLocalFunctions) {
  Channel::resetToDefaults();
  Channel local(0);
  local.setType(SUPLA_CHANNELTYPE_BINARYSENSOR);
  local.setDefault(SUPLA_CHANNELFNC_MAILSENSOR);
  local.setNewValue(true);
  TDS_SuplaDeviceChannel_E snapshot;
  ASSERT_TRUE(buildChannelSnapshot(&local, &snapshot));
  EXPECT_TRUE(
      ChannelState(&local).availableFor(ChannelCapability::FloodDetection));
  EXPECT_TRUE(ChannelState(&snapshot, true)
                  .availableFor(ChannelCapability::ContainerLevel));
  snapshot.Default = SUPLA_CHANNELFNC_POWERSWITCH;
  snapshot.FuncList = UINT32_MAX;
  EXPECT_FALSE(ChannelState(&snapshot, true).availableFor(
      ChannelCapability::ContainerLevel));
  bool value;
  EXPECT_FALSE(ChannelState(&snapshot, true).binary(&value));
  snapshot.Default = SUPLA_CHANNELFNC_FLOOD_SENSOR;
  EXPECT_TRUE(ChannelState(&snapshot, true).binary(&value));
  EXPECT_TRUE(value);
}

TEST_F(M4bDevice, SharedContainerValveMetadataTtlAndRecovery) {
  auto c = containerWire();
  auto v = valveWire();
  ASSERT_EQ(container->applyChannelConfig(&c), ApplyConfigResult::Success);
  ASSERT_EQ(valve->applyChannelConfig(&v), ApplyConfigResult::Success);
  EXPECT_EQ(adapter.remoteResources()->resourceCount(), 1);
  EXPECT_EQ(adapter.remoteResources()->consumerCount(), 2);
  binary(true);
  time.advance(1000);
  // TTL boundary is unavailable; no invented DRY or empty container level.
  EXPECT_FALSE(valve->isFloodDetected());
  binary(true);
  EXPECT_TRUE(valve->isFloodDetected());
  EXPECT_FALSE(valve->isFloodDetected());
  binary(true, SUPLA_CHANNELFNC_POWERSWITCH);
  EXPECT_FALSE(valve->isFloodDetected());
  binary(true);
  EXPECT_TRUE(valve->isFloodDetected());
  time.advance(100);
  container->iterateAlways();
  EXPECT_EQ(container->getChannel()->getContainerFillValue(), 80);
  peers.replaceAcl(0, 2, nullptr, 0);
  container->iterateAlways();
  time.advance(10000);
  container->iterateAlways();
  EXPECT_EQ(container->getChannel()->getContainerFillValue(), -1);
}

TEST_F(M4bDevice, FullSlotsAndUnavailableInputsAreRetained) {
  auto c = containerWire();
  auto wire = reinterpret_cast<TChannelConfig_Container *>(c.Config);
  for (int i = 0; i < 10; ++i) {
    wire->SensorInfo[i].ChannelId = i % 2 ? 601 : 1001;
    wire->SensorInfo[i].FillLevel = i * 10;
  }
  wire->AlarmAboveLevel = 90;
  wire->Reserved[3] = 42;
  ASSERT_EQ(container->applyChannelConfig(&c), ApplyConfigResult::Success);
  TChannelConfig_Container outgoing;
  int size;
  container->fillChannelConfig(&outgoing, &size, 0);
  ASSERT_EQ(size, sizeof(outgoing));
  EXPECT_EQ(std::memcmp(&outgoing, wire, size), 0);
  auto v = valveWire();
  auto valves = reinterpret_cast<TChannelConfig_Valve *>(v.Config);
  for (int i = 0; i < 20; ++i) valves->SensorInfo[i].ChannelId = 1001 + i;
  ASSERT_EQ(valve->applyChannelConfig(&v), ApplyConfigResult::Success);
  TChannelConfig_Valve valveOut;
  valve->fillChannelConfig(&valveOut, &size, 0);
  ASSERT_EQ(size, sizeof(valveOut));
  EXPECT_EQ(std::memcmp(&valveOut, valves, size), 0);
  EXPECT_EQ(adapter.remoteResources()->resourceExhausted(),
            RemoteResourceManager::kCapacity < 20);
  EXPECT_EQ(valve->sensorReference(19), ChannelReference::server(1020));
  EXPECT_FALSE(valve->isFloodDetected());
  // No preemption; removing admitted Container interests releases slots.
  wire->SensorInfo[0].ChannelId = 0;
  for (int i = 1; i < 10; ++i) wire->SensorInfo[i].ChannelId = 0;
  ASSERT_EQ(container->applyChannelConfig(&c), ApplyConfigResult::Success);
  adapter.remoteResources()->iterate(time.value);
  EXPECT_GT(adapter.remoteResources()->consumerCount(), 0);
}

TEST_F(M4bDevice, PersistenceFailureBlocksActivationAndIdenticalReplayIsQuiet) {
  auto c = containerWire();
  storage.failCommit = true;
  EXPECT_EQ(container->applyChannelConfig(&c), ApplyConfigResult::DataError);
  EXPECT_EQ(container->sensorReference(0).kind, ChannelReferenceKind::NONE);
  EXPECT_FALSE(container->isChannelConfigDurable());
  EXPECT_EQ(adapter.remoteResources()->consumerCount(), 0);
  storage.failCommit = false;
  ASSERT_EQ(container->applyChannelConfig(&c), ApplyConfigResult::Success);
  const auto events = storage.events.size();
  ASSERT_EQ(container->applyChannelConfig(&c), ApplyConfigResult::Success);
  EXPECT_EQ(storage.events.size(), events);
  auto v = valveWire();
  storage.failWrite = true;
  EXPECT_EQ(valve->applyChannelConfig(&v), ApplyConfigResult::DataError);
  EXPECT_EQ(valve->sensorReference(0).kind, ChannelReferenceKind::NONE);
  EXPECT_FALSE(valve->isChannelConfigDurable());
}

TEST_F(M4bDevice, LocalUpdatePreservesRemoteReferenceAndNoFallback) {
  auto c = containerWire();
  ASSERT_EQ(container->applyChannelConfig(&c), ApplyConfigResult::Success);
  EXPECT_FALSE(container->setSensorSlot(0, 0, 50));
  auto wire = reinterpret_cast<TChannelConfig_Container *>(c.Config);
  wire->SensorInfo[0].IsSet = 1;
  wire->SensorInfo[0].ChannelNo = 0;
  ASSERT_EQ(container->applyChannelConfig(&c, true),
            ApplyConfigResult::Success);
  EXPECT_EQ(container->sensorReference(0), ChannelReference::server(1001));
  auto v = valveWire();
  ASSERT_EQ(valve->applyChannelConfig(&v), ApplyConfigResult::Success);
  std::memset(v.Config, 0, v.ConfigSize);
  ASSERT_EQ(valve->applyChannelConfig(&v, true), ApplyConfigResult::Success);
  EXPECT_EQ(valve->sensorReference(0), ChannelReference::server(1001));
}

TEST_F(M4bDevice, BindingDurabilityRebootRevocationAndEmpty) {
  auto body = binding();
  auto manager = adapter.resourceBindings();
  storage.failCommit = true;
  EXPECT_NE(manager->accept(0, body.data(), body.size()), 3);
  EXPECT_EQ(manager->bindingCount(), 0);
  storage.failCommit = false;
  ASSERT_EQ(manager->accept(0, body.data(), body.size()), 3);
  EXPECT_EQ(manager->bindingCount(), 1);
  const auto events = storage.events.size();
  EXPECT_EQ(manager->accept(0, body.data(), body.size()), 3);
  EXPECT_EQ(storage.events.size(), events);
  manager->iterate(time.value);
  hvacSample(true);
  manager->iterate(time.value);
  bool configured;
  EXPECT_TRUE(manager->relayDemand(3, &configured));
  EXPECT_TRUE(configured);
  storage.reboot();
  manager->attachAssociations(&associations);
  manager->iterate(time.value);
  EXPECT_EQ(manager->bindingCount(), 1);
  request.AclRevision = 2;
  request.ResourceCount = 0;
  storage.failCommit = true;
  EXPECT_EQ(associations.accept(request).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(manager->bindingCount(), 1);
  storage.failCommit = false;
  EXPECT_EQ(associations.accept(request).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(manager->bindingCount(), 0);
  EXPECT_FALSE(manager->relayDemand(3, &configured));
  EXPECT_EQ(storage.getBlobSize("slb_0"), -1);
}

TEST_F(M4bDevice, BindingCapacityIsAtomicAndIndependentOfRemoteCache) {
  auto manager = adapter.resourceBindings();
  const int perPeer = SUPLAN_MAX_BINDINGS_PER_PEER;
  int remaining = SUPLAN_MAX_RESOURCE_BINDINGS;
  int peer = 0;
  while (remaining) {
    const int count = remaining < perPeer ? remaining : perPeer;
    request.PeerContext.SourceNodeId = 101 + peer;
    request.AclRevision = 2;
    request.ResourceCount = 1;
    request.Resources[0] = {2, static_cast<uint32_t>(101 + peer),
                            SUPLA_SUPLAN_PERMISSION_READ};
    ASSERT_EQ(associations.accept(request).Result, SUPLA_SUPLAN_RESULT_OK);
    std::vector<uint8_t> body(1 + 12 * count, 0);
    body[0] = count;
    for (int i = 0; i < count; ++i) {
      auto entry = body.data() + 1 + 12 * i;
      entry[0] = entry[1] = entry[6] = 1;
      putUint32(entry + 2, 1001 + peer * perPeer + i);
      putUint32(entry + 7, 604);
    }
    ASSERT_EQ(manager->accept(peer, body.data(), body.size()), 3);
    remaining -= count;
    ++peer;
  }
  EXPECT_EQ(manager->bindingCount(), SUPLAN_MAX_RESOURCE_BINDINGS);
  EXPECT_EQ(adapter.remoteResources()->resourceCount(), 0);
  const auto writes = storage.events.size();
  auto overflow = binding();
  request.PeerContext.SourceNodeId = 101 + peer;
  request.Resources[0].ResourceId = 101 + peer;
  ASSERT_EQ(associations.accept(request).Result, SUPLA_SUPLAN_RESULT_OK);
  const auto before = storage.events.size();
  EXPECT_EQ(manager->accept(peer, overflow.data(), overflow.size()), 26);
  EXPECT_EQ(storage.events.size(), before);
  EXPECT_EQ(manager->bindingCount(), SUPLAN_MAX_RESOURCE_BINDINGS);
  uint8_t empty = 0;
  EXPECT_EQ(manager->accept(0, &empty, 1), 3);
  EXPECT_EQ(manager->accept(peer, overflow.data(), overflow.size()), 3);
  EXPECT_GT(storage.events.size(), writes);
}

TEST_F(M4bDevice, BindingGraceUsesFreshArrivalNotCachedPollAndLiveTtl) {
  auto body = binding();
  auto manager = adapter.resourceBindings();
  ASSERT_EQ(manager->accept(0, body.data(), body.size()), 3);
  manager->iterate(time.value);
  time.advance(1000);
  hvacSample(true, 1000);
  manager->iterate(time.value);
  time.advance(900001);
  manager->iterate(time.value);
  bool configured;
  // Still-live TTL permits live demand even after the grace deadline.
  EXPECT_TRUE(manager->relayDemand(3, &configured));
  time.advance(100000);
  manager->iterate(time.value);
  EXPECT_FALSE(manager->relayDemand(3, &configured));
  hvacSample(true, 1);
  manager->iterate(time.value);
  time.advance(1000);
  manager->iterate(time.value);
  EXPECT_TRUE(manager->relayDemand(3, &configured));
  time.advance(899001);
  manager->iterate(time.value);
  EXPECT_FALSE(manager->relayDemand(3, &configured));
  hvacSample(true, 1);
  manager->iterate(time.value);
  time.advance(100);
  hvacSample(true, 1);
  manager->iterate(time.value);
  time.advance(899999);
  manager->iterate(time.value);
  EXPECT_TRUE(manager->relayDemand(3, &configured));
  const uint8_t empty = 0;
  ASSERT_EQ(manager->accept(0, &empty, 1), 3);
  EXPECT_FALSE(manager->relayDemand(3, &configured));
}

TEST_F(M4bDevice, BindingWithNoConsumerSlotCannotBorrowAnotherCache) {
  auto resources = adapter.remoteResources();
  for (int i = 0; i < RemoteResourceManager::kCapacity; ++i)
    ASSERT_TRUE(resources->consume({static_cast<uint32_t>(10000 + i),
                                   {1, 1001}, 1}));
  hvacSample(true, 0);
  auto manager = adapter.resourceBindings();
  auto body = binding();
  ASSERT_EQ(manager->accept(0, body.data(), body.size()), 3);
  manager->iterate(time.value);
  bool configured = false;
  EXPECT_FALSE(manager->relayDemand(3, &configured));
  EXPECT_TRUE(configured);
  EXPECT_TRUE(resources->resourceExhausted());
  resources->remove(10000);
  manager->iterate(time.value);
  EXPECT_TRUE(manager->relayDemand(3, &configured));
  EXPECT_EQ(resources->resourceCount(), 1);
}

TEST_F(M4bDevice, IndependentContributorsUseOrAndReplacementDropsOldDemand) {
  request.AclRevision++;
  request.ResourceCount = 2;
  request.Resources[1] = {1, 1002, 1};
  ASSERT_EQ(associations.accept(request).Result, SUPLA_SUPLAN_RESULT_OK);
  auto first = binding();
  std::vector<uint8_t> body(25, 0);
  body[0] = 2;
  std::memcpy(body.data() + 1, first.data() + 1, 12);
  std::memcpy(body.data() + 13, first.data() + 1, 12);
  putUint32(body.data() + 15, 1002);
  auto manager = adapter.resourceBindings();
  ASSERT_EQ(manager->accept(0, body.data(), body.size()), 3);
  manager->iterate(time.value);
  hvacSample(true, 0);
  TDS_SuplaDeviceChannel_E state = {};
  state.Number = 0xff;
  state.Type = SUPLA_CHANNELTYPE_HVAC;
  state.Default = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  adapter.remoteResources()->receive(0, {1, 1002},
      reinterpret_cast<uint8_t *>(&state), sizeof(state), time.value);
  manager->iterate(time.value);
  bool configured;
  EXPECT_TRUE(manager->relayDemand(3, &configured));
  // Complete replacement keeps only the inactive second contributor.
  putUint32(first.data() + 3, 1002);
  ASSERT_EQ(manager->accept(0, first.data(), first.size()), 3);
  EXPECT_FALSE(manager->relayDemand(3, &configured));
  EXPECT_EQ(manager->bindingCount(), 1);
}

TEST_F(M4bDevice, BindingRejectsUnknownRoleTargetAndGenerationReplaces) {
  auto manager = adapter.resourceBindings();
  auto body = binding();
  body[1] = 2;
  EXPECT_NE(manager->accept(0, body.data(), body.size()), 3);
  body[1] = 1;
  putUint32(body.data() + 8, 601);
  EXPECT_NE(manager->accept(0, body.data(), body.size()), 3);
  body = binding();
  EXPECT_EQ(manager->accept(0, body.data(), body.size()), 3);
  request.PeerContext.PeerGeneration++;
  request.PeerKey[0]++;
  EXPECT_EQ(associations.accept(request).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(manager->bindingCount(), 0);
}

TEST_F(M4bDevice, HvacAuxBinaryMasterShareCacheAndNeverInventFalse) {
  request.AclRevision++;
  request.ResourceCount = 3;
  request.Resources[1] = {1, 1002, 1};
  request.Resources[2] = {1, 1003, 1};
  ASSERT_EQ(associations.accept(request).Result, SUPLA_SUPLAN_RESULT_OK);
  TChannelConfig_HVAC wire;
  hvac->copyFullChannelConfigTo(&wire);
  wire.MainThermometerChannelId = 1001;
  wire.AuxThermometerChannelId = 1001;
  wire.AuxThermometerType = SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR;
  wire.BinarySensorChannelId = 1002;
  wire.MasterThermostatChannelId = 1003;
  wire.PumpSwitchChannelId = wire.HeatOrColdSourceSwitchChannelId = 0;
  TSD_ChannelConfig config = {};
  config.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  config.ConfigSize = sizeof(wire);
  std::memcpy(config.Config, &wire, sizeof(wire));
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(adapter.remoteResources()->resourceCount(), 3);
  EXPECT_EQ(adapter.remoteResources()->consumerCount(), 4);
  TDS_SuplaDeviceChannel_E temperature = {};
  temperature.Number = 0xff;
  temperature.Type = SUPLA_CHANNELTYPE_THERMOMETER;
  temperature.Default = SUPLA_CHANNELFNC_THERMOMETER;
  temperature.ValueValidityTimeSec = 2;
  double degrees = 21.25;
  std::memcpy(temperature.value, &degrees, sizeof(degrees));
  adapter.remoteResources()->receive(0, {1, 1001},
                                     reinterpret_cast<uint8_t *>(&temperature),
                                     sizeof(temperature), time.value);
  EXPECT_EQ(hvac->getPrimaryTemp(), 2125);
  EXPECT_EQ(hvac->getSecondaryTemp(), 2125);
  peers.get(0)->endpointState = kPeerEndpointNone;
  EXPECT_EQ(hvac->getSecondaryTemp(), 2125);
  time.advance(2000);
  EXPECT_EQ(hvac->getSecondaryTemp(), INT16_MIN);
  // Let normal HVAC startup/config reaction gates settle, then provide valid
  // thermometers so their protections do not mask the binary interlock.
  time.advance(40000);
  temperature.ValueValidityTimeSec = 3600;
  adapter.remoteResources()->receive(0, {1, 1001},
                                     reinterpret_cast<uint8_t *>(&temperature),
                                     sizeof(temperature), time.value);
  // A valid master applies its setpoint, then unavailable preserves that state.
  TDS_SuplaDeviceChannel_E master = {};
  master.Number = 0xff;
  master.Type = SUPLA_CHANNELTYPE_HVAC;
  master.Default = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  master.ValueValidityTimeSec = 1;
  THVACValue value = {};
  value.Mode = SUPLA_HVAC_MODE_HEAT;
  value.Flags = SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
  value.SetpointTemperatureHeat = 2250;
  std::memcpy(master.value, &value, sizeof(value));
  adapter.remoteResources()->receive(0, {1, 1003},
      reinterpret_cast<uint8_t *>(&master), sizeof(master), time.value);
  hvac->iterateAlways();
  EXPECT_EQ(hvac->getChannel()->getHvacSetpointTemperatureHeat(), 2250);
  time.advance(2000);
  hvac->iterateAlways();
  EXPECT_EQ(hvac->getChannel()->getHvacSetpointTemperatureHeat(), 2250);
  // Missing binary does not force OFF. Valid false does.
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagForcedOffBySensor());
  TDS_SuplaDeviceChannel_E interlock = {};
  interlock.Number = 0xff;
  interlock.Type = SUPLA_CHANNELTYPE_BINARYSENSOR;
  interlock.Default = SUPLA_CHANNELFNC_BINARY_SENSOR;
  adapter.remoteResources()->receive(0, {1, 1002},
      reinterpret_cast<uint8_t *>(&interlock), sizeof(interlock), time.value);
  time.advance(6000);
  hvac->iterateAlways();
  EXPECT_TRUE(hvac->getChannel()->isHvacFlagForcedOffBySensor());
  interlock.Default = SUPLA_CHANNELFNC_POWERSWITCH;
  adapter.remoteResources()->receive(0, {1, 1002},
      reinterpret_cast<uint8_t *>(&interlock), sizeof(interlock), time.value);
  time.advance(2000);
  hvac->iterateAlways();
  EXPECT_FALSE(hvac->getChannel()->isHvacFlagForcedOffBySensor());
}

TEST_F(M4bDevice, FunctionMetadataReachesPeerWithUnchangedValue) {
  ScopedCryptoTestState rng{5};
  TSDS_SuplaSetSuplanSourceAssociation source = {};
  source.PeerContext = {1, 0, 1, 202, 1, 303, identity.rootEpoch(), 1};
  source.AclRevision = 1;
  source.Flags = SUPLA_SUPLAN_SOURCE_FLAG_RETURN_PEER_KEY;
  source.AclEntryCount = 2;
  source.Acl[0] = {1, 601, 1};
  source.Acl[1] = {1, 605, 1};
  const auto provisioned = associations.accept(source);
  ASSERT_EQ(provisioned.Result, SUPLA_SUPLAN_RESULT_OK);
  PeerContext context;
  ASSERT_TRUE(decodePeerContext(peers.get(1)->contextBytes, &context));
  PeerTable destinationPeers;
  const AclEntry expected[] = {{{1, 601}, 1}, {{1, 605}, 1}};
  uint8_t peer;
  ASSERT_TRUE(destinationPeers.addPeer(&context, provisioned.PeerKey, 1,
                                      expected, 2, &peer, true));
  IdlePort port;
  port.time = &time;
  port.address = {0x0200007f, 2016};
  datagrams.linked = &port;
  port.linked = &datagrams;
  BindingReceiver receiver;
  Runtime destination(&crypto, &port, &receiver, &destinationPeers,
                      {1, 303}, 29);
  auto pump = [&]() {
    for (int i = 0; i < 200; ++i) {
      time.advance(10);
      runtime.iterate();
      destination.iterate();
    }
  };
  ASSERT_TRUE(destination.requestRead(peer, {1, 601}));
  pump();
  ASSERT_GT(receiver.states, 0);
  std::array<int8_t, 8> original;
  std::memcpy(original.data(), receiver.state.value, 8);
  TSD_ChannelConfig selected = {};
  selected.Func = SUPLA_CHANNELFNC_FLOOD_SENSOR;
  sensor->handleChannelConfig(&selected, false);
  pump();
  EXPECT_EQ(receiver.state.Default, selected.Func);
  EXPECT_EQ(std::memcmp(original.data(), receiver.state.value, 8), 0);
  EXPECT_TRUE(ChannelState(&receiver.state, true).availableFor(
      ChannelCapability::BinaryState));
  selected.Func = 0;
  ASSERT_EQ(sensor->handleChannelConfig(&selected, false),
            SUPLA_CONFIG_RESULT_TRUE);
  pump();
  EXPECT_EQ(receiver.state.Default, 0);
  EXPECT_FALSE(ChannelState(&receiver.state, true).availableFor(
      ChannelCapability::BinaryState));
  identity.disconnected();
  ASSERT_TRUE(sensor->setRuntimeFunction(
      SUPLA_CHANNELFNC_OPENINGSENSOR_WINDOW));
  pump();
  EXPECT_EQ(receiver.state.Default, SUPLA_CHANNELFNC_OPENINGSENSOR_WINDOW);
  EXPECT_EQ(std::memcmp(original.data(), receiver.state.value, 8), 0);

  ASSERT_TRUE(destination.requestRead(peer, {1, 605}));
  pump();
  selected.Func = 0;
  ASSERT_EQ(hvac->handleChannelConfig(&selected, false),
            SUPLA_CONFIG_RESULT_TRUE);
  pump();
  EXPECT_EQ(receiver.state.Default, 0);
  std::memcpy(original.data(), receiver.state.value, 8);
  const int before = receiver.states;
  selected.Func = hvac->getChannel()->getDefaultFunction();
  ASSERT_EQ(hvac->handleChannelConfig(&selected, false),
            SUPLA_CONFIG_RESULT_TRUE);
  pump();
  EXPECT_GT(receiver.states, before);
  EXPECT_EQ(receiver.state.Default, selected.Func);
  EXPECT_EQ(std::memcmp(original.data(), receiver.state.value, 8), 0);
}

TEST_F(M4bDevice, HvacAcceptedNoneSurvivesReplayRebootAndKeepsLocalFunction) {
  hvac->getChannel()->addToFuncList(SUPLA_BIT_FUNC_HVAC_THERMOSTAT_HEAT_COOL);
  hvac->changeFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL, true);
  const auto internal = hvac->getChannel()->getDefaultFunction();
  TSD_ChannelConfig none = {};
  static_cast<FollowupHvac *>(hvac)->setOutput(100, true);
  ASSERT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), internal);
  EXPECT_EQ(output.outputValue, 0);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  EXPECT_EQ(storage.getBlobSize("4_hvac_cfg2"), 124);
  EXPECT_EQ(storedHvac().serverNone.channelId, 605);
  TDS_SuplaDeviceChannel_E snapshot;
  ASSERT_TRUE(buildChannelSnapshot(hvac->getChannel(), &snapshot));
  EXPECT_EQ(snapshot.Default, 0);
  EXPECT_FALSE(ChannelState(&snapshot, true).availableFor(
      ChannelCapability::HvacMaster));
  EXPECT_FALSE(ChannelState(&snapshot, true).availableFor(
      ChannelCapability::HvacDemand));
  EXPECT_TRUE(ChannelState(hvac->getChannel()).availableFor(
      ChannelCapability::HvacMaster));
  hvac->onRegistered(nullptr);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  storage.reboot();
  hvac->onLoadConfig(nullptr);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), internal);
  const int commits = storage.commits;
  ASSERT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(storage.commits, commits);
  auto active = bindingConfig(0, 0);
  active.Func = internal;
  active.ConfigSize = 0;
  ASSERT_EQ(hvac->handleChannelConfig(&active, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac->supLanFunction(), internal);
  EXPECT_EQ(storedHvac().serverNone.rootEpoch, 0);
  storage.reboot();
  hvac->onLoadConfig(nullptr);
  EXPECT_EQ(hvac->supLanFunction(), internal);
}

TEST_F(M4bDevice, HvacRejectedOrUndurableFunctionDoesNotChangeProjection) {
  const auto internal = hvac->getChannel()->getDefaultFunction();
  TSD_ChannelConfig request = {};
  request.Func = SUPLA_CHANNELFNC_POWERSWITCH;
  EXPECT_EQ(hvac->handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED);
  EXPECT_EQ(hvac->supLanFunction(), internal);
  request.Func = 0;
  request.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
  EXPECT_EQ(hvac->handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  storage.failCommit = true;
  EXPECT_EQ(hvac->handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_EQ(hvac->supLanFunction(), internal);
  EXPECT_FALSE(hvac->isChannelConfigDurable());
  storage.failCommit = false;
  ASSERT_EQ(hvac->handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  auto active = bindingConfig(0, 0);
  storage.failCommit = true;
  EXPECT_EQ(hvac->handleChannelConfig(&active, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  hvac->onRegistered(nullptr);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  storage.failCommit = false;
  // A supported empty DEFAULT also applies the function and requests upload.
  request.Func = internal;
  EXPECT_EQ(hvac->handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac->supLanFunction(), internal);
}

TEST_F(M4bDevice, AcceptedNoneRestoresInNewHvacWithoutServer) {
  TSD_ChannelConfig none = {};
  ASSERT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_TRUE);
  identity.load(nullptr);
  delete hvac;
  storage.reboot();
  hvac = new FollowupHvac(&output);
  ASSERT_EQ(hvac->getChannelNumber(), 4);
  hvac->onLoadConfig(nullptr);
  hvac->onInit();
  identity.load(&storage);
  associations.load(&storage, &identity);
  EXPECT_TRUE(identity.identityAvailable());
  EXPECT_EQ(hvac->supLanFunction(), 0);
  TDS_SuplaDeviceChannel_E snapshot;
  ASSERT_TRUE(buildChannelSnapshot(hvac->getChannel(), &snapshot));
  EXPECT_EQ(snapshot.Default, 0);
  EXPECT_NE(hvac->getChannel()->getDefaultFunction(), 0);
}

TEST_F(M4bDevice, NoneRecordRejectsCorruptVersionAndPartialScope) {
  TSD_ChannelConfig none = {};
  ASSERT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_TRUE);
  auto record = storedHvac();
  auto broken = record;
  Control::HvacConfiguration restored;
  broken.version = 99;
  EXPECT_FALSE(Control::restoreHvacConfig(broken, &restored));
  broken = record;
  broken.serverNone.channelId = 0;
  EXPECT_FALSE(Control::restoreHvacConfig(broken, &restored));
  ASSERT_TRUE(storage.setBlob("4_hvac_cfg2",
      reinterpret_cast<const char *>(&broken), sizeof(broken)));
  ASSERT_TRUE(storage.commit());
  storage.reboot();
  hvac->onLoadConfig(nullptr);
  EXPECT_NE(hvac->supLanFunction(), 0);
  broken = record;
  broken.serverNone.rootEpoch = 0;
  EXPECT_FALSE(Control::restoreHvacConfig(broken, &restored));
  ASSERT_TRUE(storage.setBlob("4_hvac_cfg2",
      reinterpret_cast<const char *>(&record), sizeof(record)));
  ASSERT_TRUE(storage.commit());
  storage.reboot();
  hvac->onLoadConfig(nullptr);
  EXPECT_EQ(hvac->supLanFunction(), 0);
}

TEST_F(M4bDevice, ScalarEditPreservesNoneInOneCompleteRecord) {
  TSD_ChannelConfig none = {};
  ASSERT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_TRUE(hvac->setOutputValueOnError(50));
  hvac->saveConfig();
  const auto record = storedHvac();
  EXPECT_EQ(record.serverNone.channelId, 605);
  storage.reboot();
  hvac->onLoadConfig(nullptr);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  EXPECT_EQ(storedHvac().scalars.OutputValueOnError, 50);
}

TEST_F(M4bDevice, OriginalV2PrefixLoadsWithoutEagerRewrite) {
  hvac->saveConfig();
  auto record = storedHvac();
  constexpr size_t prefixSize =
      offsetof(Control::HvacStoredConfigV2, serverNone);
  static_assert(prefixSize == 112, "original V2 size");
  ASSERT_TRUE(storage.setBlob("4_hvac_cfg2",
      reinterpret_cast<const char *>(&record), prefixSize));
  ASSERT_TRUE(storage.commit());
  storage.reboot();
  hvac->onLoadConfig(nullptr);
  EXPECT_NE(hvac->supLanFunction(), 0);
  EXPECT_EQ(storage.getBlobSize("4_hvac_cfg2"), prefixSize);
  storage.failCommit = true;
  TSD_ChannelConfig none = {};
  EXPECT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_EQ(storage.getBlobSize("4_hvac_cfg2"), prefixSize);
  storage.failCommit = false;
  ASSERT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(storage.getBlobSize("4_hvac_cfg2"), sizeof(record));
  EXPECT_EQ(storedHvac().serverNone.channelId, 605);
}

TEST_F(M4bDevice, NoneRecordWriteFailuresKeepPreviousProjection) {
  TSD_ChannelConfig config = {};
  const auto active = hvac->getChannel()->getDefaultFunction();
  storage.failWrite = true;
  EXPECT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_EQ(hvac->supLanFunction(), active);
  storage.failWrite = false;
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  storage.failWrite = true;
  config.Func = active;
  EXPECT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  storage.failWrite = false;
  config.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL;
  hvac->enableDifferentialFunctionSupport();
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac->supLanFunction(), config.Func);
  EXPECT_EQ(storedHvac().serverNone.rootEpoch, 0);
}

TEST_F(M4bDevice, HvacNoneScopeRejectsRemappedIdentityAndRootRotation) {
  TSD_ChannelConfig none = {};
  ASSERT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_TRUE);
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 202;
  ids.ChannelCount = 5;
  for (int i = 0; i < 5; ++i) ids.ChannelId[i] = 701 + i;
  ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_TRUE(identity.identityTransition());
  hvac->onServerIdentityTransition();
  EXPECT_EQ(hvac->supLanFunction(), hvac->getChannel()->getDefaultFunction());
  ASSERT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  identity.syncDone();
  ASSERT_TRUE(identity.rotateRoot());
  EXPECT_EQ(hvac->supLanFunction(), hvac->getChannel()->getDefaultFunction());
}

TEST_F(M4bDevice, LocalFunctionEditClearsAcceptedNoneOnlyAfterPersistence) {
  TSD_ChannelConfig none = {};
  ASSERT_EQ(hvac->handleChannelConfig(&none, false),
            SUPLA_CONFIG_RESULT_TRUE);
  identity.disconnected();
  storage.failCommit = true;
  hvac->changeFunction(hvac->getChannel()->getDefaultFunction(), true);
  EXPECT_EQ(hvac->supLanFunction(), 0);
  storage.failCommit = false;
  hvac->changeFunction(hvac->getChannel()->getDefaultFunction(), true);
  EXPECT_EQ(hvac->supLanFunction(), hvac->getChannel()->getDefaultFunction());
}

TEST_F(M4bDevice, SourceFirstEnsureUsesRealHvacDesiredConfigAndSyncBarrier) {
  TChannelConfig_HVAC wire;
  hvac->copyFullChannelConfigTo(&wire);
  wire.MainThermometerChannelId = wire.AuxThermometerChannelId = 0;
  wire.BinarySensorChannelId = wire.MasterThermostatChannelId = 0;
  wire.PumpSwitchChannelId = 9001;
  wire.HeatOrColdSourceSwitchChannelId = 0;
  TSD_ChannelConfig config = {};
  config.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  config.ConfigSize = sizeof(wire);
  std::memcpy(config.Config, &wire, sizeof(wire));
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  auto manager = adapter.resourceBindings();
  identity.disconnected();
  manager->iterate(time.value);
  EXPECT_EQ(adapter.shares, 0);
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 202;
  ids.ChannelCount = 5;
  for (int i = 0; i < 5; ++i) ids.ChannelId[i] = 601 + i;
  ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  identity.syncDone();
  manager->iterate(time.value);
  ASSERT_EQ(adapter.shares, 1);
  EXPECT_EQ(adapter.lastShare.SourceResource.ResourceId, 605);
  EXPECT_EQ(adapter.lastShare.DestinationResource.ResourceId, 9001);
  EXPECT_EQ(adapter.lastShare.Permissions, SUPLA_SUPLAN_PERMISSION_READ);
  TSD_SuplaEnsureResourceShareResult result = {};
  result.Result = SUPLA_SUPLAN_RESULT_OK;
  result.DestinationDeviceId = 303;
  manager->ensureShareResult(result);
  manager->iterate(time.value);
  EXPECT_EQ(adapter.shares, 1);
}

TEST_F(M4bDevice, BlockedShareDoesNotStarveProvisionedBinding) {
  TSDS_SuplaSetSuplanSourceAssociation source = {};
  source.PeerContext = {1, 0, 1, 202, 1, 303, identity.rootEpoch(), 1};
  source.AclRevision = 1;
  source.AclEntryCount = 1;
  source.Acl[0] = {1, 605, 1};
  ASSERT_EQ(associations.accept(source).Result, SUPLA_SUPLAN_RESULT_OK);
  auto config = bindingConfig(9001, 9002);
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  auto manager = adapter.resourceBindings();
  time.advance(1000);
  manager->iterate(time.value);
  ASSERT_EQ(adapter.shares, 1);
  TSD_SuplaEnsureResourceShareResult result = {};
  result.Result = SUPLA_SUPLAN_RESULT_OK;
  result.DestinationDeviceId = 303;
  manager->ensureShareResult(result);
  manager->iterate(time.value);
  ASSERT_EQ(adapter.shares, 2);
  // A genuinely unresolved route still prevents a partial complete snapshot.
  EXPECT_EQ(datagrams.locates, 0);
  result.Result = SUPLA_SUPLAN_RESULT_NOT_AUTHORIZED;
  manager->ensureShareResult(result);
  manager->iterate(time.value);
  EXPECT_GT(datagrams.locates, 0);
  const int shares = adapter.shares;
  manager->iterate(time.value);
  EXPECT_EQ(adapter.shares, shares);
  manager->serverReconnected();
  manager->iterate(time.value);
  EXPECT_EQ(adapter.shares, shares + 1);
}

TEST_F(M4bDevice, MissingPeerAuthorizationDoesNotStarveOtherPeer) {
  TSDS_SuplaSetSuplanSourceAssociation source = {};
  source.PeerContext = {1, 0, 1, 202, 1, 303, identity.rootEpoch(), 1};
  source.AclRevision = 1;
  source.AclEntryCount = 1;
  source.Acl[0] = {1, 604, 1};  // Unrelated grant keeps this peer installed.
  ASSERT_EQ(associations.accept(source).Result, SUPLA_SUPLAN_RESULT_OK);
  source.PeerContext.DestinationNodeId = 404;
  source.Acl[0].ResourceId = 605;
  ASSERT_EQ(associations.accept(source).Result, SUPLA_SUPLAN_RESULT_OK);
  auto config = bindingConfig(9001, 9002);
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  auto manager = adapter.resourceBindings();
  time.advance(1000);
  manager->iterate(time.value);
  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(adapter.shares, i + 1);
    TSD_SuplaEnsureResourceShareResult result = {};
    result.Result = SUPLA_SUPLAN_RESULT_OK;
    result.DestinationDeviceId =
        adapter.lastShare.DestinationResource.ResourceId == 9001 ? 303 : 404;
    manager->ensureShareResult(result);
    manager->iterate(time.value);
  }
  EXPECT_GT(datagrams.locates, 0);
}

TEST_F(M4bDevice, TransientShareFailureRetriesWithoutDeclaringEmpty) {
  TSDS_SuplaSetSuplanSourceAssociation source = {};
  source.PeerContext = {1, 0, 1, 202, 1, 303, identity.rootEpoch(), 1};
  source.AclRevision = 1;
  source.AclEntryCount = 1;
  source.Acl[0] = {1, 605, 1};
  ASSERT_EQ(associations.accept(source).Result, SUPLA_SUPLAN_RESULT_OK);
  auto config = bindingConfig(9001, 0);
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  auto manager = adapter.resourceBindings();
  time.advance(1000);
  manager->iterate(time.value);
  ASSERT_EQ(adapter.shares, 1);
  TSD_SuplaEnsureResourceShareResult result = {};
  result.Result = SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR;
  manager->ensureShareResult(result);
  time.advance(999);
  manager->iterate(time.value);
  EXPECT_EQ(adapter.shares, 1);
  EXPECT_EQ(datagrams.locates, 0);
  time.advance(1);
  manager->iterate(time.value);
  EXPECT_EQ(adapter.shares, 2);
  result.Result = SUPLA_SUPLAN_RESULT_OK;
  result.DestinationDeviceId = 303;
  manager->ensureShareResult(result);
  manager->iterate(time.value);
  EXPECT_GT(datagrams.locates, 0);
}

TEST_F(M4bDevice, FailedHvacPersistenceDoesNotPublishEmptyBindings) {
  ScopedCryptoTestState rng{5};
  TSDS_SuplaSetSuplanSourceAssociation source = {};
  source.PeerContext = {1, 0, 1, 202, 1, 303, identity.rootEpoch(), 1};
  source.AclRevision = 1;
  source.Flags = SUPLA_SUPLAN_SOURCE_FLAG_RETURN_PEER_KEY;
  source.AclEntryCount = 1;
  source.Acl[0] = {1, 605, 1};
  const auto provisioned = associations.accept(source);
  ASSERT_EQ(provisioned.Result, SUPLA_SUPLAN_RESULT_OK);
  ASSERT_EQ(provisioned.PeerKeySize, 32);
  PeerContext context;
  ASSERT_TRUE(decodePeerContext(peers.get(1)->contextBytes, &context));
  PeerTable destinationPeers;
  const AclEntry expected = {{kResourceTypeChannel, 605}, kPermissionRead};
  uint8_t destinationPeer;
  ASSERT_TRUE(destinationPeers.addPeer(&context, provisioned.PeerKey, 1,
                                      &expected, 1, &destinationPeer, true));
  IdlePort destinationPort;
  destinationPort.time = &time;
  destinationPort.address = {0x0200007f, 2016};
  datagrams.linked = &destinationPort;
  destinationPort.linked = &datagrams;
  BindingReceiver receiver;
  Runtime destination(&crypto, &destinationPort, &receiver, &destinationPeers,
                      {1, 303}, 29);
  auto manager = adapter.resourceBindings();
  auto pump = [&]() {
    for (int i = 0; i < 200; ++i) {
      time.advance(10);
      manager->iterate(time.value);
      runtime.iterate();
      destination.iterate();
    }
  };
  auto config = bindingConfig(9001, 0);
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  manager->iterate(time.value);
  ASSERT_EQ(adapter.shares, 1);
  TSD_SuplaEnsureResourceShareResult result = {};
  result.Result = SUPLA_SUPLAN_RESULT_OK;
  result.DestinationDeviceId = 303;
  manager->ensureShareResult(result);
  pump();
  EXPECT_GT(datagrams.locates, 0);
  EXPECT_GT(destination.diagnostics().locateRx, 0);
  EXPECT_EQ(destination.diagnostics().invalidLocateDrop, 0);
  EXPECT_GT(runtime.diagnostics().locateReplyRx, 0);
  EXPECT_GT(runtime.diagnostics().sessionEstablished, 0);
  EXPECT_GT(destination.diagnostics().sessionEstablished, 0);
  ASSERT_EQ(receiver.snapshots.size(), 1);
  ASSERT_EQ(receiver.snapshots[0].size(), 13);
  EXPECT_EQ(receiver.snapshots[0][0], 1);
  EXPECT_EQ(getUint32(receiver.snapshots[0].data() + 8), 9001);
  ASSERT_FALSE(runtime.bindingsInFlight());

  // A failed scalar-only update keeps the previous dependency active.
  auto wire = reinterpret_cast<TChannelConfig_HVAC *>(config.Config);
  wire->OutputValueOnError = 50;
  storage.failCommit = true;
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  ASSERT_TRUE(identity.serverSyncComplete());
  EXPECT_EQ(hvac->bindingTarget(0), ChannelReference::server(9001));
  pump();
  EXPECT_EQ(receiver.snapshots.size(), 1);
  EXPECT_FALSE(runtime.bindingsInFlight());

  // Also withhold cleanup while an unset request has not been persisted.
  auto unset = bindingConfig(0, 0);
  ASSERT_EQ(hvac->handleChannelConfig(&unset, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  pump();
  EXPECT_EQ(receiver.snapshots.size(), 1);
  EXPECT_EQ(hvac->bindingTarget(0), ChannelReference::server(9001));

  // Retrying the accepted dependency preserves its resolved route.
  storage.failCommit = false;
  ASSERT_EQ(hvac->handleChannelConfig(&config, false),
            SUPLA_CONFIG_RESULT_TRUE);
  pump();
  EXPECT_EQ(adapter.shares, 1);
  EXPECT_EQ(receiver.snapshots.size(), 1);

  // Only a durable removal may replace the binding set with an empty one.
  ASSERT_EQ(hvac->handleChannelConfig(&unset, false),
            SUPLA_CONFIG_RESULT_TRUE);
  pump();
  ASSERT_EQ(receiver.snapshots.size(), 2);
  ASSERT_EQ(receiver.snapshots[1].size(), 1);
  EXPECT_EQ(receiver.snapshots[1][0], 0);
  EXPECT_FALSE(runtime.bindingsInFlight());
  datagrams.linked = nullptr;
}

TEST_F(M4bDevice, LegacyCleanupRetriesFailedCommitForContainerAndValve) {
  Sensor::ContainerConfig legacy;
  legacy.sensorData[0].channelNumber = 0;
  legacy.sensorData[0].fillLevel = 80;
  Control::ValveConfig oldValve;
  oldValve.sensorData[0] = 0;
  oldValve.closeValveOnFloodType = 2;
  ASSERT_TRUE(storage.setBlob("1_container",
      reinterpret_cast<char *>(&legacy), sizeof(legacy)));
  ASSERT_TRUE(storage.setBlob("2_valve_cfg",
      reinterpret_cast<char *>(&oldValve), sizeof(oldValve)));
  ASSERT_TRUE(storage.commit());
  // The V2 commit succeeds; only the subsequent legacy erase commit fails.
  storage.failCommitNumber = storage.commits + 2;
  container->onLoadConfig(nullptr);
  ASSERT_GE(storage.getBlobSize("1_cnt_cfg2"), 0);
  ASSERT_EQ(storage.getBlobSize("1_container"), -1);
  const int commits = storage.commits;
  storage.failCommit = true;
  time.advance(60000);
  container->iterateAlways();
  EXPECT_GT(storage.commits, commits);
  storage.failCommit = false;
  time.advance(60000);
  container->iterateAlways();
  storage.reboot();
  EXPECT_EQ(storage.getBlobSize("1_container"), -1);
  ASSERT_GE(storage.getBlobSize("2_valve_cfg"), 0);
  storage.failCommitNumber = storage.commits + 2;
  valve->onLoadConfig(nullptr);
  ASSERT_GE(storage.getBlobSize("2_valve_cfg2"), 0);
  ASSERT_EQ(storage.getBlobSize("2_valve_cfg"), -1);
  const int valveCommits = storage.commits;
  time.advance(60000);
  valve->iterateAlways();
  EXPECT_GT(storage.commits, valveCommits);
  storage.reboot();
  EXPECT_EQ(storage.getBlobSize("2_valve_cfg"), -1);
}

TEST(M4bPersistence, NeverRegisteredZeroMigrationCleanupFailureAndCorruption) {
  Channel::resetToDefaults();
  RegisterDevice::resetToDefaults();
  SimpleTime time;
  DurableConfig storage;
  Storage::SetConfigInstance(&storage);
  Sensor::VirtualBinary binary;
  Sensor::Container container;
  Control::VirtualValve valve;
  Sensor::ContainerConfig legacy;
  legacy.sensorData[0].channelNumber = 0;
  legacy.sensorData[0].fillLevel = 80;
  Control::ValveConfig oldValve;
  oldValve.sensorData[0] = 0;
  oldValve.closeValveOnFloodType = 2;
  ASSERT_TRUE(storage.setBlob("1_container",
      reinterpret_cast<char *>(&legacy), sizeof(legacy)));
  ASSERT_TRUE(storage.setBlob("2_valve_cfg",
      reinterpret_cast<char *>(&oldValve), sizeof(oldValve)));
  ASSERT_TRUE(storage.commit());
  storage.events.clear();
  storage.failErase = true;
  container.onLoadConfig(nullptr);
  valve.onLoadConfig(nullptr);
  EXPECT_EQ(container.sensorReference(0), ChannelReference::local(0));
  EXPECT_EQ(valve.sensorReference(0), ChannelReference::local(0));
  ASSERT_EQ(storage.events[0], "function:1_fnc");
  ASSERT_EQ(storage.events[1], "write:1_cnt_cfg2");
  ASSERT_EQ(storage.events[2], "commit");
  ASSERT_EQ(storage.events[3], "erase:1_container");
  EXPECT_GE(storage.getBlobSize("1_container"), 0);
  storage.reboot();
  legacy.sensorData[0].channelNumber = 7;
  storage.setBlob("1_container", reinterpret_cast<char *>(&legacy),
                   sizeof(legacy));
  container.onLoadConfig(nullptr);
  EXPECT_EQ(container.sensorReference(0), ChannelReference::local(0));
  storage.failErase = false;
  time.advance(60001);
  container.iterateAlways();
  valve.iterateAlways();
  EXPECT_EQ(storage.getBlobSize("1_container"), -1);
  EXPECT_EQ(storage.getBlobSize("2_valve_cfg"), -1);
  Sensor::ContainerStoredConfigV2 record;
  ASSERT_TRUE(storage.getBlob("1_cnt_cfg2",
      reinterpret_cast<char *>(&record), sizeof(record)));
  EXPECT_EQ(sizeof(record), 102);
  EXPECT_EQ(sizeof(Control::ValveStoredConfigV2), 137);
  record.config.sensorData[0].source.kind = 99;
  storage.setBlob("1_cnt_cfg2", reinterpret_cast<char *>(&record),
                   sizeof(record));
  container.onLoadConfig(nullptr);
  EXPECT_FALSE(container.isChannelConfigDurable());
  Storage::SetConfigInstance(nullptr);
}

TEST_F(M4bDevice, CoherenceMatchingFunctionRestoresAndMismatchRejectsLegacy) {
  for (int family = 0; family < 3; ++family) {
    SCOPED_TRACE(family);
    auto request = familyRequest(family);
    if (family == 2) {
      auto wire = reinterpret_cast<TChannelConfig_HVAC *>(request.Config);
      wire->MainThermometerChannelId = 1001;
    }
    auto element = familyElement(family);
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    const auto reference = familyReference(family);
    ASSERT_EQ(reference, ChannelReference::server(1001));
    storage.reboot();
    loadFamily(family);
    EXPECT_EQ(familyReference(family), reference);
    // A present mismatched V2 must not revive this superseded local blob.
    if (family == 0) {
      Sensor::ContainerConfig legacy;
      legacy.sensorData[0].channelNumber = 0;
      legacy.sensorData[0].fillLevel = 80;
      ASSERT_TRUE(storage.setBlob("1_container",
          reinterpret_cast<char *>(&legacy), sizeof(legacy)));
    } else if (family == 1) {
      Control::ValveConfig legacy;
      legacy.sensorData[0] = 0;
      ASSERT_TRUE(storage.setBlob("2_valve_cfg",
          reinterpret_cast<char *>(&legacy), sizeof(legacy)));
    } else {
      TChannelConfig_HVAC legacy;
      hvac->copyFullChannelConfigTo(&legacy);
      legacy.PumpSwitchIsSet = 1;
      legacy.PumpSwitchChannelNo = 3;
      ASSERT_TRUE(storage.setBlob("4_hvac_cfg",
          reinterpret_cast<char *>(&legacy), sizeof(legacy)));
    }
    ASSERT_TRUE(storage.setChannelFunction(element->getChannelNumber(), 0));
    ASSERT_TRUE(storage.commit());
    storage.reboot();
    // HVAC NONE is not a valid internal selection: firmware default stays.
    if (family == 2) {
      hvac->enableDifferentialFunctionSupport();
      hvac->getChannel()->setDefaultFunction(
          SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);
    }
    loadFamily(family);
    EXPECT_EQ(familyReference(family).kind, ChannelReferenceKind::NONE);
    EXPECT_FALSE(element->isChannelConfigDurable());
    EXPECT_GE(storage.getBlobSize(familyKey(family)), 0);
    if (family != 2) {
      EXPECT_EQ(element->getChannel()->getDefaultFunction(), 0);
    }
  }
}

TEST_F(M4bDevice, CoherenceMissingUnsupportedAndZeroMarkerNeverSelectFunction) {
  for (int family = 0; family < 3; ++family) {
    auto element = familyElement(family);
    auto request = familyRequest(family);
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    const auto function = element->getChannel()->getDefaultFunction();
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    storage.generateKey(key, element->getChannelNumber(), "fnc");
    ASSERT_TRUE(storage.eraseKey(key));
    ASSERT_TRUE(storage.commit());
    storage.reboot();
    loadFamily(family);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), function);
    ASSERT_TRUE(storage.setChannelFunction(element->getChannelNumber(), 9999));
    ASSERT_TRUE(storage.commit());
    storage.reboot();
    loadFamily(family);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), function);
    // No family recognizes zero as an absent-marker wildcard.
    const int size = storage.getBlobSize(familyKey(family));
    std::vector<char> bytes(size);
    ASSERT_TRUE(storage.getBlob(familyKey(family), bytes.data(), size));
    const size_t offset = family == 2 ? 2 : 1;
    std::memset(bytes.data() + offset, 0, 4);
    ASSERT_TRUE(storage.setBlob(familyKey(family), bytes.data(), size));
    ASSERT_TRUE(storage.commit());
    storage.reboot();
    loadFamily(family);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), function);
    EXPECT_FALSE(element->isChannelConfigDurable());
  }
}

TEST_F(M4bDevice, CoherenceFunctionChangeCommitsBothAndReplayIsQuiet) {
  for (int family = 0; family < 3; ++family) {
    auto request = familyRequest(family, true);
    auto element = familyElement(family);
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    EXPECT_EQ(storage.getChannelFunction(element->getChannelNumber()),
              request.Func);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), request.Func);
    const auto commits = storage.commits;
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    EXPECT_EQ(storage.commits, commits);
    storage.reboot();
    loadFamily(family);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), request.Func);
    EXPECT_TRUE(element->isChannelConfigDurable());
  }
}

TEST_F(M4bDevice, CoherenceWriteCommitAndReadbackFailuresBlockActivation) {
  for (int family = 0; family < 3; ++family) {
    for (int failure = 0; failure < 5; ++failure) {
      SCOPED_TRACE(family);
      SCOPED_TRACE(failure);
      auto initial = familyRequest(family);
      auto element = familyElement(family);
      ASSERT_EQ(element->handleChannelConfig(&initial, false),
                SUPLA_CONFIG_RESULT_TRUE);
      const auto function = element->getChannel()->getDefaultFunction();
      const auto reference = familyReference(family);
      auto request = familyRequest(family, true);
      storage.failFunction = failure == 0;
      storage.failWrite = failure == 1;
      storage.failCommit = failure == 2;
      storage.failFunctionReadback = failure == 3;
      storage.failBlobReadback = failure == 4;
      EXPECT_EQ(element->handleChannelConfig(&request, false),
                SUPLA_CONFIG_RESULT_DATA_ERROR);
      EXPECT_EQ(element->getChannel()->getDefaultFunction(), function);
      EXPECT_EQ(familyReference(family), reference);
      EXPECT_FALSE(element->isChannelConfigDurable());
      storage.failFunction = storage.failWrite = storage.failCommit = false;
      storage.failFunctionReadback = storage.failBlobReadback = false;
      storage.readbackArmed = false;
      // A retry must confirm durability even when staging matches old bytes.
      ASSERT_EQ(element->handleChannelConfig(&initial, false),
                SUPLA_CONFIG_RESULT_TRUE);
      EXPECT_TRUE(element->isChannelConfigDurable());
      storage.reboot();
      loadFamily(family);
      EXPECT_EQ(element->getChannel()->getDefaultFunction(), function);
    }
  }
}

TEST_F(M4bDevice, CoherenceContainerNoneDoesNotRestoreActiveV2AfterRestart) {
  auto request = containerWire();
  ASSERT_EQ(container->handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  request.Func = 0;
  request.ConfigSize = 0;
  ASSERT_EQ(container->handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  storage.reboot();
  container->onLoadConfig(nullptr);
  EXPECT_EQ(container->getChannel()->getDefaultFunction(), 0);
  EXPECT_EQ(container->sensorReference(0).kind, ChannelReferenceKind::NONE);
}

TEST_F(M4bDevice, CoherenceHvacEmptyFunctionChangeHasOneRequiredCommit) {
  TSD_ChannelConfig none = {};
  ASSERT_EQ(hvac->handleChannelConfig(&none, false), SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_TRUE(hvac->setOutputValueOnError(50));
  hvac->saveConfig();
  static_cast<FollowupHvac *>(hvac)->clearPending();
  auto request = familyRequest(2, true);
  const auto commits = storage.commits;
  storage.failCommitNumber = commits + 2;
  ASSERT_EQ(hvac->handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(storage.commits, commits + 1);
  EXPECT_EQ(hvac->supLanFunction(), request.Func);
  storage.reboot();
  hvac->onLoadConfig(nullptr);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(), request.Func);
  EXPECT_EQ(hvac->supLanFunction(), request.Func);
}


TEST_F(M4bDevice, CoherenceLocalOfflineChangesSurviveRestart) {
  identity.disconnected();
  ASSERT_TRUE(container->setRuntimeFunction(SUPLA_CHANNELFNC_SEPTIC_TANK));
  ASSERT_TRUE(container->setSensorSlot(0, 0, 80));
  container->saveConfig();
  ASSERT_TRUE(valve->addSensor(0));
  valve->saveConfig();
  hvac->enableDifferentialFunctionSupport();
  hvac->changeFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL, true);
  Sensor::VirtualThermometer thermometer;
  ASSERT_TRUE(hvac->setMainThermometerChannelNo(
      thermometer.getChannelNumber()));
  hvac->saveConfig();
  storage.reboot();
  container->getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_CONTAINER);
  hvac->getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  container->onLoadConfig(nullptr);
  valve->onLoadConfig(nullptr);
  hvac->onLoadConfig(nullptr);
  EXPECT_EQ(container->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_SEPTIC_TANK);
  EXPECT_EQ(container->sensorReference(0), ChannelReference::local(0));
  EXPECT_EQ(valve->sensorReference(0), ChannelReference::local(0));
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);
}

TEST_F(M4bDevice, CoherenceAllFamiliesBlockDeviceSyncDoneOnFunctionWriteError) {
  SuplaDeviceClass device;
  CoherenceSrpc srpc(&device);
  int handle = 0;
  srpc.prepare(&handle);
  auto &syncIdentity = srpc.serverIdentity();
  syncIdentity.load(&storage);
  ASSERT_TRUE(syncIdentity.registrationStarted());
  syncIdentity.registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 202;
  ids.ChannelCount = 5;
  for (int family = 0; family < 3; ++family) {
    for (int i = 0; i < 5; ++i) ids.ChannelId[i] = 701 + family * 100 + i;
    ASSERT_EQ(syncIdentity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
    ASSERT_TRUE(syncIdentity.identityTransition());
    auto element = familyElement(family);
    element->onServerIdentityTransition();
    auto request = familyRequest(family, true);
    storage.failFunction = true;
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_DATA_ERROR);
    srpc.onDeviceSyncDone();
    EXPECT_TRUE(syncIdentity.identityTransition());
    EXPECT_FALSE(syncIdentity.serverSyncComplete());
    storage.failFunction = false;
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    srpc.onDeviceSyncDone();
    EXPECT_FALSE(syncIdentity.identityTransition());
    EXPECT_TRUE(syncIdentity.serverSyncComplete());
  }
}


TEST_F(M4bDevice, CoherenceColdHvacMissingFunctionUsesFirmwareSelection) {
  auto request = bindingConfig(1001, 0);
  ASSERT_EQ(hvac->handleChannelConfig(&request, false),
            SUPLA_CONFIG_RESULT_TRUE);
  ASSERT_TRUE(storage.eraseKey("4_fnc"));
  ASSERT_TRUE(storage.commit());
  delete hvac;
  storage.reboot();
  hvac = new FollowupHvac(&output);
  ASSERT_EQ(hvac->getChannel()->getDefaultFunction(), 0);
  hvac->onLoadConfig(nullptr);
  EXPECT_EQ(hvac->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  EXPECT_EQ(hvac->bindingTarget(0), ChannelReference::server(1001));
  hvac->onInit();
  EXPECT_EQ(hvac->bindingTarget(0), ChannelReference::server(1001));
}


TEST_F(M4bDevice, CoherenceNewMarkerWithOldFunctionFailsClosedAfterCrash) {
  for (int family = 0; family < 3; ++family) {
    auto element = familyElement(family);
    const auto oldFunction = element->getChannel()->getDefaultFunction();
    auto request = familyRequest(family, true);
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    // Emulate a multi-key power cut exposing the new blob but old selector.
    ASSERT_TRUE(storage.setChannelFunction(element->getChannelNumber(),
                                          oldFunction));
    ASSERT_TRUE(storage.commit());
    storage.reboot();
    loadFamily(family);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), oldFunction);
    EXPECT_FALSE(element->isChannelConfigDurable());
    EXPECT_EQ(familyReference(family).kind, ChannelReferenceKind::NONE);
  }
}

TEST_F(M4bDevice, CoherenceFullActiveFunctionReplacementSurvivesReboot) {
  for (int family : {0, 2}) {
    auto request = familyRequest(family);
    auto element = familyElement(family);
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    request.Func = familyRequest(family, true).Func;
    if (family == 0) {
      auto wire = reinterpret_cast<TChannelConfig_Container *>(request.Config);
      wire->WarningAboveLevel = 90;
    } else {
      auto wire = reinterpret_cast<TChannelConfig_HVAC *>(request.Config);
      wire->OutputValueOnError = 50;
    }
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    storage.reboot();
    loadFamily(family);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), request.Func);
    EXPECT_EQ(familyReference(family), ChannelReference::server(1001));
    EXPECT_TRUE(element->isChannelConfigDurable());
    if (family == 2) {
      EXPECT_EQ(storedHvac().scalars.OutputValueOnError, 50);
    }
  }
}

TEST_F(M4bDevice, CoherenceReadbackErrorCanRebootIntoNewCompleteState) {
  for (int family = 0; family < 3; ++family) {
    auto element = familyElement(family);
    const auto before = element->getChannel()->getDefaultFunction();
    auto request = familyRequest(family, true);
    storage.failBlobReadback = true;
    EXPECT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_DATA_ERROR);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), before);
    storage.failBlobReadback = false;
    storage.readbackArmed = false;
    // The commit succeeded: rollback of staging cannot undo durable storage.
    storage.reboot();
    loadFamily(family);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), request.Func);
    EXPECT_TRUE(element->isChannelConfigDurable());
  }
}


TEST_F(M4bDevice, CoherenceStagedLocalFunctionCannotMakeReplayDurable) {
  for (int family = 0; family < 3; ++family) {
    auto element = familyElement(family);
    const auto original = element->getChannel()->getDefaultFunction();
    auto request = familyRequest(family, true);
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    // Local selector changes independently; the old V2 must fail closed.
    ASSERT_TRUE(storage.setChannelFunction(element->getChannelNumber(),
                                          original));
    ASSERT_TRUE(element->setRuntimeFunction(original));
    ASSERT_TRUE(storage.commit());
    // UI stages a return to the same function as the existing V2 marker.
    ASSERT_TRUE(storage.setChannelFunction(element->getChannelNumber(),
                                          request.Func));
    ASSERT_TRUE(element->setRuntimeFunction(request.Func));
    const auto commits = storage.commits;
    storage.failCommit = true;
    EXPECT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_DATA_ERROR);
    EXPECT_FALSE(element->isChannelConfigDurable());
    storage.failCommit = false;
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    EXPECT_GE(storage.commits, commits + 2);
    storage.reboot();
    loadFamily(family);
    EXPECT_EQ(element->getChannel()->getDefaultFunction(), request.Func);
    EXPECT_TRUE(element->isChannelConfigDurable());
  }
}


TEST_F(M4bDevice, CoherenceMalformedVersionAndLengthRejectWholeRecord) {
  for (int family = 0; family < 3; ++family) {
    auto element = familyElement(family);
    auto request = familyRequest(family);
    ASSERT_EQ(element->handleChannelConfig(&request, false),
              SUPLA_CONFIG_RESULT_TRUE);
    const auto function = element->getChannel()->getDefaultFunction();
    const int size = storage.getBlobSize(familyKey(family));
    std::vector<char> original(size);
    ASSERT_TRUE(storage.getBlob(familyKey(family), original.data(), size));
    for (bool badVersion : {true, false}) {
      auto bytes = original;
      if (badVersion) bytes[0] = 99;
      ASSERT_TRUE(storage.setBlob(familyKey(family), bytes.data(),
                                   badVersion ? size : size - 1));
      ASSERT_TRUE(storage.commit());
      storage.reboot();
      loadFamily(family);
      EXPECT_EQ(element->getChannel()->getDefaultFunction(), function);
      EXPECT_EQ(familyReference(family).kind, ChannelReferenceKind::NONE);
      EXPECT_FALSE(element->isChannelConfigDurable());
    }
  }
}

}  // namespace
