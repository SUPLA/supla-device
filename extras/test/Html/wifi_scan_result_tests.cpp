// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/network/wifi_scan_result.h>

#include <cstdio>

class WifiScanResultCacheTests : public ::testing::Test {
 protected:
  void TearDown() override {
    Supla::WifiScanResultCache::Instance()->clear();
  }
};

TEST_F(WifiScanResultCacheTests, ReturnsNotAvailableBeforeFirstScan) {
  Supla::WifiScanResult result = {};
  EXPECT_EQ(Supla::WifiScanLookupStatus::NotAvailable,
            Supla::WifiScanResultCache::Instance()->lookup(
                "ssid", &result, 1000));
}

TEST_F(WifiScanResultCacheTests, KeepsStrongestResultForDuplicatedSsid) {
  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid", -80, 1);
  cache->addOrUpdate("ssid", -55, 6);
  cache->finishUpdate(1000);

  Supla::WifiScanResult result = {};
  EXPECT_EQ(Supla::WifiScanLookupStatus::Found,
            cache->lookup("ssid", &result, 1200));
  EXPECT_STREQ("ssid", result.ssid);
  EXPECT_EQ(-55, result.rssi);
  EXPECT_EQ(6, result.channel);
}

TEST_F(WifiScanResultCacheTests, ReturnsResultByIndex) {
  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid", -55, 6);
  cache->finishUpdate(1000);

  Supla::WifiScanResult result = {};
  EXPECT_TRUE(cache->getResult(0, &result));
  EXPECT_STREQ("ssid", result.ssid);
  EXPECT_EQ(-55, result.rssi);
  EXPECT_EQ(6, result.channel);
  EXPECT_FALSE(cache->getResult(1, &result));
  EXPECT_FALSE(cache->getResult(0, nullptr));
}

TEST_F(WifiScanResultCacheTests, SortsResultsByBestRssiFirst) {
  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("weak", -90, 1);
  cache->addOrUpdate("strong", -40, 6);
  cache->addOrUpdate("medium", -70, 11);
  cache->finishUpdate(1000);

  Supla::WifiScanResult result = {};
  ASSERT_TRUE(cache->getResult(0, &result));
  EXPECT_STREQ("strong", result.ssid);
  EXPECT_EQ(-40, result.rssi);
  ASSERT_TRUE(cache->getResult(1, &result));
  EXPECT_STREQ("medium", result.ssid);
  EXPECT_EQ(-70, result.rssi);
  ASSERT_TRUE(cache->getResult(2, &result));
  EXPECT_STREQ("weak", result.ssid);
  EXPECT_EQ(-90, result.rssi);
}

TEST_F(WifiScanResultCacheTests, BoundsResultsAndKeepsStrongerNetworks) {
  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  for (int i = 0; i < Supla::WifiScanMaxResults; i++) {
    char ssid[Supla::WifiScanSsidMaxSize] = {};
    snprintf(ssid, sizeof(ssid), "ssid_%02d", i);
    cache->addOrUpdate(ssid, -90 + i, i);
  }
  cache->addOrUpdate("strong", -30, 11);
  cache->addOrUpdate("weak", -100, 12);
  cache->finishUpdate(1000);

  EXPECT_EQ(Supla::WifiScanMaxResults, cache->getCount());

  Supla::WifiScanResult result = {};
  EXPECT_EQ(Supla::WifiScanLookupStatus::Found,
            cache->lookup("strong", &result, 1000));
  EXPECT_EQ(-30, result.rssi);
  EXPECT_EQ(Supla::WifiScanLookupStatus::NotFound,
            cache->lookup("weak", &result, 1000));
}

TEST_F(WifiScanResultCacheTests, ReportsStaleScan) {
  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();
  cache->addOrUpdate("ssid", -60, 1);
  cache->finishUpdate(1000);

  Supla::WifiScanResult result = {};
  EXPECT_EQ(Supla::WifiScanLookupStatus::Stale,
            cache->lookup("ssid", &result, 2001, 1000));
}
