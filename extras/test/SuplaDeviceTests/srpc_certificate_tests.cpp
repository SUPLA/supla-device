// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <SuplaDevice.h>
#include <supla/device/register_device.h>
#include <supla/network/client.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/storage.h>

#include <string>

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>
#endif

#include "../doubles/config_mock.h"

namespace {

const char borrowedCA[] = "CA in firmware";
const char otherBorrowedCA[] = "Other CA in firmware";

void expectReleased(const char *certificate) {
#if defined(__SANITIZE_ADDRESS__)
  EXPECT_NE(__asan_address_is_poisoned(certificate), 0);
#else
  (void)certificate;
#endif
}

struct CaClientStats {
  int stops = 0;
  int destroyed = 0;
  std::string caAtStop;
  std::string caAtDestruction;
};

class CaClient : public Supla::Client {
 public:
  using Supla::Client::isCertificateValidationEnabled;

  explicit CaClient(CaClientStats &stats) : stats(stats) {
  }

  ~CaClient() override {
    stats.caAtDestruction = rootCACert ? rootCACert : "";
    stats.destroyed++;
  }

  const char *certificate() const {
    return rootCACert;
  }

  int available() override { return 0; }
  uint8_t connected() override { return 0; }
  void setTimeoutMs(uint16_t) override {}

  void stop() override {
    stats.caAtStop = rootCACert ? rootCACert : "";
    stats.stops++;
  }

 protected:
  int connectImp(const char *, uint16_t) override { return 0; }
  size_t writeImp(const uint8_t *, size_t size) override { return size; }
  int readImp(uint8_t *, size_t) override { return 0; }

 private:
  CaClientStats &stats;
};

class CaSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit CaSrpc(SuplaDeviceClass *sdc) : SuplaSrpc(sdc) {
  }

  const char *certificate() const {
    return selectedCertificate;
  }

  void markRegistered() {
    registered = 1;
    firstConnectionAttempt = false;
  }

  bool reconnectStartsFresh() const {
    return registered == 0 && firstConnectionAttempt;
  }
};

class SrpcCertificateTests : public ::testing::Test {
 protected:
  void SetUp() override {
    using ::testing::_;
    using ::testing::Return;
    using ::testing::StrEq;
    Supla::RegisterDevice::resetToDefaults();
    ON_CALL(cfg, isSuplaCommProtocolEnabled()).WillByDefault(Return(true));
    ON_CALL(cfg, getSuplaServerPort()).WillByDefault(Return(2016));
    ON_CALL(cfg, getSuplaServer(_)).WillByDefault([](char *result) {
      snprintf(result, SUPLA_SERVER_NAME_MAXSIZE, "%s", "private.example.com");
      return true;
    });
    ON_CALL(cfg, getEmail(_)).WillByDefault([](char *result) {
      snprintf(result, SUPLA_EMAIL_MAXSIZE, "%s", "user@example.com");
      return true;
    });
    ON_CALL(cfg, getUInt8(StrEq("security_level"), _))
        .WillByDefault([this](const char *, uint8_t *result) {
          *result = securityLevel;
          return true;
        });
    ON_CALL(cfg, getCustomCASize()).WillByDefault([this]() {
      return static_cast<int>(customCA.size() + 1);
    });
    ON_CALL(cfg, getCustomCA(_, _))
        .WillByDefault([this](char *result, int maxSize) {
          EXPECT_GT(maxSize, customCA.size());
          snprintf(result, maxSize, "%s", customCA.c_str());
          return true;
        });
  }

  void TearDown() override {
    Supla::RegisterDevice::resetToDefaults();
  }

  ::testing::NiceMock<ConfigMock> cfg;
  uint8_t securityLevel = 1;
  std::string customCA = "First configured CA";
};

}  // namespace

