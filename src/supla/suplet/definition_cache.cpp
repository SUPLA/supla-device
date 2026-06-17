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
#include <string.h>
#include <supla/suplet/definition_cache.h>

namespace Supla {
namespace Suplet {

namespace {

constexpr uint32_t kCacheMagic = 0x5344504C;  // SDPL
constexpr uint8_t kCacheVersion = 2;
constexpr uint8_t kMaxCacheSlots = SUPLA_SUPLET_MAX_CACHED_DEFINITIONS < 4
? SUPLA_SUPLET_MAX_CACHED_DEFINITIONS
: 4;

struct BlobHeader {
  uint32_t magic = 0;
  uint8_t version = 0;
  uint8_t reserved = 0;
  uint16_t jsonSize = 0;
  uint32_t definitionId = 0;
  uint16_t definitionVersion = 0;
  uint8_t sha256[32] = {};
};

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
  if (jsonSize == 0 || jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE) {
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

  SlotData slotData = {};
  if (!loadSlot(static_cast<uint8_t>(slot), &slotData) ||
      jsonSize <= slotData.jsonSize ||
      !calculateAndVerify(
          slotData.json, slotData.jsonSize, slotData.sha256, nullptr)) {
    clearSlot(&slotData);
    return false;
  }

  memcpy(json, slotData.json, slotData.jsonSize);
  json[slotData.jsonSize] = '\0';
  if (info != nullptr) {
    info->definitionId = slotData.definitionId;
    info->definitionVersion = slotData.definitionVersion;
    info->jsonSize = slotData.jsonSize;
    memcpy(info->sha256, slotData.sha256, sizeof(info->sha256));
  }
  clearSlot(&slotData);
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
  if (info == nullptr) {
    return false;
  }
  SlotData slotData = {};
  if (!loadSlot(index, &slotData)) {
    return false;
  }
  info->definitionId = slotData.definitionId;
  info->definitionVersion = slotData.definitionVersion;
  info->jsonSize = slotData.jsonSize;
  memcpy(info->sha256, slotData.sha256, sizeof(info->sha256));
  clearSlot(&slotData);
  return true;
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

bool DefinitionCache::loadSlot(uint8_t index, SlotData *slot) const {
  if (config == nullptr || slot == nullptr || index >= kMaxCacheSlots) {
    return false;
  }
  clearSlot(slot);
  int blobSize = config->getBlobSize(slotKey(index));
  if (blobSize <= static_cast<int>(sizeof(BlobHeader)) ||
      blobSize > static_cast<int>(sizeof(BlobHeader) +
                                  SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1)) {
    return false;
  }

  uint8_t *buffer = new uint8_t[blobSize];
  if (buffer == nullptr) {
    return false;
  }

  bool loaded = config->getBlob(
      slotKey(index), reinterpret_cast<char *>(buffer), blobSize);
  if (!loaded) {
    delete[] buffer;
    return false;
  }

  BlobHeader header = {};
  memcpy(&header, buffer, sizeof(header));
  if (header.magic != kCacheMagic || header.version != kCacheVersion ||
      header.definitionId == 0 || header.definitionVersion == 0 ||
      header.jsonSize == 0 ||
      header.jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE ||
      blobSize != static_cast<int>(sizeof(BlobHeader) + header.jsonSize + 1)) {
    delete[] buffer;
    return false;
  }

  const char *json =
      reinterpret_cast<const char *>(buffer + sizeof(BlobHeader));
  if (json[header.jsonSize] != '\0') {
    delete[] buffer;
    return false;
  }

  slot->json = new char[header.jsonSize + 1];
  if (slot->json == nullptr) {
    delete[] buffer;
    return false;
  }

  slot->definitionId = header.definitionId;
  slot->definitionVersion = header.definitionVersion;
  slot->jsonSize = header.jsonSize;
  memcpy(slot->sha256, header.sha256, sizeof(slot->sha256));
  memcpy(slot->json, json, header.jsonSize + 1);
  delete[] buffer;
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

  const size_t blobSize = sizeof(BlobHeader) + jsonSize + 1;
  uint8_t *buffer = new uint8_t[blobSize];
  if (buffer == nullptr) {
    return false;
  }

  BlobHeader header = {};
  header.magic = kCacheMagic;
  header.version = kCacheVersion;
  header.definitionId = definitionId;
  header.definitionVersion = definitionVersion;
  header.jsonSize = jsonSize;
  memcpy(header.sha256, sha256, sizeof(header.sha256));
  memcpy(buffer, &header, sizeof(header));
  memcpy(buffer + sizeof(header), json, jsonSize);
  buffer[blobSize - 1] = '\0';

  bool result = config->setBlob(
      slotKey(index), reinterpret_cast<const char *>(buffer), blobSize);
  delete[] buffer;
  if (result) {
    config->commit();
  }
  return result;
}

void DefinitionCache::clearSlot(SlotData *slot) {
  if (slot == nullptr) {
    return;
  }
  if (slot->json != nullptr) {
    delete[] slot->json;
  }
  *slot = SlotData();
}

bool DefinitionCache::eraseSlot(uint8_t index) {
  if (config == nullptr || index >= kMaxCacheSlots) {
    return false;
  }
  bool result = config->eraseKey(slotKey(index));
  if (result) {
    config->commit();
  }
  return result;
}

int DefinitionCache::findSlot(uint32_t definitionId,
                              uint16_t definitionVersion) const {
  for (uint8_t i = 0; i < kMaxCacheSlots; i++) {
    SlotData slot = {};
    bool found = loadSlot(i, &slot) && slot.definitionId == definitionId &&
                 slot.definitionVersion == definitionVersion;
    clearSlot(&slot);
    if (found) {
      return i;
    }
  }
  return -1;
}

int DefinitionCache::findFreeSlot() const {
  for (uint8_t i = 0; i < kMaxCacheSlots; i++) {
    SlotData slot = {};
    bool used = loadSlot(i, &slot);
    clearSlot(&slot);
    if (!used) {
      return i;
    }
  }
  return -1;
}

const char *DefinitionCache::slotKey(uint8_t index) {
  switch (index) {
    case 0:
      return "spld0";
    case 1:
      return "spld1";
    case 2:
      return "spld2";
    case 3:
      return "spld3";
    default:
      return "";
  }
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
