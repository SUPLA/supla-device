/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <arpa/inet.h>
#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string.h>
#include <supla/network/netif_config.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace {

constexpr uint32_t kIp = 0xC0A80164u;       // 192.168.1.100
constexpr uint32_t kNetmask = 0xFFFFFF00u;  // 255.255.255.0
constexpr uint32_t kGateway = 0xC0A80101u;  // 192.168.1.1
constexpr uint32_t kDns1 = 0x08080808u;     // 8.8.8.8
constexpr uint32_t kDns2 = 0x08080404u;     // 8.8.4.4

Supla::NetifConfigBlob makeStaticCfg() {
  Supla::NetifConfigBlob cfg = {};
  cfg.version = Supla::NETIF_CONFIG_BLOB_VERSION;
  cfg.ipMode = static_cast<uint8_t>(Supla::NetifIpMode::Static);
  cfg.ip = kIp;
  cfg.netmask = kNetmask;
  cfg.gateway = kGateway;
  cfg.dns1 = kDns1;
  cfg.dns2 = kDns2;
  return cfg;
}

}  // namespace

TEST(NetifConfigTests, parseIpv4AddressTests) {
  uint32_t ip = 0;
  EXPECT_TRUE(Supla::parseIpv4Address("192.168.1.100", &ip));
  EXPECT_EQ(ip, kIp);

  EXPECT_FALSE(Supla::parseIpv4Address("", &ip));
  EXPECT_FALSE(Supla::parseIpv4Address("192.168.1.256", &ip));
  EXPECT_FALSE(Supla::parseIpv4Address("192.168.1", &ip));
}

TEST(NetifConfigTests, parseNetmaskOrPrefixTests) {
  uint32_t mask = 0;
  EXPECT_TRUE(Supla::parseNetmaskOrPrefix("255.255.255.0", &mask));
  EXPECT_EQ(mask, kNetmask);
  EXPECT_TRUE(Supla::parseNetmaskOrPrefix("/24", &mask));
  EXPECT_EQ(mask, kNetmask);
  EXPECT_TRUE(Supla::parseNetmaskOrPrefix("32", &mask));
  EXPECT_EQ(mask, 0xFFFFFFFFu);
  EXPECT_FALSE(Supla::parseNetmaskOrPrefix("255.0.255.0", &mask));
}

TEST(NetifConfigTests, validStaticConfigTests) {
  auto cfg = makeStaticCfg();
  EXPECT_TRUE(Supla::isValidStaticNetifConfig(cfg));

  cfg.gateway = 0xC0A80201u;
  EXPECT_FALSE(Supla::isValidStaticNetifConfig(cfg));

  cfg = makeStaticCfg();
  cfg.ip = 0xC0A80100u;
  EXPECT_FALSE(Supla::isValidStaticNetifConfig(cfg));

  cfg = makeStaticCfg();
  cfg.ip = 0xC0A801FFu;
  EXPECT_FALSE(Supla::isValidStaticNetifConfig(cfg));

  cfg = makeStaticCfg();
  cfg.gateway = 0xC0A801FFu;
  EXPECT_FALSE(Supla::isValidStaticNetifConfig(cfg));

  cfg = makeStaticCfg();
  cfg.gateway = cfg.ip;
  EXPECT_FALSE(Supla::isValidStaticNetifConfig(cfg));
}

TEST(NetifConfigTests, loadNetifConfigMissingBlobDefaultsToDhcp) {
  ConfigMock cfg;
  Supla::NetifConfigBlob loaded = {};

  EXPECT_CALL(cfg,
              getBlob(StrEq("wifi_cfg"), _, sizeof(Supla::NetifConfigBlob)))
      .WillOnce(Return(false));

  EXPECT_FALSE(cfg.Supla::Config::loadNetifConfig(
      Supla::ConfigTag::WifiNetifCfgTag, &loaded));
  EXPECT_EQ(loaded.ipMode, static_cast<uint8_t>(Supla::NetifIpMode::DHCP));
  EXPECT_EQ(loaded.ip, 0u);
}

