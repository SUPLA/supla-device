// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_WEB_LOGIN_LIMITER_H_
#define EXTRAS_PORTING_ESP_IDF_WEB_LOGIN_LIMITER_H_

#include <stdint.h>
#include <string.h>

namespace Supla {

class WebLoginLimiter {
 public:
  struct Address {
    // IPv4 uses its IPv4-mapped IPv6 representation.
    uint8_t bytes[16] = {};
    uint32_t scope = 0;

    bool operator==(const Address &other) const {
      return scope == other.scope &&
             memcmp(bytes, other.bytes, sizeof(bytes)) == 0;
    }
  };

  static constexpr unsigned CAPACITY = 8;
  static constexpr uint8_t MAX_ATTEMPTS = 5;
  static constexpr uint32_t TIMEOUT_MS = 60000;

  bool isBlocked(const Address &address, uint32_t now) {
    const Entry *entry = findEntry(address, now);
    // Fail closed for new sources when full; never evict active counters.
    return entry == nullptr || entry->attempts >= MAX_ATTEMPTS;
  }

  uint8_t failedAttempt(const Address &address, uint32_t now) {
    Entry *entry = findEntry(address, now);
    if (entry == nullptr) {
      return MAX_ATTEMPTS;
    }
    if (entry->attempts < MAX_ATTEMPTS) {
      entry->address = address;
      entry->attempts++;
      entry->lastFailure = now;
    }
    return entry->attempts;
  }

  void reset(const Address &address) {
    for (auto &entry : entries) {
      if (entry.attempts != 0 && entry.address == address) {
        entry.attempts = 0;
        return;
      }
    }
  }

 private:
  struct Entry {
    Address address;
    uint32_t lastFailure = 0;
    uint8_t attempts = 0;
  };

  Entry *findEntry(const Address &address, uint32_t now) {
    Entry *available = nullptr;
    for (auto &entry : entries) {
      // Also release partial counters after a minute without failures.
      if (entry.attempts != 0 &&
          static_cast<uint32_t>(now - entry.lastFailure) >= TIMEOUT_MS) {
        entry.attempts = 0;
      }
      if (entry.attempts == 0) {
        if (available == nullptr) {
          available = &entry;
        }
      } else if (entry.address == address) {
        return &entry;
      }
    }
    return available;
  }

  Entry entries[CAPACITY] = {};
};

}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_WEB_LOGIN_LIMITER_H_