TEST_F(SrpcCertificateTests, RepeatedReloadReleasesOldCAAndUpdatesClient) {
  SuplaDeviceClass sd;
  CaClientStats stats;
  CaSrpc srpc(&sd);
  ASSERT_TRUE(srpc.onLoadConfig());
  auto client = new CaClient(stats);
  srpc.setNetworkClient(client);

  for (int i = 0; i < 5; i++) {
    const char *oldCA = srpc.certificate();
    const std::string oldValue = customCA;
    srpc.markRegistered();
    srpc.markWriteFailure();
    customCA = "Next configured CA " + std::to_string(i);
    EXPECT_CALL(cfg, getCustomCASize()).WillOnce([&]() {
      expectReleased(oldCA);
      EXPECT_EQ(stats.caAtStop, oldValue);
      EXPECT_STREQ(client->certificate(), "SUPLA");
      return static_cast<int>(customCA.size() + 1);
    });

    ASSERT_TRUE(srpc.onLoadConfig());

    EXPECT_EQ(stats.stops, i + 1);
    EXPECT_EQ(client->certificate(), srpc.certificate());
    EXPECT_STREQ(client->certificate(), customCA.c_str());
    EXPECT_TRUE(srpc.reconnectStartsFresh());
    EXPECT_TRUE(srpc.hasWriteFailure());
  }
}

TEST_F(SrpcCertificateTests, CustomCAToBorrowedCAStopsBeforeRelease) {
  SuplaDeviceClass sd;
  CaClientStats stats;
  CaSrpc srpc(&sd);
  srpc.setSupla3rdPartyCACert(borrowedCA);
  ASSERT_TRUE(srpc.onLoadConfig());
  const char *oldCA = srpc.certificate();
  auto client = new CaClient(stats);
  srpc.setNetworkClient(client);
  securityLevel = 0;

  ASSERT_TRUE(srpc.onLoadConfig());

  expectReleased(oldCA);
  EXPECT_EQ(stats.stops, 1);
  EXPECT_EQ(stats.caAtStop, customCA);
  EXPECT_EQ(client->certificate(), borrowedCA);
  EXPECT_TRUE(client->isCertificateValidationEnabled());
  EXPECT_TRUE(srpc.onLoadConfig());
  EXPECT_EQ(stats.stops, 1);
}

TEST_F(SrpcCertificateTests,
       BorrowedCANotFreedWhenSettersChangeOrAtDestruction) {
  SuplaDeviceClass sd;
  securityLevel = 0;
  {
    CaSrpc srpc(&sd);
    srpc.setSupla3rdPartyCACert(borrowedCA);
    ASSERT_TRUE(srpc.onLoadConfig());
    EXPECT_EQ(srpc.certificate(), borrowedCA);
    ASSERT_TRUE(srpc.onLoadConfig());
    srpc.setSupla3rdPartyCACert(otherBorrowedCA);
  }
  EXPECT_STREQ(borrowedCA, "CA in firmware");
  EXPECT_STREQ(otherBorrowedCA, "Other CA in firmware");
}

TEST_F(SrpcCertificateTests, ExplicitInsecureModeReleasesCustomCA) {
  SuplaDeviceClass sd;
  CaClientStats stats;
  CaSrpc srpc(&sd);
  ASSERT_TRUE(srpc.onLoadConfig());
  const char *oldCA = srpc.certificate();
  auto client = new CaClient(stats);
  srpc.setNetworkClient(client);
  securityLevel = 2;

  ASSERT_TRUE(srpc.onLoadConfig());

  expectReleased(oldCA);
  EXPECT_EQ(stats.stops, 1);
  EXPECT_EQ(client->certificate(), nullptr);
  EXPECT_FALSE(client->isCertificateValidationEnabled());
}

TEST_F(SrpcCertificateTests, FailedCAReadKeepsValidationEnabled) {
  SuplaDeviceClass sd;
  CaClientStats stats;
  CaSrpc srpc(&sd);
  ASSERT_TRUE(srpc.onLoadConfig());
  const char *oldCA = srpc.certificate();
  auto client = new CaClient(stats);
  srpc.setNetworkClient(client);
  EXPECT_CALL(cfg, getCustomCA(::testing::_, ::testing::_))
      .WillOnce([](char *result, int maxSize) {
        snprintf(result, maxSize, "%s", "Partially read CA");
        return false;
      });

  ASSERT_TRUE(srpc.onLoadConfig());

  expectReleased(oldCA);
  EXPECT_EQ(stats.stops, 1);
  EXPECT_STREQ(client->certificate(), "SUPLA");
  EXPECT_TRUE(client->isCertificateValidationEnabled());
}