TEST(NetifConfigTests, loadNetifConfigStaticBlobConvertsFromNetworkOrder) {
  ConfigMock cfg;
  Supla::NetifConfigBlob stored = makeStaticCfg();
  stored.ip = htonl(stored.ip);
  stored.netmask = htonl(stored.netmask);
  stored.gateway = htonl(stored.gateway);
  stored.dns1 = htonl(stored.dns1);
  stored.dns2 = htonl(stored.dns2);

  Supla::NetifConfigBlob loaded = {};
  EXPECT_CALL(cfg,
              getBlob(StrEq("wifi_cfg"), _, sizeof(Supla::NetifConfigBlob)))
      .WillOnce([stored](const char*, char* value, size_t) {
        memcpy(value, &stored, sizeof(stored));
        return true;
      });

  EXPECT_TRUE(cfg.Supla::Config::loadNetifConfig(
      Supla::ConfigTag::WifiNetifCfgTag, &loaded));
  EXPECT_EQ(loaded.ipMode, static_cast<uint8_t>(Supla::NetifIpMode::Static));
  EXPECT_EQ(loaded.ip, kIp);
  EXPECT_EQ(loaded.netmask, kNetmask);
  EXPECT_EQ(loaded.gateway, kGateway);
  EXPECT_EQ(loaded.dns1, kDns1);
  EXPECT_EQ(loaded.dns2, kDns2);
}

TEST(NetifConfigTests, loadNetifConfigDhcpBlobNormalizesToZero) {
  ConfigMock cfg;
  Supla::NetifConfigBlob stored = {};
  stored.version = Supla::NETIF_CONFIG_BLOB_VERSION;
  stored.ipMode = static_cast<uint8_t>(Supla::NetifIpMode::DHCP);
  stored.ip = htonl(kIp);
  stored.netmask = htonl(kNetmask);
  stored.gateway = htonl(kGateway);
  stored.dns1 = htonl(kDns1);
  stored.dns2 = htonl(kDns2);

  Supla::NetifConfigBlob loaded = {};
  EXPECT_CALL(cfg,
              getBlob(StrEq("wifi_cfg"), _, sizeof(Supla::NetifConfigBlob)))
      .WillOnce([stored](const char*, char* value, size_t) {
        memcpy(value, &stored, sizeof(stored));
        return true;
      });

  EXPECT_TRUE(cfg.Supla::Config::loadNetifConfig(
      Supla::ConfigTag::WifiNetifCfgTag, &loaded));
  EXPECT_EQ(loaded.ipMode, static_cast<uint8_t>(Supla::NetifIpMode::DHCP));
  EXPECT_EQ(loaded.ip, 0u);
  EXPECT_EQ(loaded.netmask, 0u);
  EXPECT_EQ(loaded.gateway, 0u);
  EXPECT_EQ(loaded.dns1, 0u);
  EXPECT_EQ(loaded.dns2, 0u);
}

TEST(NetifConfigTests, loadNetifConfigUnsupportedVersionDefaultsToDhcp) {
  ConfigMock cfg;
  Supla::NetifConfigBlob stored = makeStaticCfg();
  stored.version = Supla::NETIF_CONFIG_BLOB_VERSION + 1;
  stored.ip = htonl(stored.ip);
  stored.netmask = htonl(stored.netmask);
  stored.gateway = htonl(stored.gateway);
  stored.dns1 = htonl(stored.dns1);
  stored.dns2 = htonl(stored.dns2);

  Supla::NetifConfigBlob loaded = {};
  EXPECT_CALL(cfg,
              getBlob(StrEq("wifi_cfg"), _, sizeof(Supla::NetifConfigBlob)))
      .WillOnce([stored](const char*, char* value, size_t) {
        memcpy(value, &stored, sizeof(stored));
        return true;
      });

  EXPECT_FALSE(cfg.Supla::Config::loadNetifConfig(
      Supla::ConfigTag::WifiNetifCfgTag, &loaded));
  EXPECT_EQ(loaded.ipMode, static_cast<uint8_t>(Supla::NetifIpMode::DHCP));
}

