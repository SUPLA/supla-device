// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_WIFI_SCAN_RESULT_H_
#define SRC_SUPLA_NETWORK_WIFI_SCAN_RESULT_H_

#include <stdint.h>

namespace Supla {

constexpr uint8_t WifiScanMaxResults = 16;
constexpr uint8_t WifiScanSsidMaxSize = 33;
constexpr uint32_t WifiScanDefaultMaxAgeMs = 5 * 60 * 1000;

struct WifiScanResult {
  char ssid[WifiScanSsidMaxSize] = {};
  int32_t rssi = 0;
  int32_t channel = 0;
};

enum class WifiScanLookupStatus : uint8_t {
  NotAvailable,
  Found,
  NotFound,
  Stale,
};

class WifiScanResultCache {
 public:
  static WifiScanResultCache *Instance();

  void clear();
  void beginUpdate();
  void addOrUpdate(const char *ssid, int32_t rssi, int32_t channel);
  void finishUpdate(uint32_t timestampMs);

  WifiScanLookupStatus lookup(const char *ssid,
                              WifiScanResult *result,
                              uint32_t nowMs,
                              uint32_t maxAgeMs =
                                  WifiScanDefaultMaxAgeMs) const;

  uint8_t getCount() const;
  uint32_t getTimestampMs() const;
  bool hasScan() const;
  bool getResult(uint8_t index, WifiScanResult *result) const;

 private:
  void sortByRssi();
  int find(const char *ssid) const;
  int findWeakest() const;

  WifiScanResult entries[WifiScanMaxResults] = {};
  uint8_t count = 0;
  uint32_t timestampMs = 0;
  bool scanAvailable = false;
};

}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_WIFI_SCAN_RESULT_H_
