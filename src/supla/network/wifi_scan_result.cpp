// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wifi_scan_result.h"

#include <string.h>

namespace Supla {

namespace {
WifiScanResultCache instance;
}

void WifiScanResultCache::sortByRssi() {
  for (uint8_t i = 1; i < count; i++) {
    WifiScanResult current = entries[i];
    uint8_t j = i;
    while (j > 0 && current.rssi > entries[j - 1].rssi) {
      entries[j] = entries[j - 1];
      j--;
    }
    entries[j] = current;
  }
}

WifiScanResultCache *WifiScanResultCache::Instance() {
  return &instance;
}

void WifiScanResultCache::clear() {
  count = 0;
  timestampMs = 0;
  scanAvailable = false;
  for (uint8_t i = 0; i < WifiScanMaxResults; i++) {
    entries[i] = WifiScanResult{};
  }
}

void WifiScanResultCache::beginUpdate() {
  count = 0;
  scanAvailable = false;
  for (uint8_t i = 0; i < WifiScanMaxResults; i++) {
    entries[i] = WifiScanResult{};
  }
}

void WifiScanResultCache::addOrUpdate(const char *ssid,
                                      int32_t rssi,
                                      int32_t channel) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return;
  }

  int idx = find(ssid);
  if (idx >= 0) {
    if (rssi > entries[idx].rssi) {
      entries[idx].rssi = rssi;
      entries[idx].channel = channel;
    }
    return;
  }

  if (count < WifiScanMaxResults) {
    idx = count++;
  } else {
    idx = findWeakest();
    if (idx < 0 || rssi <= entries[idx].rssi) {
      return;
    }
  }

  strncpy(entries[idx].ssid, ssid, WifiScanSsidMaxSize - 1);
  entries[idx].ssid[WifiScanSsidMaxSize - 1] = '\0';
  entries[idx].rssi = rssi;
  entries[idx].channel = channel;
}

void WifiScanResultCache::finishUpdate(uint32_t timestamp) {
  sortByRssi();
  timestampMs = timestamp;
  scanAvailable = true;
}

WifiScanLookupStatus WifiScanResultCache::lookup(
    const char *ssid,
    WifiScanResult *result,
    uint32_t nowMs,
    uint32_t maxAgeMs) const {
  if (ssid == nullptr || ssid[0] == '\0' || !scanAvailable) {
    return WifiScanLookupStatus::NotAvailable;
  }

  if (maxAgeMs > 0 && nowMs - timestampMs > maxAgeMs) {
    return WifiScanLookupStatus::Stale;
  }

  int idx = find(ssid);
  if (idx < 0) {
    return WifiScanLookupStatus::NotFound;
  }

  if (result != nullptr) {
    *result = entries[idx];
  }
  return WifiScanLookupStatus::Found;
}

uint8_t WifiScanResultCache::getCount() const {
  return count;
}

uint32_t WifiScanResultCache::getTimestampMs() const {
  return timestampMs;
}

bool WifiScanResultCache::hasScan() const {
  return scanAvailable;
}

bool WifiScanResultCache::getResult(uint8_t index,
                                    WifiScanResult *result) const {
  if (result == nullptr || index >= count) {
    return false;
  }

  *result = entries[index];
  return true;
}

int WifiScanResultCache::find(const char *ssid) const {
  if (ssid == nullptr) {
    return -1;
  }
  for (uint8_t i = 0; i < count; i++) {
    if (strncmp(entries[i].ssid, ssid, WifiScanSsidMaxSize) == 0) {
      return i;
    }
  }
  return -1;
}

int WifiScanResultCache::findWeakest() const {
  if (count == 0) {
    return -1;
  }

  uint8_t weakest = 0;
  for (uint8_t i = 1; i < count; i++) {
    if (entries[i].rssi < entries[weakest].rssi) {
      weakest = i;
    }
  }
  return weakest;
}

}  // namespace Supla
