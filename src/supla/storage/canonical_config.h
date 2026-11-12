// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_STORAGE_CANONICAL_CONFIG_H_
#define SRC_SUPLA_STORAGE_CANONICAL_CONFIG_H_

#include <string.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>

namespace Supla {
namespace StorageDetail {
// Stage normal ChannelConfig without making runtime acceptance depend on IO.
// Set failures are diagnostic; only commit failures use Config's delayed retry.
template <typename Record>
bool stageCanonicalConfig(const char *key, const Record &record,
                          int channelNo) {
  auto cfg = Storage::ConfigInstance();
  if (!cfg) return false;
  Record previous;
  if (cfg->getBlobSize(key) == sizeof(record) &&
      cfg->getBlob(key, reinterpret_cast<char *>(&previous),
                   sizeof(previous)) &&
      !memcmp(&previous, &record, sizeof(record)) &&
      cfg->getChannelFunction(channelNo) ==
          static_cast<int32_t>(record.function)) return true;
  cfg->saveWithDelay(5000);
  const bool function = cfg->setChannelFunction(channelNo, record.function);
  const bool config = cfg->setBlob(key,
      reinterpret_cast<const char *>(&record), sizeof(record));
  return function && config;
}

// Legacy migration must confirm V2 before deleting its only recovery record.
template <typename Record>
bool persistCanonicalConfig(const char *key, const Record &record,
                            int channelNo) {
  auto cfg = Storage::ConfigInstance();
  Record stored;
  return cfg && cfg->setChannelFunction(channelNo, record.function) &&
      cfg->setBlob(key, reinterpret_cast<const char *>(&record),
                   sizeof(record)) && cfg->commit() &&
      cfg->getBlob(key, reinterpret_cast<char *>(&stored), sizeof(stored)) &&
      !memcmp(&stored, &record, sizeof(record)) &&
      cfg->getChannelFunction(channelNo) ==
          static_cast<int32_t>(record.function);
}

inline bool cleanupLegacyConfig(const char *key) {
  auto cfg = Storage::ConfigInstance();
  if (!cfg) return false;
  return cfg->getBlobSize(key) < 0 || (cfg->eraseKey(key) && cfg->commit());
}
}  // namespace StorageDetail
}  // namespace Supla
#endif  // SRC_SUPLA_STORAGE_CANONICAL_CONFIG_H_