TEST(NetifConfigTests, loadNetifConfigRejectsInvalidStaticBlob) {
  ConfigMock cfg;
  Supla::NetifConfigBlob stored = makeStaticCfg();
  stored.netmask = 0xFF00FF00u;
  stored.ip = htonl(stored.ip);
  stored.netmask = htonl(stored.netmask);
  stored.gateway = htonl(stored.gateway);
  stored.dns1 = htonl(stored.dns1);
  stored.dns2 = htonl(stored.dns2);

  Supla::NetifConfigBlob loaded = {};
  EXPECT_CALL(cfg,
              getBlob(StrEq("wifi_cfg"), _, sizeof(Supla::NetifConfigBlob)))
      .WillOnce([stored](const char*, char* value, size_t) {
        memcpy(value, &stored, sizeof(stored));
        return true;
      });

  EXPECT_FALSE(cfg.Supla::Config::loadNetifConfig(
      Supla::ConfigTag::WifiNetifCfgTag, &loaded));
  EXPECT_EQ(loaded.ipMode, static_cast<uint8_t>(Supla::NetifIpMode::DHCP));
}

TEST(NetifConfigTests, saveNetifConfigWritesNetworkOrderStaticBlob) {
  ConfigMock cfg;
  Supla::NetifConfigBlob stored = makeStaticCfg();

  EXPECT_CALL(cfg,
              setBlob(StrEq("wifi_cfg"), _, sizeof(Supla::NetifConfigBlob)))
      .WillOnce([&](const char*, const char* value, size_t blobSize) {
        EXPECT_EQ(blobSize, sizeof(Supla::NetifConfigBlob));
        Supla::NetifConfigBlob raw = {};
        memcpy(&raw, value, blobSize);
        EXPECT_EQ(raw.version, Supla::NETIF_CONFIG_BLOB_VERSION);
        EXPECT_EQ(raw.ipMode, static_cast<uint8_t>(Supla::NetifIpMode::Static));
        EXPECT_EQ(raw.ip, htonl(kIp));
        EXPECT_EQ(raw.netmask, htonl(kNetmask));
        EXPECT_EQ(raw.gateway, htonl(kGateway));
        EXPECT_EQ(raw.dns1, htonl(kDns1));
        EXPECT_EQ(raw.dns2, htonl(kDns2));
        return true;
      });

  EXPECT_TRUE(cfg.Supla::Config::saveNetifConfig(
      Supla::ConfigTag::WifiNetifCfgTag, stored));
}

TEST(NetifConfigTests, saveNetifConfigDhcpWritesZeroedBlob) {
  ConfigMock cfg;
  Supla::NetifConfigBlob stored = {};
  stored.ipMode = static_cast<uint8_t>(Supla::NetifIpMode::DHCP);
  stored.ip = kIp;
  stored.netmask = kNetmask;
  stored.gateway = kGateway;
  stored.dns1 = kDns1;
  stored.dns2 = kDns2;

  EXPECT_CALL(cfg,
              setBlob(StrEq("wifi_cfg"), _, sizeof(Supla::NetifConfigBlob)))
      .WillOnce([&](const char*, const char* value, size_t blobSize) {
        Supla::NetifConfigBlob raw = {};
        memcpy(&raw, value, blobSize);
        EXPECT_EQ(raw.version, Supla::NETIF_CONFIG_BLOB_VERSION);
        EXPECT_EQ(raw.ipMode, static_cast<uint8_t>(Supla::NetifIpMode::DHCP));
        EXPECT_EQ(raw.ip, 0u);
        EXPECT_EQ(raw.netmask, 0u);
        EXPECT_EQ(raw.gateway, 0u);
        EXPECT_EQ(raw.dns1, 0u);
        EXPECT_EQ(raw.dns2, 0u);
        return true;
      });

  EXPECT_TRUE(cfg.Supla::Config::saveNetifConfig(
      Supla::ConfigTag::WifiNetifCfgTag, stored));
}

TEST(NetifConfigTests, saveNetifConfigRejectsInvalidStaticBlob) {
  ConfigMock cfg;
  Supla::NetifConfigBlob stored = makeStaticCfg();
  stored.netmask = 0xFF00FF00u;

  EXPECT_CALL(cfg, setBlob(_, _, _)).Times(0);
  EXPECT_FALSE(cfg.Supla::Config::saveNetifConfig(
      Supla::ConfigTag::WifiNetifCfgTag, stored));
}

TEST(NetifConfigTests, removeNetifConfigErasesBlobKey) {
  ConfigMock cfg;
  EXPECT_CALL(cfg, eraseKey(StrEq("wifi_cfg"))).WillOnce(Return(true));
  EXPECT_TRUE(
      cfg.Supla::Config::removeNetifConfig(Supla::ConfigTag::WifiNetifCfgTag));
}
