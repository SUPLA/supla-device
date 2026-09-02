// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/device/register_device.h>
#include <supla/element.h>
#include <supla/protocol/supla_srpc.h>

#include <cstring>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

class TestSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit TestSrpc(SuplaDeviceClass *sdc) : Supla::Protocol::SuplaSrpc(sdc) {
  }

  void initializeSrpcForTest() {
    initializeSrpc();
  }

  void deinitializeSrpcForTest() {
    deinitializeSrpc();
  }

  void setRegisteredForTest(bool value) {
    registered = value ? 1 : 0;
  }

  void setDeviceConfigReceivedForTest(bool value) {
    setDeviceConfigReceivedAfterRegistration = value;
  }
};

class PendingElement : public Supla::Element {
 public:
  bool isAnyUpdatePending() const override {
    return pending;
  }

  bool pending = true;
};

uint32_t captureRegistrationFlags(int version, bool sleeping) {
  Supla::RegisterDevice::resetToDefaults();
  Supla::RegisterDevice::setServerName("supla.example");
  Supla::RegisterDevice::setEmail("user@example.com");
  if (sleeping) {
    Supla::RegisterDevice::addFlags(SUPLA_DEVICE_FLAG_SLEEP_MODE_ENABLED);
  }

  NiceMock<SrpcMock> srpc;
  int dummy = 0;
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  protocol.setVersion(version);
  auto *client = new NiceMock<NetworkClientMock>;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, version));
  EXPECT_CALL(*client, connected()).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillOnce(Return(SUPLA_RESULT_TRUE));

  uint32_t flags = 0;
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _))
      .WillOnce([&flags](void *, TDS_SuplaRegisterDeviceHeader *header) {
        flags = static_cast<uint32_t>(header->Flags);
        return SUPLA_RESULT_TRUE;
      });

  protocol.setNetworkClient(client);
  protocol.initializeSrpcForTest();
  EXPECT_FALSE(protocol.iterate(0));
  protocol.deinitializeSrpcForTest();
  return flags;
}

class SuplaSrpcSyncDoneTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::RegisterDevice::resetToDefaults();
  }

  void TearDown() override {
    Supla::RegisterDevice::resetToDefaults();
  }

  void configureSleepingV29(TestSrpc *protocol) {
    Supla::RegisterDevice::addFlags(SUPLA_DEVICE_FLAG_SLEEP_MODE_ENABLED);
    protocol->setVersion(29);
    protocol->setRegisteredForTest(true);
  }

  SimpleTime time;
};

}  // namespace

TEST_F(SuplaSrpcSyncDoneTests,
       RegistrationAdvertisesSyncDoneOnlyForSleepingV29) {
  EXPECT_NE(captureRegistrationFlags(29, true) &
                SUPLA_DEVICE_FLAG_SYNC_DONE_SUPPORTED,
            0U);
  EXPECT_EQ(captureRegistrationFlags(28, true) &
                SUPLA_DEVICE_FLAG_SYNC_DONE_SUPPORTED,
            0U);
  EXPECT_EQ(captureRegistrationFlags(29, false) &
                SUPLA_DEVICE_FLAG_SYNC_DONE_SUPPORTED,
            0U);
}

TEST_F(SuplaSrpcSyncDoneTests, SleepingV29WaitsForSyncDone) {
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  configureSleepingV29(&protocol);

  EXPECT_TRUE(protocol.isUpdatePending());
  protocol.onDeviceSyncDone();
  EXPECT_FALSE(protocol.isUpdatePending());
}

TEST_F(SuplaSrpcSyncDoneTests, OlderProtocolDoesNotRequireSyncDone) {
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  Supla::RegisterDevice::addFlags(SUPLA_DEVICE_FLAG_SLEEP_MODE_ENABLED);
  protocol.setVersion(28);
  protocol.setRegisteredForTest(true);

  EXPECT_FALSE(protocol.isUpdatePending());
}

TEST_F(SuplaSrpcSyncDoneTests, SyncDoneIsIgnoredBeforeRegistration) {
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  configureSleepingV29(&protocol);
  protocol.setRegisteredForTest(false);

  protocol.onDeviceSyncDone();
  protocol.setRegisteredForTest(true);
  EXPECT_TRUE(protocol.isUpdatePending());
}

TEST_F(SuplaSrpcSyncDoneTests,
       SyncDoneReceivedThroughMessageClearsInitialGate) {
  NiceMock<SrpcMock> srpc;
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  configureSleepingV29(&protocol);

  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([](void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->call_id = SUPLA_SD_CALL_DEVICE_SYNC_DONE;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_rd_free(_));

  Supla::messageReceived(nullptr, 0, 0, &protocol, 29);
  EXPECT_FALSE(protocol.isUpdatePending());
  EXPECT_STREQ(TestSrpc::callIdToName(SUPLA_SD_CALL_DEVICE_SYNC_DONE),
               "DEVICE_SYNC_DONE");
}

TEST_F(SuplaSrpcSyncDoneTests,
       PendingCalcfgKeepsDeviceAwakeAfterSyncDoneUntilCleared) {
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  configureSleepingV29(&protocol);

  protocol.calCfgResultPending.set(
      -1, 1234, SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
  EXPECT_FALSE(protocol.calCfgResultPending.empty());

  protocol.onDeviceSyncDone();
  EXPECT_TRUE(protocol.isUpdatePending());

  protocol.calCfgResultPending.clear(-1,
                                     SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
  EXPECT_TRUE(protocol.calCfgResultPending.empty());
  EXPECT_FALSE(protocol.isUpdatePending());
}

TEST_F(SuplaSrpcSyncDoneTests, NewSrpcInitializationRequiresNewSyncDone) {
  NiceMock<SrpcMock> srpc;
  int dummy = 0;
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  configureSleepingV29(&protocol);

  EXPECT_CALL(srpc, srpc_params_init(_)).Times(2);
  EXPECT_CALL(srpc, srpc_init(_)).Times(2).WillRepeatedly(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 29)).Times(2);
  EXPECT_CALL(srpc, srpc_free(_)).Times(2);

  protocol.initializeSrpcForTest();
  protocol.onDeviceSyncDone();
  EXPECT_FALSE(protocol.isUpdatePending());

  protocol.initializeSrpcForTest();
  protocol.setRegisteredForTest(true);
  EXPECT_TRUE(protocol.isUpdatePending());
  protocol.deinitializeSrpcForTest();
}

TEST_F(SuplaSrpcSyncDoneTests, RemoteDeviceConfigStillBlocksSleep) {
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  configureSleepingV29(&protocol);
  Supla::RegisterDevice::addFlags(SUPLA_DEVICE_FLAG_DEVICE_CONFIG_SUPPORTED);
  protocol.onDeviceSyncDone();

  EXPECT_TRUE(protocol.isUpdatePending());
  protocol.setDeviceConfigReceivedForTest(true);
  EXPECT_FALSE(protocol.isUpdatePending());
}

TEST_F(SuplaSrpcSyncDoneTests, ElementUpdateStillBlocksSleep) {
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  configureSleepingV29(&protocol);
  PendingElement element;
  protocol.onDeviceSyncDone();

  EXPECT_TRUE(protocol.isUpdatePending());
  element.pending = false;
  EXPECT_FALSE(protocol.isUpdatePending());
}
