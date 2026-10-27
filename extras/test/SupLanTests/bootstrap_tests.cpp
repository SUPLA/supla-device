// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <SuplaDevice.h>
#include <supla/actions.h>
#include <crypto_test_hooks.h>
#include <board_mock.h>
#include <supla/device/factory_test.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <output_mock.h>
#include <supla/channels/channel.h>
#include <supla/correction.h>
#include <supla/control/hvac_base.h>
#include <supla/control/virtual_relay.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/clock/clock.h>
#include <supla/device/register_device.h>
#include <supla/network/network.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/key_value.h>
#include <supla/suplan/owned_stack.h>
#include <suplan_bootstrap_port.h>
#include <suplan/suplan_wire.h>
#include <timer_mock.h>
#include <storage_mock.h>
#include <array>
#include <cstdio>
#include <vector>
#include "suplan_crypto_openssl.h"

namespace {
using namespace Supla::Device;  // NOLINT(build/namespaces)
using Stack = OwnedSupLanStack<Supla::SupLan::OpenSslCryptoPort, BootstrapUdp>;
int creates;
SupLanStack *makeStack(SuplaDeviceClass *device, uint8_t version) {
  ++creates;
  auto stack = allocateSupLan<Stack>(device, version);
  if (stack && !stack->initialize()) {
    delete stack;
    return nullptr;
  }
  return stack;
}
Supla::Protocol::SupLan *protocolFor(SuplaDeviceClass *device) {
  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto;
       proto = proto->next()) {
    if (proto->getSdc() == device && proto->isSupLan())
      return static_cast<Supla::Protocol::SupLan *>(proto);
  }
  return nullptr;
}
class BootConfig : public Supla::KeyValue {
 public:
  bool init() override { return true; }
  bool commit() override {
    ++commitAttempts;
    if (failCommit) return false;
    std::array<uint8_t, 16384> bytes = {};
    const auto size = serializeToMemory(bytes.data(), bytes.size());
    if (size == SIZE_MAX) return false;
    disk.assign(bytes.begin(), bytes.begin() + size);
    return true;
  }
  void removeAll() override {
    if (!failErase) KeyValue::removeAll();
  }
  bool removeAllChecked() override {
    ++eraseAttempts;
    if (transientEraseFailures > 0) {
      --transientEraseFailures;
      return false;
    }
    return !failErase && KeyValue::removeAllChecked();
  }
  bool isDeviceDataPartitionAvailable() override { return fixedCredentials; }
  bool setGUID(const char *value) override {
    return !fixedCredentials && Config::setGUID(value);
  }
  bool setAuthKey(const char *value) override {
    return !fixedCredentials && Config::setAuthKey(value);
  }
  bool getGUID(char *value) override {
    if (!fixedCredentials) return Config::getGUID(value);
    const char guid[SUPLA_GUID_SIZE] = {1};
    memcpy(value, guid, sizeof(guid));
    return true;
  }
  bool getAuthKey(char *value) override {
    if (!fixedCredentials) return Config::getAuthKey(value);
    const char auth[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(value, auth, sizeof(auth));
    return true;
  }
  void reboot() {
    removeAllMemory();
    if (!disk.empty()) {
      ASSERT_TRUE(initFromMemory(disk.data(), disk.size()));
    }
  }
  int commitAttempts = 0;
  int eraseAttempts = 0;
  int transientEraseFailures = 0;
  bool failErase = false;
  bool failCommit = false;
  bool fixedCredentials = false;
  std::vector<uint8_t> disk;
};
class BootNetwork : public Supla::Network {
 public:
  void setup() override {}
  void disable() override { ready = false; }
  bool isReady() override { return ready; }
  uint32_t getIP() override { return ip; }
  bool getMacAddr(uint8_t *) override { return false; }
  bool ready = false;
  uint32_t ip = 0;
};
class SupLanBootstrap : public testing::Test {
 protected:
  void SetUp() override {
    if (SuplaDevice.getClock()) delete SuplaDevice.getClock();
    Supla::RegisterDevice::resetToDefaults();
    Supla::Channel::resetToDefaults();
    Supla::Correction::clear();
    char guid[SUPLA_GUID_SIZE] = {1};
    char auth[SUPLA_AUTHKEY_SIZE] = {2};
    ASSERT_TRUE(config.setGUID(guid));
    ASSERT_TRUE(config.setAuthKey(auth));
    ASSERT_TRUE(config.setSuplaServer("supla.example"));
    ASSERT_TRUE(config.setEmail("user@example.com"));
    creates = 0;
    failSupLanAllocationAfter(-1);
    setSupLanFactoryForTest(makeStack);
  }
  void TearDown() override {
    setSupLanFactoryForTest(nullptr);
    failSupLanAllocationAfter(-1);
    if (SuplaDevice.getClock()) delete SuplaDevice.getClock();
    Supla::RegisterDevice::resetToDefaults();
    Supla::Channel::resetToDefaults();
    Supla::Correction::clear();
  }
  SimpleTime time;
  ScopedCryptoTestState crypto{83};
  testing::NiceMock<TimerMock> timer;
  BootConfig config;
  BootNetwork network;
};
TEST_F(SupLanBootstrap, DisabledBothVersionsDoNotConstructOrAdvertise) {
  for (uint8_t version : {23, 29}) {
    SuplaDeviceClass device;
    EXPECT_TRUE(device.begin(version));
    EXPECT_EQ(protocolFor(&device), nullptr);
    EXPECT_FALSE(hasSupLanServerStack(&device));
  }
  EXPECT_EQ(creates, 0);
}
TEST_F(SupLanBootstrap, DisabledIdentityKeepsLegacyWireAndLocalConfig) {
  Supla::Sensor::VirtualThermometer thermometer;
  OutputSimulator output;
  Supla::Control::HvacBase hvac(&output);
  SuplaDeviceClass device;
  ASSERT_TRUE(device.begin(29));
  hvac.setMainThermometerChannelNo(thermometer.getChannelNumber());
  hvac.setAuxThermometerChannelNo(hvac.getChannelNumber());
  hvac.setBinarySensorChannelNo(hvac.getChannelNumber());
  auto identity = &device.getSrpcLayer()->serverIdentity();
  ASSERT_TRUE(identity->registrationStarted());
  identity->registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 202;
  ids.ChannelCount = 2;
  ids.ChannelId[0] = 501;
  ids.ChannelId[1] = 601;
  ASSERT_EQ(identity->accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  identity->syncDone();
  ASSERT_TRUE(identity->capable());
  EXPECT_FALSE(Supla::usesServerReferences());
  TChannelConfig_HVAC wire = {};
  int size = 0;
  hvac.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  ASSERT_EQ(size, sizeof(wire));
  EXPECT_EQ(wire.MainThermometerChannelNo, 0);
  uint32_t id = 0;
  ASSERT_TRUE(Supla::referenceWireId(
      Supla::ChannelReference::server(501), &id));
  EXPECT_EQ(id, 0u);  // Legacy wire uses legal local ChannelNumber zero.
  TSD_ChannelConfig incoming = {};
  incoming.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  incoming.ConfigSize = sizeof(wire);
  memcpy(incoming.Config, &wire, sizeof(wire));
  EXPECT_EQ(hvac.handleChannelConfig(&incoming, false),
            SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac.getMainThermometerChannelNo(), 0);
}
TEST_F(SupLanBootstrap, EnabledIdentityMaterializesLocalReferenceAsServerId) {
  Supla::Sensor::VirtualThermometer thermometer;
  OutputSimulator output;
  Supla::Control::HvacBase hvac(&output);
  SuplaDeviceClass device;
  ASSERT_TRUE(device.enableSupLan());
  ASSERT_TRUE(device.begin(29));
  hvac.setMainThermometerChannelNo(thermometer.getChannelNumber());
  hvac.setAuxThermometerChannelNo(hvac.getChannelNumber());
  hvac.setBinarySensorChannelNo(hvac.getChannelNumber());
  auto identity = &device.getSrpcLayer()->serverIdentity();
  ASSERT_TRUE(identity->registrationStarted());
  identity->registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 202;
  ids.ChannelCount = 2;
  ids.ChannelId[0] = 501;
  ids.ChannelId[1] = 601;
  ASSERT_EQ(identity->accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  identity->syncDone();
  EXPECT_TRUE(Supla::usesServerReferences());
  TChannelConfig_HVAC wire = {};
  int size = 0;
  hvac.fillChannelConfig(&wire, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  ASSERT_EQ(size, sizeof(wire));
  EXPECT_EQ(wire.MainThermometerChannelId, 501);
}
TEST_F(SupLanBootstrap, EnabledBeforeLoadAndInitWithoutIpAndDoubleEnable) {
  SuplaDeviceClass device;
  EXPECT_TRUE(device.enableSupLan());
  EXPECT_TRUE(device.enableSupLan());
  EXPECT_EQ(creates, 0);
  ASSERT_TRUE(device.begin(29));
  EXPECT_EQ(creates, 1);
  auto protocol = protocolFor(&device);
  ASSERT_NE(protocol, nullptr);
  EXPECT_TRUE(protocol->isSupLanServerCapable());
  // Associations were loaded by normal onInit, with the restored SRPC identity.
  ASSERT_NE(protocol->remoteResources()->associations, nullptr);
  EXPECT_TRUE(protocol->remoteResources()->associations->available());
  EXPECT_EQ(protocol->remoteResources()->associations->identity(),
            &device.getSrpcLayer()->serverIdentity());
  EXPECT_FALSE(protocol->isTransportOpen());
  EXPECT_FALSE(device.enableSupLan());
  EXPECT_FALSE(device.begin(29));
  EXPECT_EQ(creates, 1);
}
TEST_F(SupLanBootstrap, InvalidVersionDoesNotAllocateStack) {
  for (uint8_t version : {23, 28, 30, 255}) {
    SuplaDeviceClass device;
    ASSERT_TRUE(device.enableSupLan());
    EXPECT_FALSE(device.begin(version));
    EXPECT_FALSE(hasSupLanServerStack(&device));
  }
  EXPECT_EQ(creates, 0);
}
TEST_F(SupLanBootstrap, UnsupportedHostIsExplicitAndNoAutomaticStack) {
  setSupLanFactoryForTest(nullptr);
  SuplaDeviceClass device;
  EXPECT_FALSE(device.enableSupLan());
  EXPECT_TRUE(device.begin(29));
  EXPECT_EQ(creates, 0);
  EXPECT_FALSE(hasSupLanServerStack(&device));
}
TEST_F(SupLanBootstrap, EveryAllocationFailureRollsBackRegisteredLayer) {
  // Request, owned tables, ProtocolLayer, associations, resource and bindings.
  for (int allowed = 0; allowed < 6; ++allowed) {
    SuplaDeviceClass device;
    failSupLanAllocationAfter(allowed);
    if (!allowed) {
      EXPECT_FALSE(device.enableSupLan());
    } else {
      ASSERT_TRUE(device.enableSupLan());
      EXPECT_FALSE(device.begin(29)) << allowed;
    }
    EXPECT_EQ(protocolFor(&device), nullptr) << allowed;
    EXPECT_FALSE(hasSupLanServerStack(&device)) << allowed;
  }
}
TEST_F(SupLanBootstrap, FailedRandomInitializationCannotAdvertise) {
  SuplaDeviceClass device;
  ASSERT_TRUE(device.enableSupLan());
  crypto.failRandom = true;
  EXPECT_FALSE(device.begin(29));
  EXPECT_FALSE(hasSupLanServerStack(&device));
  EXPECT_EQ(protocolFor(&device), nullptr);
}
TEST_F(SupLanBootstrap, UnsupportedCipherCannotLeaveCapableStack) {
  class RejectCrypto : public Supla::SupLan::OpenSslCryptoPort {
   public:
    bool aes128CcmEncrypt(const uint8_t[16], const uint8_t[12],
                          const uint8_t *, size_t, const uint8_t *, size_t,
                          uint8_t *, uint8_t[16]) override { return false; }
  };
  SuplaDeviceClass device;
  {
    OwnedSupLanStack<RejectCrypto, BootstrapUdp> stack(&device, 29);
    EXPECT_FALSE(stack.initialize());
    EXPECT_FALSE(stack.ready());
    EXPECT_FALSE(hasSupLanServerStack(&device));
  }
  EXPECT_EQ(protocolFor(&device), nullptr);
}
TEST_F(SupLanBootstrap, ManualCompleteStackTakesPrecedenceAndStaysOwned) {
  auto device = new SuplaDeviceClass;
  Stack manual(device, 29);
  ASSERT_TRUE(manual.initialize());
  auto protocol = protocolFor(device);
  ASSERT_NE(protocol, nullptr);
  ASSERT_TRUE(device->enableSupLan());
  ASSERT_TRUE(device->begin(29));
  EXPECT_EQ(creates, 0);
  EXPECT_EQ(protocolFor(device), protocol);
  delete device;
  EXPECT_EQ(protocol->getSdc(), device);  // SDK did not reparent/delete it.
  EXPECT_TRUE(protocol->isSupLanServerCapable());
}
TEST_F(SupLanBootstrap, ManualRegisteredAfterEnableDoesNotDuplicate) {
  SuplaDeviceClass device;
  ASSERT_TRUE(device.enableSupLan());
  Stack manual(&device, 29);
  ASSERT_TRUE(manual.initialize());
  ASSERT_TRUE(device.begin(29));
  EXPECT_EQ(creates, 0);
}
TEST_F(SupLanBootstrap, IncompleteOrDuplicateManualStackFailsExplicitly) {
  SuplaDeviceClass device;
  Supla::SupLan::PeerTable peers;
  Supla::Protocol::SupLan incomplete(&device, &peers);
  EXPECT_FALSE(device.enableSupLan());
  Stack manual(&device, 29);
  ASSERT_TRUE(manual.initialize());
  EXPECT_FALSE(device.enableSupLan());
  EXPECT_FALSE(hasSupLanServerStack(&device));
}
TEST_F(SupLanBootstrap, ConflictIntroducedAfterEnableCannotStartAuto) {
  SuplaDeviceClass device;
  ASSERT_TRUE(device.enableSupLan());
  Supla::SupLan::PeerTable peers;
  Supla::Protocol::SupLan incomplete(&device, &peers);
  EXPECT_FALSE(device.begin(29));
  EXPECT_EQ(creates, 0);
}
TEST_F(SupLanBootstrap, DeviceDestructionRemovesOnlyOwnedLayer) {
  auto before = Supla::Protocol::ProtocolLayer::last();
  {
    SuplaDeviceClass device;
    ASSERT_TRUE(device.enableSupLan());
    ASSERT_TRUE(device.begin(29));
    ASSERT_NE(protocolFor(&device), nullptr);
  }
  EXPECT_EQ(Supla::Protocol::ProtocolLayer::last(), before);
}
TEST_F(SupLanBootstrap, NetworkFlapAndAddressChangeWithoutSrpc) {
  BootstrapUdp udp;
  EXPECT_FALSE(udp.networkReady(0));
  EXPECT_EQ(udp.attempts, 0);
  network.ready = true;
  EXPECT_FALSE(udp.networkReady(0));  // No IP yet.
  network.ip = 0x2800a8c0;
  ASSERT_TRUE(udp.networkReady(0));
  ASSERT_TRUE(udp.openIfDue(0));
  network.ip = 0x2900a8c0;
  EXPECT_TRUE(udp.networkReady(1));
  EXPECT_FALSE(udp.isOpen());
  EXPECT_EQ(udp.closes, 1);
  EXPECT_FALSE(udp.openIfDue(1));  // Existing retry throttle.
  EXPECT_TRUE(udp.openIfDue(1000));
  network.ready = false;
  EXPECT_FALSE(udp.networkReady(1001));
}
TEST_F(SupLanBootstrap, PortConflictIsThrottledAndRecoversWithoutBegin) {
  BootstrapUdp udp;
  network.ready = true;
  network.ip = 0x2800a8c0;
  ASSERT_TRUE(udp.networkReady(0));
  udp.failOpen = true;
  EXPECT_FALSE(udp.openIfDue(0));
  EXPECT_FALSE(udp.openIfDue(999));
  EXPECT_EQ(udp.attempts, 1);
  udp.failOpen = false;
  EXPECT_TRUE(udp.openIfDue(1000));
  EXPECT_EQ(udp.attempts, 2);
}
TEST_F(SupLanBootstrap, EthernetReadyDoesNotNeedWifiOrServer) {
  BootNetwork ethernet;
  BootstrapUdp udp;
  ethernet.ready = true;
  ethernet.ip = 0x2a00a8c0;
  EXPECT_TRUE(udp.networkReady(0));
  EXPECT_TRUE(udp.openIfDue(0));
}
TEST_F(SupLanBootstrap, FactoryResetClearsRuntimeAuthorization) {
  SuplaDeviceClass device;
  ASSERT_TRUE(device.enableSupLan());
  ASSERT_TRUE(device.begin(29));
  auto protocol = protocolFor(&device);
  ASSERT_NE(protocol, nullptr);
  auto identity = &device.getSrpcLayer()->serverIdentity();
  ASSERT_TRUE(identity->registrationStarted());
  identity->registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 202;
  ASSERT_EQ(identity->accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  identity->syncDone();
  TSDS_SuplaSetSuplanDestinationAssociation request = {};
  request.PeerContext = {1, 0, 1, 101, 1, 202, 1, 1};
  request.AclRevision = 1;
  request.PeerKeySize = 32;
  request.PeerKey[0] = 42;
  request.ResourceCount = 1;
  request.Resources[0] = {1, 501, 1};
  TDS_SuplaSetSuplanDestinationAssociationResult result = {};
  ASSERT_TRUE(protocol->setSuplanDestinationAssociation(request, &result));
  ASSERT_EQ(result.Result, SUPLA_SUPLAN_RESULT_OK);
  Supla::ChannelHandle handle(501, Supla::READ);
  Supla::ChannelHandle::iterateAll();
  ASSERT_TRUE(handle.hasReadPermission());
  device.resetToFactorySettings();
  EXPECT_TRUE(protocol->isSupLanServerCapable());
  EXPECT_FALSE(protocol->isTransportOpen());
  EXPECT_FALSE(protocol->remoteResources()->associations->identity()
                   ->identityAvailable());
  EXPECT_EQ(protocol->remoteResources()->effectivePermissions(
      {Supla::SupLan::kResourceTypeChannel, 501}), 0);
  EXPECT_FALSE(handle.hasReadPermission());
  EXPECT_FALSE(handle.state().available());
}
TEST_F(SupLanBootstrap, FailedResetReportsFailureAndBlocksReactivation) {
  for (bool eraseFailure : {false, true}) {
    SuplaDeviceClass device;
    ASSERT_TRUE(device.enableSupLan());
    ASSERT_TRUE(device.begin(29));
    auto identity = &device.getSrpcLayer()->serverIdentity();
    ASSERT_TRUE(identity->registrationStarted());
    identity->registrationSucceeded();
    TSD_SuplaDeviceIdentities ids = {};
    ids.DeviceId = 202;
    ASSERT_EQ(identity->accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
    ASSERT_NE(identity->rootEpoch(), 0U);
    config.failErase = eraseFailure;
    config.failCommit = !eraseFailure;
    EXPECT_FALSE(device.resetToFactorySettingsChecked());
    EXPECT_EQ(identity->rootEpoch(), 0U);
    EXPECT_FALSE(identity->identityAvailable());
    ASSERT_TRUE(identity->registrationStarted());
    identity->registrationSucceeded();
    EXPECT_NE(identity->accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
    config.failErase = config.failCommit = false;
    // Failed erase/commit cannot guarantee deletion of flash. It must be
    // reported as failure. A subsequent reboot may restore the old state.
    config.reboot();
    identity->load(&config);
    EXPECT_NE(identity->rootEpoch(), 0U);
    ASSERT_TRUE(device.resetToFactorySettingsChecked());
  }
}
TEST_F(SupLanBootstrap, PopulatedResetDurablyRemovesAllSupLanAuthorization) {
  Supla::Sensor::VirtualThermometer thermometer;
  Supla::Control::VirtualRelay relay;
  relay.getChannel()->setDefault(SUPLA_CHANNELFNC_PUMPSWITCH);
  SuplaDeviceClass device;
  ASSERT_TRUE(device.enableSupLan());
  ASSERT_TRUE(device.begin(29));
  auto protocol = protocolFor(&device);
  auto identity = &device.getSrpcLayer()->serverIdentity();
  ASSERT_TRUE(identity->registrationStarted());
  identity->registrationSucceeded();
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 202;
  ids.ChannelCount = 2;
  ids.ChannelId[0] = 601;
  ids.ChannelId[1] = 602;
  ASSERT_EQ(identity->accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
  identity->syncDone();
  TSDS_SuplaSetSuplanDestinationAssociation destination = {};
  destination.PeerContext = {1, 0, 1, 101, 1, 202, 123, 1};
  destination.AclRevision = 1;
  destination.PeerKeySize = 32;
  destination.PeerKey[0] = 42;
  destination.ResourceCount = 1;
  destination.Resources[0] = {1, 501, 1};
  TDS_SuplaSetSuplanDestinationAssociationResult installed = {};
  ASSERT_TRUE(protocol->setSuplanDestinationAssociation(destination,
                                                        &installed));
  ASSERT_EQ(installed.Result, SUPLA_SUPLAN_RESULT_OK);
  TSDS_SuplaSetSuplanSourceAssociation source = {};
  source.PeerContext = {1, 0, 1, 202, 1, 303, identity->rootEpoch(), 1};
  source.AclRevision = 1;
  source.AclEntryCount = 1;
  source.Acl[0] = {1, 601, 1};
  TDS_SuplaSetSuplanSourceAssociationResult granted = {};
  ASSERT_TRUE(protocol->setSuplanSourceAssociation(source, &granted));
  ASSERT_EQ(granted.Result, SUPLA_SUPLAN_RESULT_OK);
  uint8_t binding[13] = {1, 1, 1};
  Supla::SupLan::putUint32(binding + 3, 501);
  binding[7] = 1;
  Supla::SupLan::putUint32(binding + 8, 602);
  ASSERT_EQ(protocol->resourceBindings()->accept(0, binding, sizeof(binding)),
            3);
  ASSERT_EQ(protocol->resourceBindings()->bindingCount(), 1);
  ASSERT_TRUE(device.resetToFactorySettingsChecked());
  EXPECT_EQ(protocol->resourceBindings()->bindingCount(), 0);
  config.reboot();
  identity->load(&config);
  EXPECT_EQ(identity->rootEpoch(), 0U);
  EXPECT_FALSE(identity->identityAvailable());
  Supla::Config &cfg = config;
  EXPECT_LT(cfg.getBlobSize("sl-root"), 0);
  EXPECT_LT(cfg.getBlobSize("sl-identity"), 0);
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "sl-peer-%d", i);
    EXPECT_LT(cfg.getBlobSize(key), 0);
    snprintf(key, sizeof(key), "slb_%d", i);
    EXPECT_LT(cfg.getBlobSize(key), 0);
  }
  char guid[SUPLA_GUID_SIZE], auth[SUPLA_AUTHKEY_SIZE];
  EXPECT_TRUE(config.getGUID(guid));
  EXPECT_TRUE(config.getAuthKey(auth));
  EXPECT_EQ(memcmp(guid, Supla::RegisterDevice::getGUID(), sizeof(guid)), 0);
  EXPECT_EQ(memcmp(auth, Supla::RegisterDevice::getAuthKey(), sizeof(auth)), 0);
}
TEST_F(SupLanBootstrap, ResetPreservesReadonlyDevicePartitionCredentials) {
  config.fixedCredentials = true;
  SuplaDeviceClass device;
  ASSERT_TRUE(device.enableSupLan());
  ASSERT_TRUE(device.begin(29));
  EXPECT_TRUE(device.resetToFactorySettingsChecked());
  config.reboot();
  char guid[SUPLA_GUID_SIZE], auth[SUPLA_AUTHKEY_SIZE];
  EXPECT_TRUE(config.getGUID(guid));
  EXPECT_TRUE(config.getAuthKey(auth));
  EXPECT_EQ(memcmp(guid, Supla::RegisterDevice::getGUID(), sizeof(guid)), 0);
  EXPECT_EQ(memcmp(auth, Supla::RegisterDevice::getAuthKey(), sizeof(auth)), 0);
}
class ResetPolicyDevice : public SuplaDeviceClass {
 public:
  void processReset() { handleLocalActionTriggers(); }
  void processScheduledRestart() { checkIfLeaveCfgModeOrRestartIsNeeded(); }
  uint32_t scheduledRestart() const { return forceRestartTimeMs; }
};
TEST_F(SupLanBootstrap, TransientResetFailureRetriesOnceAndSucceeds) {
  SuplaDeviceClass device;
  ASSERT_TRUE(device.begin(29));
  config.transientEraseFailures = 1;
  EXPECT_TRUE(device.resetToFactorySettingsChecked());
  EXPECT_EQ(config.eraseAttempts, 2);
  EXPECT_LT(static_cast<Supla::Config &>(config).getBlobSize("sl-root"), 0);
}
TEST_F(SupLanBootstrap, SuccessfulResetDoesNotRetry) {
  SuplaDeviceClass device;
  ASSERT_TRUE(device.begin(29));
  EXPECT_TRUE(device.resetToFactorySettingsChecked());
  EXPECT_EQ(config.eraseAttempts, 1);
}
TEST_F(SupLanBootstrap, FailedButtonResetRestartsWithoutConfigMode) {
  ResetPolicyDevice device;
  ASSERT_TRUE(device.enableSupLan());
  ASSERT_TRUE(device.begin(29));
  config.failErase = true;
  testing::StrictMock<BoardMock> board;
  EXPECT_CALL(board, deviceSoftwareReset()).Times(1);
  device.handleAction(0, Supla::RESET_TO_FACTORY_SETTINGS);
  device.processReset();
  EXPECT_EQ(config.eraseAttempts, 2);
  EXPECT_EQ(device.getDeviceMode(), Supla::DEVICE_MODE_NORMAL);
  EXPECT_FALSE(device.getSrpcLayer()->serverIdentity().identityAvailable());
}
TEST_F(SupLanBootstrap, FailedFactoryTestResetReportsFailureAndRestarts) {
  ResetPolicyDevice device;
  ASSERT_TRUE(device.begin(29));
  Supla::Device::FactoryTest factory(&device, 30);
  factory.setTestFinished();
  config.failErase = true;
  factory.iterateAlways();
  EXPECT_TRUE(factory.hasFailed());
  EXPECT_EQ(factory.getFailReason(), 22);
  EXPECT_EQ(config.eraseAttempts, 2);
  EXPECT_EQ(device.scheduledRestart(), 1000U);
  factory.iterateAlways();
  EXPECT_EQ(config.eraseAttempts, 2);
}
TEST_F(SupLanBootstrap, TransientFactoryTestResetFailureStillPasses) {
  ResetPolicyDevice device;
  ASSERT_TRUE(device.begin(29));
  Supla::Device::FactoryTest factory(&device, 30);
  factory.setTestFinished();
  config.transientEraseFailures = 1;
  factory.iterateAlways();
  EXPECT_FALSE(factory.hasFailed());
  EXPECT_EQ(config.eraseAttempts, 2);
  EXPECT_EQ(device.scheduledRestart(), 1000U);
}
class SaveCountingRelay : public Supla::Control::VirtualRelay {
 public:
  void onSaveState() override {
    ++saves;
    VirtualRelay::onSaveState();
  }
  int saves = 0;
};
TEST_F(SupLanBootstrap, FactoryTestRestartSkipsPreResetStateAndConfigWrites) {
  for (bool failure : {false, true}) {
    testing::NiceMock<StorageMockSimulator> stateStorage;
    SaveCountingRelay relay;
    ResetPolicyDevice device;
    ASSERT_TRUE(device.begin(29));
    relay.turnOn();
    Supla::Device::FactoryTest factory(&device, 30);
    factory.setTestFinished();
    config.failErase = failure;
    factory.iterateAlways();
    EXPECT_EQ(factory.hasFailed(), failure);
    const int saves = relay.saves;
    const int commits = config.commitAttempts;
    const int erases = config.eraseAttempts;
    // Periodic saving must also stay suppressed during the restart delay.
    device.saveStateToStorage();
    time.advance(1001);
    testing::StrictMock<BoardMock> board;
    EXPECT_CALL(board, deviceSoftwareReset()).Times(1);
    device.processScheduledRestart();
    EXPECT_EQ(relay.saves, saves);
    EXPECT_EQ(config.commitAttempts, commits);
    EXPECT_EQ(config.eraseAttempts, erases);
    EXPECT_EQ(device.getDeviceMode(), Supla::DEVICE_MODE_NORMAL);
    config.failErase = false;
  }
}
// Exercise the default Config compatibility path, not KV's checked override.
class LegacyBootConfig : public BootConfig {
 public:
  bool removeAllChecked() override {
    return Supla::Config::removeAllChecked();
  }
};
TEST_F(SupLanBootstrap, LegacyResetPreservesDurableGuidAndAuthKey) {
  LegacyBootConfig legacy;
  char guid[SUPLA_GUID_SIZE] = {7};
  char auth[SUPLA_AUTHKEY_SIZE] = {9};
  ASSERT_TRUE(legacy.setGUID(guid));
  ASSERT_TRUE(legacy.setAuthKey(auth));
  ASSERT_TRUE(legacy.setSuplaServer("supla.example"));
  ASSERT_TRUE(legacy.setEmail("user@example.com"));
  SuplaDeviceClass device;
  ASSERT_TRUE(device.begin(29));
  ASSERT_TRUE(device.resetToFactorySettingsChecked());
  legacy.reboot();
  char restoredGuid[SUPLA_GUID_SIZE], restoredAuth[SUPLA_AUTHKEY_SIZE];
  EXPECT_TRUE(legacy.getGUID(restoredGuid));
  EXPECT_TRUE(legacy.getAuthKey(restoredAuth));
  EXPECT_EQ(memcmp(guid, restoredGuid, sizeof(guid)), 0);
  EXPECT_EQ(memcmp(auth, restoredAuth, sizeof(auth)), 0);
}
TEST_F(SupLanBootstrap, LegacyCheckedResetReportsCommitFailure) {
  LegacyBootConfig legacy;
  legacy.failCommit = true;
  EXPECT_FALSE(legacy.removeAllChecked());
  EXPECT_GT(legacy.commitAttempts, 0);
}
}  // namespace
