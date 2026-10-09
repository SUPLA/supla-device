// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_STORAGE_CANONICAL_CONFIG_H_
#define SRC_SUPLA_STORAGE_CANONICAL_CONFIG_H_

#include <string.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>

namespace Supla {
namespace StorageDetail {
// Uses Config commit/readback. ChannelFunction remains the selector; the V2
// function is only a marker. Negative channelNo preserves local staged saves.
// Multi-key backends can expose either write before commit: boot rejects a
// mismatch. Rollback restores staging only, never claims durable rollback.
template <typename Record>
bool persistCanonicalConfig(const char *key, const Record &record,
                            bool *uncertain, int channelNo = -1) {
  auto cfg = Storage::ConfigInstance();
  if (!cfg || !uncertain) return false;
  Record previous = {};
  const int previousSize = cfg->getBlobSize(key);
  const bool existed = previousSize > 0 &&
      previousSize <= static_cast<int>(sizeof(previous)) &&
      cfg->getBlob(key, reinterpret_cast<char *>(&previous), previousSize);
  const int32_t oldFunction = channelNo >= 0
      ? cfg->getChannelFunction(channelNo) : -1;
  const bool functionChanged = channelNo >= 0 &&
      oldFunction != static_cast<int32_t>(record.function);
  const bool recordChanged = !existed || previousSize != sizeof(record) ||
      memcmp(&record, &previous, sizeof(record)) != 0;
  if (!*uncertain && !functionChanged && !recordChanged) return true;
  if ((!functionChanged ||
       cfg->setChannelFunction(channelNo, record.function)) &&
      (!recordChanged || cfg->setBlob(key,
          reinterpret_cast<const char *>(&record), sizeof(record))) &&
      cfg->commit()) {
    Record confirmed = {};
    if ((channelNo < 0 || cfg->getChannelFunction(channelNo) ==
            static_cast<int32_t>(record.function)) &&
        cfg->getBlobSize(key) == sizeof(confirmed) &&
        cfg->getBlob(key, reinterpret_cast<char *>(&confirmed),
                     sizeof(confirmed)) &&
        memcmp(&record, &confirmed, sizeof(record)) == 0) {
      *uncertain = false;
      return true;
    }
  }
  *uncertain = true;
  if (functionChanged) {
    if (oldFunction >= 0) {
      cfg->setChannelFunction(channelNo, oldFunction);
    } else {
      char functionKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
      cfg->generateKey(functionKey, channelNo, ConfigTag::ChannelFunctionTag);
      cfg->eraseKey(functionKey);
    }
  }
  if (existed) {
    cfg->setBlob(key, reinterpret_cast<const char *>(&previous), previousSize);
  } else {
    cfg->eraseKey(key);
  }
  return false;
}

inline bool cleanupLegacyConfig(const char *key, bool retry = false) {
  auto cfg = Storage::ConfigInstance();
  if (!cfg) return false;
  // An earlier erase may have changed only the backend's staging state.
  // Keep retrying its commit until the caller can clear cleanupPending.
  if (cfg->getBlobSize(key) < 0) return !retry || cfg->commit();
  return cfg->eraseKey(key) && cfg->commit();
}
}  // namespace StorageDetail
}  // namespace Supla
#endif  // SRC_SUPLA_STORAGE_CANONICAL_CONFIG_H_