TEST_F(SrpcCertificateTests, MissingCAKeepsValidationEnabled) {
  SuplaDeviceClass sd;
  CaClientStats stats;
  CaSrpc srpc(&sd);
  ASSERT_TRUE(srpc.onLoadConfig());
  auto client = new CaClient(stats);
  srpc.setNetworkClient(client);
  EXPECT_CALL(cfg, getCustomCASize()).WillOnce(::testing::Return(0));

  ASSERT_TRUE(srpc.onLoadConfig());

  EXPECT_EQ(stats.stops, 1);
  EXPECT_STREQ(client->certificate(), "SUPLA");
  EXPECT_TRUE(client->isCertificateValidationEnabled());
}

TEST_F(SrpcCertificateTests, DisablingProtocolReleasesCustomCA) {
  SuplaDeviceClass sd;
  CaClientStats stats;
  CaSrpc srpc(&sd);
  ASSERT_TRUE(srpc.onLoadConfig());
  const char *oldCA = srpc.certificate();
  auto client = new CaClient(stats);
  srpc.setNetworkClient(client);
  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled())
      .WillOnce(::testing::Return(false));

  ASSERT_TRUE(srpc.onLoadConfig());

  expectReleased(oldCA);
  EXPECT_FALSE(srpc.isEnabled());
  EXPECT_EQ(stats.stops, 1);
  EXPECT_STREQ(client->certificate(), "SUPLA");
}

TEST_F(SrpcCertificateTests, SwitchingToPlainPortReleasesCustomCA) {
  SuplaDeviceClass sd;
  CaClientStats stats;
  CaSrpc srpc(&sd);
  ASSERT_TRUE(srpc.onLoadConfig());
  const char *oldCA = srpc.certificate();
  auto client = new CaClient(stats);
  srpc.setNetworkClient(client);
  EXPECT_CALL(cfg, getSuplaServerPort()).WillOnce(::testing::Return(2015));

  ASSERT_TRUE(srpc.onLoadConfig());

  expectReleased(oldCA);
  EXPECT_EQ(stats.stops, 1);
  EXPECT_STREQ(client->certificate(), "SUPLA");
}

TEST_F(SrpcCertificateTests, InitWithoutConfigReplacesCustomCAWithBorrowedCA) {
  SuplaDeviceClass sd;
  CaClientStats stats;
  CaSrpc srpc(&sd);
  srpc.setSupla3rdPartyCACert(borrowedCA);
  ASSERT_TRUE(srpc.onLoadConfig());
  const char *oldCA = srpc.certificate();
  auto client = new CaClient(stats);
  srpc.setNetworkClient(client);

  Supla::Storage::SetConfigInstance(nullptr);
  srpc.onInit();
  Supla::Storage::SetConfigInstance(&cfg);

  expectReleased(oldCA);
  EXPECT_EQ(stats.stops, 1);
  EXPECT_EQ(client->certificate(), borrowedCA);
}

TEST_F(SrpcCertificateTests,
       DestructionStopsAndDeletesClientBeforeReleasingCA) {
  SuplaDeviceClass sd;
  CaClientStats stats;
  const char *oldCA = nullptr;
  {
    CaSrpc srpc(&sd);
    ASSERT_TRUE(srpc.onLoadConfig());
    oldCA = srpc.certificate();
    srpc.setNetworkClient(new CaClient(stats));
  }

  expectReleased(oldCA);
  EXPECT_EQ(stats.stops, 1);
  EXPECT_EQ(stats.destroyed, 1);
  EXPECT_EQ(stats.caAtStop, customCA);
  EXPECT_EQ(stats.caAtDestruction, customCA);
}

TEST_F(SrpcCertificateTests, DestructionStopsClientsWithoutOwnedCA) {
  SuplaDeviceClass sd;
  for (uint8_t level : {0, 2}) {
    securityLevel = level;
    CaClientStats stats;
    {
      CaSrpc srpc(&sd);
      srpc.setSupla3rdPartyCACert(borrowedCA);
      ASSERT_TRUE(srpc.onLoadConfig());
      srpc.setNetworkClient(new CaClient(stats));
    }
    EXPECT_EQ(stats.stops, 1);
    EXPECT_EQ(stats.destroyed, 1);
    EXPECT_EQ(stats.caAtStop, level == 0 ? borrowedCA : "");
  }
}
