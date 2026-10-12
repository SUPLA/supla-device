// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <SuplaDevice.h>
#include <crypto_test_hooks.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <output_mock.h>
#include <supla/channels/channel.h>
#include <supla/correction.h>
#include <supla/control/hvac_base.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/clock/clock.h>
#include <supla/device/register_device.h>
#include <supla/network/network.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/key_value.h>
#include <supla/suplan/owned_stack.h>
#include <suplan_bootstrap_port.h>
#include <timer_mock.h>
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
  bool commit() override { return true; }
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
  Supla::Protocol::SupLan incomplete(&device, &peers, nullptr, 0);
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
  Supla::Protocol::SupLan incomplete(&device, &peers, nullptr, 0);
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
  ASSERT_TRUE(handle.canRead());
  device.resetToFactorySettings();
  EXPECT_TRUE(protocol->isSupLanServerCapable());
  EXPECT_FALSE(protocol->isTransportOpen());
  EXPECT_FALSE(protocol->remoteResources()->associations->identity()
                   ->identityAvailable());
  EXPECT_EQ(protocol->remoteResources()->effectivePermissions(
      {Supla::SupLan::kResourceTypeChannel, 501}), 0);
  EXPECT_FALSE(handle.canRead());
  EXPECT_FALSE(handle.state().available());
}
}  // namespace
