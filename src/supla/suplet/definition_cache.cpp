/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

#include <supla/storage/config.h>
#if !defined(SUPLA_TEST) && (defined(ESP32) || defined(SUPLA_DEVICE_ESP32))
#include <supla/sha256.h>
#endif
#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <supla/suplet/definition_cache.h>

namespace Supla {
namespace Suplet {

namespace {

constexpr uint32_t kCacheMagic = 0x5344504C;  // SDPL
constexpr uint8_t kCacheVersion = 3;
constexpr uint16_t kChunkSize = SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE;
constexpr uint8_t kMaxCacheSlots = SUPLA_SUPLET_MAX_CACHED_DEFINITIONS;
constexpr uint16_t kMaxChunkCount =
(SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + kChunkSize - 1) / kChunkSize;

#pragma pack(push, 1)
struct BlobHeader {
  uint32_t magic = 0;
  uint8_t version = 0;
  uint8_t reserved = 0;
  uint16_t jsonSize = 0;
  uint32_t definitionId = 0;
  uint16_t definitionVersion = 0;
  uint16_t chunkCount = 0;
  uint16_t chunkSize = 0;
  uint8_t sha256[32] = {};
};
#pragma pack(pop)

uint16_t chunkCountForSize(uint16_t size) {
  return (size + kChunkSize - 1) / kChunkSize;
}

uint16_t chunkPayloadSize(uint16_t jsonSize, uint16_t chunkIndex) {
  const uint32_t offset = static_cast<uint32_t>(chunkIndex) * kChunkSize;
  if (offset >= jsonSize) {
    return 0;
  }
  const uint32_t remaining = jsonSize - offset;
  return remaining > kChunkSize ? kChunkSize : static_cast<uint16_t>(remaining);
}

}  // namespace

DefinitionCache::DefinitionCache(Supla::Config *config,
                                 Sha256Provider *sha256Provider)
    : config(config), sha256Provider(sha256Provider) {
}

#if !defined(SUPLA_TEST) && (defined(ESP32) || defined(SUPLA_DEVICE_ESP32))
bool DefaultSha256Provider::calculate(const uint8_t *data,
                                      size_t dataSize,
                                      uint8_t *output,
                                      size_t outputSize) {
  if (data == nullptr || output == nullptr || outputSize < 32 ||
      dataSize > static_cast<size_t>(INT_MAX)) {
    return false;
  }

  Supla::Sha256 sha256;
  sha256.update(data, static_cast<int>(dataSize));
  sha256.digest(output, 32);
  return true;
}
#endif

bool DefinitionCache::save(uint32_t definitionId,
                           uint16_t definitionVersion,
                           const char *json,
                           const uint8_t *expectedSha256) {
  if (config == nullptr || definitionId == 0 || definitionVersion == 0 ||
      json == nullptr || expectedSha256 == nullptr) {
    return false;
  }

  size_t jsonSize = strlen(json);
  if (jsonSize == 0 || jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE ||
      jsonSize > UINT16_MAX) {
    return false;
  }

  uint8_t calculatedSha[32] = {};
  if (!calculateAndVerify(json,
                          static_cast<uint16_t>(jsonSize),
                          expectedSha256,
                          calculatedSha)) {
    return false;
  }

  int slot = findSlot(definitionId, definitionVersion);
  if (slot < 0) {
    slot = findFreeSlot();
  }
  if (slot < 0) {
    return false;
  }

  return saveSlot(static_cast<uint8_t>(slot),
                  definitionId,
                  definitionVersion,
                  json,
                  static_cast<uint16_t>(jsonSize),
                  calculatedSha);
}

bool DefinitionCache::load(uint32_t definitionId,
                           uint16_t definitionVersion,
                           char *json,
                           size_t jsonSize,
                           CachedDefinitionInfo *info) const {
  if (json == nullptr || jsonSize == 0) {
    return false;
  }

  int slot = findSlot(definitionId, definitionVersion);
  if (slot < 0) {
    return false;
  }

  CachedDefinitionInfo loadedInfo = {};
  uint16_t chunkCount = 0;
  uint16_t chunkSize = 0;
  if (!loadHeader(
          static_cast<uint8_t>(slot), &loadedInfo, &chunkCount, &chunkSize) ||
      jsonSize <= loadedInfo.jsonSize ||
      !loadPayload(static_cast<uint8_t>(slot),
                   false,
                   loadedInfo,
                   chunkCount,
                   chunkSize,
                   json,
                   jsonSize) ||
      !calculateAndVerify(
          json, loadedInfo.jsonSize, loadedInfo.sha256, nullptr)) {
    return false;
  }

  if (info != nullptr) {
    *info = loadedInfo;
  }
  return true;
}

bool DefinitionCache::erase(uint32_t definitionId, uint16_t definitionVersion) {
  int slot = findSlot(definitionId, definitionVersion);
  if (slot < 0) {
    return false;
  }
  return eraseSlot(static_cast<uint8_t>(slot));
}

bool DefinitionCache::contains(uint32_t definitionId,
                               uint16_t definitionVersion) const {
  return findSlot(definitionId, definitionVersion) >= 0;
}

bool DefinitionCache::getInfo(uint8_t index, CachedDefinitionInfo *info) const {
  return loadHeader(index, info);
}

bool DefinitionCache::beginStagedSave(uint32_t definitionId,
                                      uint16_t definitionVersion,
                                      uint16_t jsonSize,
                                      const uint8_t *sha256,
                                      uint8_t *slot) {
  if (config == nullptr || slot == nullptr || definitionId == 0 ||
      definitionVersion == 0 || jsonSize == 0 ||
      jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE || sha256 == nullptr) {
    return false;
  }

  int targetSlot = findSlot(definitionId, definitionVersion);
  if (targetSlot < 0) {
    targetSlot = findFreeSlot();
  }
  if (targetSlot < 0) {
    return false;
  }

  *slot = static_cast<uint8_t>(targetSlot);
  eraseChunks(*slot, true);
  return true;
}

bool DefinitionCache::writeStagedChunk(uint8_t slot,
                                       uint16_t chunkIndex,
                                       const uint8_t *data,
                                       uint16_t size) {
  if (config == nullptr || slot >= kMaxCacheSlots || data == nullptr ||
      size == 0 || size > kChunkSize || chunkIndex >= kMaxChunkCount) {
    return false;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  if (!makeChunkKey(slot, chunkIndex, true, key, sizeof(key))) {
    return false;
  }
  return config->setBlob(key, reinterpret_cast<const char *>(data), size);
}

bool DefinitionCache::loadStaged(uint8_t slot,
                                 uint32_t definitionId,
                                 uint16_t definitionVersion,
                                 char *json,
                                 size_t jsonSize,
                                 CachedDefinitionInfo *info) const {
  if (slot >= kMaxCacheSlots || definitionId == 0 || definitionVersion == 0 ||
      json == nullptr || jsonSize == 0) {
    return false;
  }

  CachedDefinitionInfo stagedInfo = {};
  stagedInfo.definitionId = definitionId;
  stagedInfo.definitionVersion = definitionVersion;
  stagedInfo.jsonSize = static_cast<uint16_t>(jsonSize - 1);
  uint16_t chunkCount = chunkCountForSize(stagedInfo.jsonSize);
  if (!loadPayload(
          slot, true, stagedInfo, chunkCount, kChunkSize, json, jsonSize)) {
    return false;
  }

  if (info != nullptr) {
    *info = stagedInfo;
  }
  return true;
}

bool DefinitionCache::commitStaged(uint8_t slot,
                                   uint32_t definitionId,
                                   uint16_t definitionVersion,
                                   uint16_t jsonSize,
                                   const uint8_t *sha256) {
  if (config == nullptr || slot >= kMaxCacheSlots || definitionId == 0 ||
      definitionVersion == 0 || jsonSize == 0 ||
      jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE || sha256 == nullptr) {
    return false;
  }

  CachedDefinitionInfo stagedInfo = {};
  stagedInfo.definitionId = definitionId;
  stagedInfo.definitionVersion = definitionVersion;
  stagedInfo.jsonSize = jsonSize;
  memcpy(stagedInfo.sha256, sha256, sizeof(stagedInfo.sha256));
  uint16_t stagedChunkCount = chunkCountForSize(jsonSize);
  uint16_t stagedChunkSize = kChunkSize;

  if (!eraseSlot(slot)) {
    return false;
  }

  uint8_t *buffer = new uint8_t[kChunkSize];
  if (buffer == nullptr) {
    return false;
  }

  bool copied = true;
  for (uint16_t i = 0; copied && i < stagedChunkCount; i++) {
    const uint16_t expectedSize = chunkPayloadSize(stagedInfo.jsonSize, i);
    char srcKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    char dstKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    copied = expectedSize > 0 &&
             makeChunkKey(slot, i, true, srcKey, sizeof(srcKey)) &&
             makeChunkKey(slot, i, false, dstKey, sizeof(dstKey)) &&
             config->getBlobSize(srcKey) == expectedSize &&
             config->getBlob(
                 srcKey, reinterpret_cast<char *>(buffer), expectedSize) &&
             config->setBlob(
                 dstKey, reinterpret_cast<const char *>(buffer), expectedSize);
  }
  delete[] buffer;

  if (!copied ||
      !saveHeader(slot, stagedInfo, stagedChunkCount, stagedChunkSize)) {
    eraseSlot(slot);
    return false;
  }

  eraseChunks(slot, true);
  config->commit();
  return true;
}

bool DefinitionCache::abortStaged(uint8_t slot) {
  if (config == nullptr || slot >= kMaxCacheSlots) {
    return false;
  }
  bool result = eraseChunks(slot, true);
  if (result) {
    config->commit();
  }
  return result;
}

bool DefinitionCache::calculateAndVerify(const char *json,
                                         uint16_t jsonSize,
                                         const uint8_t *expectedSha256,
                                         uint8_t *calculatedSha256) const {
  if (sha256Provider == nullptr || json == nullptr ||
      expectedSha256 == nullptr) {
    return false;
  }
  uint8_t digest[32] = {};
  if (!sha256Provider->calculate(reinterpret_cast<const uint8_t *>(json),
                                 jsonSize,
                                 digest,
                                 sizeof(digest))) {
    return false;
  }
  if (memcmp(digest, expectedSha256, sizeof(digest)) != 0) {
    return false;
  }
  if (calculatedSha256 != nullptr) {
    memcpy(calculatedSha256, digest, sizeof(digest));
  }
  return true;
}

bool DefinitionCache::loadHeader(uint8_t index,
                                 CachedDefinitionInfo *info,
                                 uint16_t *chunkCount,
                                 uint16_t *chunkSize) const {
  if (config == nullptr || info == nullptr || index >= kMaxCacheSlots) {
    return false;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  if (!makeHeaderKey(index, key, sizeof(key)) ||
      config->getBlobSize(key) != static_cast<int>(sizeof(BlobHeader))) {
    return false;
  }

  BlobHeader header = {};
  if (!config->getBlob(
          key, reinterpret_cast<char *>(&header), sizeof(header))) {
    return false;
  }

  if (header.magic != kCacheMagic || header.version != kCacheVersion ||
      header.definitionId == 0 || header.definitionVersion == 0 ||
      header.jsonSize == 0 ||
      header.jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE ||
      header.chunkSize != kChunkSize ||
      header.chunkCount != chunkCountForSize(header.jsonSize) ||
      header.chunkCount == 0 || header.chunkCount > kMaxChunkCount) {
    return false;
  }

  info->definitionId = header.definitionId;
  info->definitionVersion = header.definitionVersion;
  info->jsonSize = header.jsonSize;
  memcpy(info->sha256, header.sha256, sizeof(info->sha256));
  if (chunkCount != nullptr) {
    *chunkCount = header.chunkCount;
  }
  if (chunkSize != nullptr) {
    *chunkSize = header.chunkSize;
  }
  return true;
}

bool DefinitionCache::saveSlot(uint8_t index,
                               uint32_t definitionId,
                               uint16_t definitionVersion,
                               const char *json,
                               uint16_t jsonSize,
                               const uint8_t *sha256) {
  if (config == nullptr || index >= kMaxCacheSlots || definitionId == 0 ||
      definitionVersion == 0 || json == nullptr || jsonSize == 0 ||
      jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE || sha256 == nullptr) {
    return false;
  }

  if (!eraseSlot(index)) {
    return false;
  }

  const uint16_t chunkCount = chunkCountForSize(jsonSize);
  for (uint16_t i = 0; i < chunkCount; i++) {
    const uint16_t offset = i * kChunkSize;
    const uint16_t size = chunkPayloadSize(jsonSize, i);
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    if (!makeChunkKey(index, i, false, key, sizeof(key)) ||
        !config->setBlob(key, json + offset, size)) {
      eraseSlot(index);
      return false;
    }
  }

  CachedDefinitionInfo info = {};
  info.definitionId = definitionId;
  info.definitionVersion = definitionVersion;
  info.jsonSize = jsonSize;
  memcpy(info.sha256, sha256, sizeof(info.sha256));
  if (!saveHeader(index, info, chunkCount, kChunkSize)) {
    eraseSlot(index);
    return false;
  }
  config->commit();
  return true;
}

bool DefinitionCache::saveHeader(uint8_t index,
                                 const CachedDefinitionInfo &info,
                                 uint16_t chunkCount,
                                 uint16_t chunkSize) {
  if (config == nullptr || index >= kMaxCacheSlots || info.definitionId == 0 ||
      info.definitionVersion == 0 || info.jsonSize == 0 ||
      info.jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE ||
      chunkCount == 0 || chunkCount > kMaxChunkCount ||
      chunkSize != kChunkSize) {
    return false;
  }

  BlobHeader header = {};
  header.magic = kCacheMagic;
  header.version = kCacheVersion;
  header.definitionId = info.definitionId;
  header.definitionVersion = info.definitionVersion;
  header.jsonSize = info.jsonSize;
  header.chunkCount = chunkCount;
  header.chunkSize = chunkSize;
  memcpy(header.sha256, info.sha256, sizeof(header.sha256));

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  return makeHeaderKey(index, key, sizeof(key)) &&
         config->setBlob(
             key, reinterpret_cast<const char *>(&header), sizeof(header));
}

bool DefinitionCache::loadPayload(uint8_t slot,
                                  bool staged,
                                  const CachedDefinitionInfo &info,
                                  uint16_t chunkCount,
                                  uint16_t chunkSize,
                                  char *json,
                                  size_t jsonSize) const {
  if (config == nullptr || slot >= kMaxCacheSlots || json == nullptr ||
      jsonSize <= info.jsonSize || info.jsonSize == 0 ||
      info.jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE ||
      chunkSize != kChunkSize ||
      chunkCount != chunkCountForSize(info.jsonSize)) {
    return false;
  }

  for (uint16_t i = 0; i < chunkCount; i++) {
    const uint16_t expectedSize = chunkPayloadSize(info.jsonSize, i);
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    if (expectedSize == 0 || !makeChunkKey(slot, i, staged, key, sizeof(key)) ||
        config->getBlobSize(key) != expectedSize ||
        !config->getBlob(
            key, json + static_cast<size_t>(i) * kChunkSize, expectedSize)) {
      return false;
    }
  }
  json[info.jsonSize] = '\0';
  return true;
}

bool DefinitionCache::eraseChunks(uint8_t index, bool staged) {
  if (config == nullptr || index >= kMaxCacheSlots) {
    return false;
  }
  for (uint16_t i = 0; i < kMaxChunkCount; i++) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    if (makeChunkKey(index, i, staged, key, sizeof(key))) {
      config->eraseKey(key);
    }
  }
  return true;
}

bool DefinitionCache::eraseSlot(uint8_t index) {
  if (config == nullptr || index >= kMaxCacheSlots) {
    return false;
  }
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  if (makeHeaderKey(index, key, sizeof(key))) {
    config->eraseKey(key);
  }
  eraseChunks(index, false);
  config->commit();
  return true;
}

int DefinitionCache::findSlot(uint32_t definitionId,
                              uint16_t definitionVersion) const {
  for (uint8_t i = 0; i < kMaxCacheSlots; i++) {
    CachedDefinitionInfo info = {};
    if (loadHeader(i, &info) && info.definitionId == definitionId &&
        info.definitionVersion == definitionVersion) {
      return i;
    }
  }
  return -1;
}

int DefinitionCache::findFreeSlot() const {
  for (uint8_t i = 0; i < kMaxCacheSlots; i++) {
    CachedDefinitionInfo info = {};
    if (!loadHeader(i, &info)) {
      return i;
    }
  }
  return -1;
}

bool DefinitionCache::makeHeaderKey(uint8_t index,
                                    char *output,
                                    size_t outputSize) {
  if (output == nullptr || outputSize == 0 || index >= kMaxCacheSlots) {
    return false;
  }
  int written = snprintf(output, outputSize, "spld%u", index);
  return written > 0 && static_cast<size_t>(written) < outputSize;
}

bool DefinitionCache::makeChunkKey(uint8_t index,
                                   uint16_t chunkIndex,
                                   bool staged,
                                   char *output,
                                   size_t outputSize) {
  if (output == nullptr || outputSize == 0 || index >= kMaxCacheSlots ||
      chunkIndex >= kMaxChunkCount) {
    return false;
  }
  int written = snprintf(output,
                         outputSize,
                         staged ? "splds%uc%u" : "spld%uc%u",
                         index,
                         chunkIndex);
  return written > 0 && static_cast<size_t>(written) < outputSize;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
