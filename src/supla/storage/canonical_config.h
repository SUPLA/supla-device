// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_STORAGE_CANONICAL_CONFIG_H_
#define SRC_SUPLA_STORAGE_CANONICAL_CONFIG_H_

#include <supla/storage/config.h>
#include <supla/storage/storage.h>

namespace Supla {
namespace StorageDetail {
// Config setters and commit are the persistence boundary. A failed candidate
// may remain staged until a later commit; callers retry normally.
// Negative channelNo preserves local ChannelFunction handling.
template <typename Record>
bool persistCanonicalConfig(const char *key, const Record &record,
                            int channelNo = -1) {
  auto cfg = Storage::ConfigInstance();
  return cfg &&
      (channelNo < 0 || cfg->setChannelFunction(channelNo, record.function)) &&
      cfg->setBlob(key, reinterpret_cast<const char *>(&record),
                   sizeof(record)) &&
      cfg->commit();
}

inline bool cleanupLegacyConfig(const char *key) {
  auto cfg = Storage::ConfigInstance();
  if (!cfg) return false;
  return cfg->getBlobSize(key) < 0 || (cfg->eraseKey(key) && cfg->commit());
}
}  // namespace StorageDetail
}  // namespace Supla
#endif  // SRC_SUPLA_STORAGE_CANONICAL_CONFIG_H_
