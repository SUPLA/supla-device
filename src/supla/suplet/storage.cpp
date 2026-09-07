// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <string.h>
#include <supla/storage/config.h>
#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <supla/suplet/storage.h>

namespace Supla {
namespace Suplet {

namespace {

constexpr uint8_t kSupletStorageVersion = 4;
constexpr uint8_t kSupletStorageVersionV3 = 3;
constexpr uint8_t kDeletedSlot = 0;
constexpr uint8_t kVariantA = 1;
constexpr uint8_t kVariantB = 2;

uint8_t otherVariant(uint8_t variant) {
  return variant == kVariantA ? kVariantB : kVariantA;
}

uint16_t artifactChunkCount(uint32_t size) {
  return static_cast<uint16_t>(
      (size + SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE - 1) /
      SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE);
}

uint16_t artifactChunkSize(uint32_t size, uint16_t chunkIndex) {
  const uint32_t offset =
      static_cast<uint32_t>(chunkIndex) *
      SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE;
  if (offset >= size) {
    return 0;
  }
  const uint32_t remaining = size - offset;
  return remaining > SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE
             ? SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE
             : static_cast<uint16_t>(remaining);
}

uint32_t updateCrc32(uint32_t crc, const uint8_t *data, size_t size) {
  for (size_t i = 0; i < size; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
  }
  return crc;
}

bool channelMapsEqual(const ChannelMap &left, const ChannelMap &right) {
  if (left.getCount() != right.getCount()) {
    return false;
  }
  for (uint8_t i = 0; i < left.getCount(); i++) {
    const auto mapping = left.getMapping(i);
    if (mapping == nullptr ||
        right.getChannelNumber(mapping->channelId) != mapping->channelNumber) {
      return false;
    }
  }
  return true;
}

bool storedRecordMatches(const InstanceRecord &record,
                         const InstanceRecord &stored) {
  if (record.instanceId != stored.instanceId ||
      record.subDeviceId != stored.subDeviceId ||
      record.definitionId != stored.definitionId ||
      record.definitionVersion != stored.definitionVersion ||
      record.revision != stored.revision ||
      record.configSize != stored.configSize ||
      record.artifactSize != stored.artifactSize ||
      record.artifactCrc32 != stored.artifactCrc32 ||
      !channelMapsEqual(record.channelMap, stored.channelMap)) {
    return false;
  }
  return record.config == nullptr ||
         (stored.config != nullptr &&
          memcmp(record.config, stored.config, record.configSize) == 0);
}

}  // namespace

InstanceRecord::InstanceRecord() {
}

InstanceRecord::InstanceRecord(const InstanceRecord &other) {
  *this = other;
}

InstanceRecord &InstanceRecord::operator=(const InstanceRecord &other) {
  if (this == &other) {
    return *this;
  }

  clearConfig();
  instanceId = other.instanceId;
  definitionId = other.definitionId;
  definitionVersion = other.definitionVersion;
  revision = other.revision;
  subDeviceId = other.subDeviceId;
  configSize = 0;
  artifactSize = other.artifactSize;
  artifactCrc32 = other.artifactCrc32;
  artifactReader = other.artifactReader;
  channelMap = other.channelMap;
  if (other.config == nullptr) {
    configSize = other.configSize;
  } else if (!setConfig(other.config, other.configSize)) {
    clearConfig();
  }
  return *this;
}

InstanceRecord::~InstanceRecord() {
  clearConfig();
}

bool InstanceRecord::setConfig(const uint8_t *data, uint16_t size) {
  if (size > SUPLA_SUPLET_MAX_CONFIG_SIZE || (size > 0 && data == nullptr)) {
    return false;
  }

  clearConfig();
  if (size == 0) {
    return true;
  }

  config = new uint8_t[size];
  if (config == nullptr) {
    return false;
  }
  memcpy(config, data, size);
  configSize = size;
  return true;
}

void InstanceRecord::clearConfig() {
  if (config != nullptr) {
    delete[] config;
    config = nullptr;
  }
  configSize = 0;
}

InstanceTable::InstanceTable() {
}

InstanceTable::~InstanceTable() {
  clear();
}

uint8_t InstanceTable::getCount() const {
  return count;
}

void InstanceTable::clear() {
  if (records != nullptr) {
    delete[] records;
    records = nullptr;
  }
  count = 0;
}

bool InstanceTable::resize(uint8_t newCount) {
  if (newCount > SUPLA_SUPLET_MAX_INSTANCES) {
    return false;
  }
  if (newCount == count) {
    return true;
  }
  if (newCount == 0) {
    clear();
    return true;
  }

  InstanceRecord *newRecords = new InstanceRecord[newCount];
  if (newRecords == nullptr) {
    return false;
  }

  uint8_t copyCount = count < newCount ? count : newCount;
  for (uint8_t i = 0; i < copyCount; i++) {
    newRecords[i] = records[i];
  }
  delete[] records;
  records = newRecords;
  count = newCount;
  return true;
}

bool InstanceTable::add(const InstanceRecord &record) {
  if (record.instanceId == 0 || record.subDeviceId == 0 ||
      record.subDeviceId != record.instanceId ||
      record.configSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
      record.artifactSize > SUPLA_SUPLET_MAX_ARTIFACT_SIZE ||
      count >= SUPLA_SUPLET_MAX_INSTANCES ||
      findByInstanceId(record.instanceId) != nullptr ||
      findBySubDeviceId(record.subDeviceId) != nullptr) {
    return false;
  }

  uint8_t oldCount = count;
  if (!resize(count + 1)) {
    return false;
  }
  records[oldCount] = record;
  return true;
}

bool InstanceTable::removeByInstanceId(uint8_t instanceId) {
  for (uint8_t i = 0; i < count; i++) {
    if (records[i].instanceId == instanceId) {
      for (uint8_t j = i; j + 1 < count; j++) {
        records[j] = records[j + 1];
      }
      return resize(count - 1);
    }
  }
  return false;
}

InstanceRecord *InstanceTable::findByInstanceId(uint8_t instanceId) {
  for (uint8_t i = 0; i < count; i++) {
    if (records[i].instanceId == instanceId) {
      return &records[i];
    }
  }
  return nullptr;
}

const InstanceRecord *InstanceTable::findByInstanceId(
    uint8_t instanceId) const {
  for (uint8_t i = 0; i < count; i++) {
    if (records[i].instanceId == instanceId) {
      return &records[i];
    }
  }
  return nullptr;
}

InstanceRecord *InstanceTable::findBySubDeviceId(uint8_t subDeviceId) {
  for (uint8_t i = 0; i < count; i++) {
    if (records[i].subDeviceId == subDeviceId) {
      return &records[i];
    }
  }
  return nullptr;
}

const InstanceRecord *InstanceTable::findBySubDeviceId(
    uint8_t subDeviceId) const {
  for (uint8_t i = 0; i < count; i++) {
    if (records[i].subDeviceId == subDeviceId) {
      return &records[i];
    }
  }
  return nullptr;
}

InstanceRecord *InstanceTable::getRecord(uint8_t index) {
  if (index >= count) {
    return nullptr;
  }
  return &records[index];
}

const InstanceRecord *InstanceTable::getRecord(uint8_t index) const {
  if (index >= count) {
    return nullptr;
  }
  return &records[index];
}

Storage::Storage(Supla::Config *config) : config(config) {
}

bool Storage::load(InstanceTable *table) {
  if (config == nullptr || table == nullptr) {
    return false;
  }

  table->clear();
  bool loadedAny = false;
  for (uint16_t i = 1; i <= SUPLA_SUPLET_MAX_INSTANCE_ID; i++) {
    InstanceRecord record = {};
    if (loadInstance(static_cast<uint8_t>(i), &record)) {
      if (!table->add(record)) {
        table->clear();
        return false;
      }
      loadedAny = true;
    }
  }
  return loadedAny;
}

bool Storage::loadIndex(InstanceTable *table) {
  if (config == nullptr || table == nullptr) {
    return false;
  }

  table->clear();
  bool loadedAny = false;
  for (uint16_t i = 1; i <= SUPLA_SUPLET_MAX_INSTANCE_ID; i++) {
    InstanceRecord record = {};
    uint8_t activeVariant = kDeletedSlot;
    if (loadActiveVariant(
            static_cast<uint8_t>(i), &activeVariant, &record, false, false)) {
      if (!table->add(record)) {
        table->clear();
        return false;
      }
      loadedAny = true;
    }
  }
  return loadedAny;
}

bool Storage::save(const InstanceTable &table,
                   const ArtifactStorageHandle *stagedArtifact) {
  if (config == nullptr) {
    return false;
  }

  if (stagedArtifact != nullptr &&
      (!stagedArtifact->valid || stagedArtifact->instanceId == 0 ||
       stagedArtifact->artifactSize != stagedArtifact->receivedSize ||
       table.findByInstanceId(stagedArtifact->instanceId) == nullptr)) {
    return false;
  }

  bool present[SUPLA_SUPLET_MAX_INSTANCE_ID + 1] = {};
  for (uint8_t i = 0; i < table.getCount(); i++) {
    auto record = table.getRecord(i);
    if (record == nullptr || record->instanceId == 0 ||
        record->subDeviceId != record->instanceId ||
        record->configSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
        record->artifactSize > SUPLA_SUPLET_MAX_ARTIFACT_SIZE) {
      return false;
    }
    uint8_t activeVariant = kDeletedSlot;
    char actKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    makeActKey(record->instanceId, actKey);
    config->getUInt8(actKey, &activeVariant);
    uint8_t targetVariant = activeVariant == kVariantA ? kVariantB : kVariantA;
    uint8_t oldVariant =
        activeVariant == kVariantA || activeVariant == kVariantB ? activeVariant
                                                                 : kDeletedSlot;

    const ArtifactStorageHandle *recordStagedArtifact =
        stagedArtifact != nullptr &&
                stagedArtifact->instanceId == record->instanceId
            ? stagedArtifact
            : nullptr;
    if (recordStagedArtifact == nullptr &&
        (activeVariant == kVariantA || activeVariant == kVariantB)) {
      InstanceRecord stored = {};
      if (loadVariant(record->instanceId,
                      activeVariant,
                      &stored,
                      record->config != nullptr) &&
          storedRecordMatches(*record, stored)) {
        present[record->instanceId] = true;
        continue;
      }
    }
    if (recordStagedArtifact != nullptr &&
        recordStagedArtifact->variant != targetVariant) {
      return false;
    }
    if (!saveVariant(*record, targetVariant, recordStagedArtifact)) {
      return false;
    }
    if (!config->setUInt8(actKey, targetVariant)) {
      return false;
    }
    config->commit();

    InstanceRecord check = {};
    if (oldVariant != kDeletedSlot &&
        loadVariant(record->instanceId, targetVariant, &check)) {
      eraseVariant(record->instanceId, oldVariant);
      config->commit();
    }
    present[record->instanceId] = true;
  }

  for (uint16_t i = 1; i <= SUPLA_SUPLET_MAX_INSTANCE_ID; i++) {
    uint8_t instanceId = static_cast<uint8_t>(i);
    if (!present[instanceId] && slotExists(instanceId)) {
      eraseInstance(instanceId);
    }
  }
  return true;
}

bool Storage::erase() {
  if (config == nullptr) {
    return false;
  }
  for (uint16_t i = 1; i <= SUPLA_SUPLET_MAX_INSTANCE_ID; i++) {
    uint8_t instanceId = static_cast<uint8_t>(i);
    if (slotExists(instanceId)) {
      eraseInstance(instanceId);
    }
  }
  return true;
}

bool Storage::loadInstance(uint8_t instanceId, InstanceRecord *record) {
  uint8_t activeVariant = kDeletedSlot;
  return loadActiveVariant(instanceId, &activeVariant, record, true, true);
}

bool Storage::loadActiveVariant(uint8_t instanceId,
                                uint8_t *activeVariant,
                                InstanceRecord *record,
                                bool loadConfig,
                                bool cleanup) {
  if (record == nullptr) {
    return false;
  }

  char actKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeActKey(instanceId, actKey);
  uint8_t currentActiveVariant = kDeletedSlot;
  if (!config->getUInt8(actKey, &currentActiveVariant) ||
      currentActiveVariant == kDeletedSlot ||
      (currentActiveVariant != kVariantA &&
       currentActiveVariant != kVariantB)) {
    if (cleanup && slotExists(instanceId)) {
      cleanupLoadedInstance(instanceId, kDeletedSlot, kDeletedSlot);
    }
    return false;
  }

  if (loadVariant(instanceId, currentActiveVariant, record, loadConfig)) {
    if (cleanup) {
      cleanupLoadedInstance(
          instanceId, currentActiveVariant, currentActiveVariant);
    }
    if (activeVariant != nullptr) {
      *activeVariant = currentActiveVariant;
    }
    return true;
  }

  uint8_t fallbackVariant = otherVariant(currentActiveVariant);
  if (loadVariant(instanceId, fallbackVariant, record, loadConfig)) {
    if (cleanup) {
      cleanupLoadedInstance(instanceId, currentActiveVariant, fallbackVariant);
    }
    if (activeVariant != nullptr) {
      *activeVariant = fallbackVariant;
    }
    return true;
  }

  if (cleanup) {
    cleanupLoadedInstance(instanceId, currentActiveVariant, kDeletedSlot);
  }
  return false;
}

bool Storage::loadVariant(uint8_t instanceId,
                          uint8_t variant,
                          InstanceRecord *record) const {
  return loadVariant(instanceId, variant, record, true);
}

bool Storage::loadVariant(uint8_t instanceId,
                          uint8_t variant,
                          InstanceRecord *record,
                          bool loadConfig) const {
  if (!loadVariantMetadata(instanceId, variant, record, loadConfig)) {
    return false;
  }
  return validateArtifact(instanceId,
                          variant,
                          record->artifactSize,
                          record->artifactCrc32);
}

bool Storage::loadVariantMetadata(uint8_t instanceId,
                                  uint8_t variant,
                                  InstanceRecord *record,
                                  bool loadConfig) const {
  if (config == nullptr || record == nullptr ||
      (variant != kVariantA && variant != kVariantB)) {
    return false;
  }

  char headerKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeHeaderKey(instanceId, variant, headerKey);
  StoredInstanceHeader header = {};
  const int storedHeaderSize = config->getBlobSize(headerKey);
  if (storedHeaderSize == static_cast<int>(sizeof(header))) {
    if (!config->getBlob(
            headerKey, reinterpret_cast<char *>(&header), sizeof(header))) {
      return false;
    }
  } else if (storedHeaderSize ==
             static_cast<int>(sizeof(StoredInstanceHeaderV3))) {
    StoredInstanceHeaderV3 legacy = {};
    if (!config->getBlob(headerKey,
                         reinterpret_cast<char *>(&legacy),
                         sizeof(legacy)) ||
        legacy.version != kSupletStorageVersionV3) {
      return false;
    }
    header.version = kSupletStorageVersion;
    header.definitionId = legacy.definitionId;
    header.definitionVersion = legacy.definitionVersion;
    header.channelCount = legacy.channelCount;
    header.configSize = legacy.configSize;
  } else {
    return false;
  }

  if (header.version != kSupletStorageVersion || header.definitionId == 0 ||
      header.definitionVersion == 0 ||
      header.channelCount > SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE ||
      header.configSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
      header.artifactSize > SUPLA_SUPLET_MAX_ARTIFACT_SIZE) {
    return false;
  }

  char channelMapKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeChannelMapKey(instanceId, variant, channelMapKey);
  const size_t channelMapSize =
      header.channelCount * sizeof(StoredChannelMapping);
  if (config->getBlobSize(channelMapKey) != static_cast<int>(channelMapSize)) {
    return false;
  }

  char configKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeConfigKey(instanceId, variant, configKey);
  if (config->getBlobSize(configKey) != header.configSize) {
    return false;
  }

  InstanceRecord loaded = {};
  loaded.instanceId = instanceId;
  loaded.subDeviceId = instanceId;
  loaded.definitionId = header.definitionId;
  loaded.definitionVersion = header.definitionVersion;
  loaded.revision = header.revision;
  loaded.configSize = header.configSize;
  loaded.artifactSize = header.artifactSize;
  loaded.artifactCrc32 = header.artifactCrc32;
  loaded.artifactReader = this;

  if (channelMapSize > 0) {
    StoredChannelMapping stored[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
    if (!config->getBlob(
            channelMapKey, reinterpret_cast<char *>(stored), channelMapSize)) {
      return false;
    }
    for (uint8_t i = 0; i < header.channelCount; i++) {
      if (stored[i].channelId == kInvalidChannelId ||
          !loaded.channelMap.add(stored[i].channelId,
                                 stored[i].channelNumber)) {
        return false;
      }
    }
  }

  if (loadConfig && header.configSize > 0) {
    uint8_t *buffer = new uint8_t[header.configSize];
    if (buffer == nullptr) {
      return false;
    }
    bool loadedConfig =
        config->getBlob(
            configKey, reinterpret_cast<char *>(buffer), header.configSize) &&
        loaded.setConfig(buffer, header.configSize);
    delete[] buffer;
    if (!loadedConfig) {
      return false;
    }
  } else if (!loadConfig) {
    loaded.configSize = header.configSize;
  }

  *record = loaded;
  return true;
}

bool Storage::saveVariant(
    const InstanceRecord &record,
    uint8_t variant,
    const ArtifactStorageHandle *stagedArtifact) {
  if (config == nullptr || (variant != kVariantA && variant != kVariantB) ||
      record.instanceId == 0 || record.subDeviceId != record.instanceId ||
      record.configSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
      record.artifactSize > SUPLA_SUPLET_MAX_ARTIFACT_SIZE ||
      record.channelMap.getCount() > SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE) {
    return false;
  }

  const uint8_t *configData = record.config;
  InstanceRecord loadedConfig = {};
  if (configData == nullptr && record.configSize > 0) {
    char actKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    makeActKey(record.instanceId, actKey);
    uint8_t activeVariant = kDeletedSlot;
    if (config->getUInt8(actKey, &activeVariant) &&
        (activeVariant == kVariantA || activeVariant == kVariantB)) {
      if (!loadVariant(record.instanceId, activeVariant, &loadedConfig, true)) {
        loadVariant(record.instanceId,
                    otherVariant(activeVariant),
                    &loadedConfig,
                    true);
      }
    }

    if (loadedConfig.config == nullptr ||
        loadedConfig.configSize != record.configSize) {
      return false;
    }
    configData = loadedConfig.config;
  }

  StoredChannelMapping stored[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  for (uint8_t i = 0; i < record.channelMap.getCount(); i++) {
    auto mapping = record.channelMap.getMapping(i);
    if (mapping == nullptr || mapping->channelId == kInvalidChannelId ||
        mapping->channelNumber < 0 || mapping->channelNumber > 255) {
      return false;
    }
    stored[i].channelId = mapping->channelId;
    stored[i].channelNumber = static_cast<uint8_t>(mapping->channelNumber);
  }

  char channelMapKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeChannelMapKey(record.instanceId, variant, channelMapKey);
  size_t channelMapSize =
      record.channelMap.getCount() * sizeof(StoredChannelMapping);
  if (!config->setBlob(channelMapKey,
                       reinterpret_cast<const char *>(stored),
                       channelMapSize)) {
    return false;
  }

  char configKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeConfigKey(record.instanceId, variant, configKey);
  uint8_t emptyConfig = 0;
  if (configData == nullptr) {
    configData = &emptyConfig;
  }
  if (!config->setBlob(configKey,
                       reinterpret_cast<const char *>(configData),
                       record.configSize)) {
    return false;
  }

  // The current A/B layout duplicates the active artifact so config-only
  // updates can switch the complete instance atomically. TODO: move
  // artifacts to independent generations/storage when that backend exists.
  if (stagedArtifact != nullptr) {
    if (!stagedArtifact->valid ||
        stagedArtifact->instanceId != record.instanceId ||
        stagedArtifact->variant != variant ||
        stagedArtifact->artifactSize != record.artifactSize ||
        stagedArtifact->receivedSize != record.artifactSize ||
        stagedArtifactCrc32(*stagedArtifact) != record.artifactCrc32 ||
        !validateArtifact(record.instanceId,
                          variant,
                          record.artifactSize,
                          record.artifactCrc32)) {
      return false;
    }
  } else if (!copyActiveArtifact(record, variant)) {
    return false;
  }

  StoredInstanceHeader header = {};
  header.version = kSupletStorageVersion;
  header.definitionId = record.definitionId;
  header.definitionVersion = record.definitionVersion;
  header.channelCount = record.channelMap.getCount();
  header.configSize = record.configSize;
  header.revision = record.revision;
  header.artifactSize = record.artifactSize;
  header.artifactCrc32 = record.artifactCrc32;

  char headerKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeHeaderKey(record.instanceId, variant, headerKey);
  return config->setBlob(
      headerKey, reinterpret_cast<const char *>(&header), sizeof(header));
}

bool Storage::beginStagedArtifact(uint8_t instanceId,
                                  uint32_t artifactSize,
                                  ArtifactStorageHandle *handle) {
  if (config == nullptr || handle == nullptr || instanceId == 0 ||
      artifactSize > SUPLA_SUPLET_MAX_ARTIFACT_SIZE) {
    return false;
  }

  uint8_t activeVariant = kDeletedSlot;
  getActiveVariant(instanceId, &activeVariant);
  const uint8_t targetVariant =
      activeVariant == kVariantA ? kVariantB : kVariantA;
  if (!eraseVariant(instanceId, targetVariant)) {
    return false;
  }

  *handle = ArtifactStorageHandle();
  handle->valid = true;
  handle->instanceId = instanceId;
  handle->variant = targetVariant;
  handle->artifactSize = artifactSize;
  return true;
}

bool Storage::writeStagedArtifactChunk(ArtifactStorageHandle *handle,
                                       const uint8_t *data,
                                       uint16_t size) {
  if (config == nullptr || handle == nullptr || !handle->valid ||
      data == nullptr || size == 0 ||
      size != artifactChunkSize(handle->artifactSize, handle->chunkIndex) ||
      handle->receivedSize + size > handle->artifactSize) {
    return false;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeArtifactChunkKey(
      handle->instanceId, handle->variant, handle->chunkIndex, key);
  if (!config->setBlob(key, reinterpret_cast<const char *>(data), size)) {
    return false;
  }
  handle->crc32State = updateCrc32(handle->crc32State, data, size);
  handle->receivedSize += size;
  handle->chunkIndex++;
  return true;
}

bool Storage::abortStagedArtifact(ArtifactStorageHandle *handle) {
  if (handle == nullptr || !handle->valid) {
    return false;
  }
  const bool result = eraseVariant(handle->instanceId, handle->variant);
  if (result) {
    config->commit();
  }
  *handle = ArtifactStorageHandle();
  return result;
}

uint32_t Storage::stagedArtifactCrc32(
    const ArtifactStorageHandle &handle) {
  return handle.crc32State ^ 0xFFFFFFFFUL;
}

bool Storage::getActiveVariant(uint8_t instanceId, uint8_t *variant) const {
  if (config == nullptr || instanceId == 0 || variant == nullptr) {
    return false;
  }
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeActKey(instanceId, key);
  uint8_t loaded = kDeletedSlot;
  if (!config->getUInt8(key, &loaded) ||
      (loaded != kVariantA && loaded != kVariantB)) {
    return false;
  }
  *variant = loaded;
  return true;
}

bool Storage::validateArtifact(uint8_t instanceId,
                               uint8_t variant,
                               uint32_t artifactSize,
                               uint32_t expectedCrc32) const {
  if (artifactSize == 0) {
    return expectedCrc32 == 0;
  }
  if (config == nullptr || instanceId == 0 ||
      (variant != kVariantA && variant != kVariantB) ||
      artifactSize > SUPLA_SUPLET_MAX_ARTIFACT_SIZE) {
    return false;
  }

  uint8_t *buffer = new uint8_t[SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE];
  if (buffer == nullptr) {
    return false;
  }
  uint32_t crc = 0xFFFFFFFFUL;
  const uint16_t chunkCount = artifactChunkCount(artifactSize);
  bool valid = true;
  for (uint16_t i = 0; valid && i < chunkCount; i++) {
    const uint16_t size = artifactChunkSize(artifactSize, i);
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    makeArtifactChunkKey(instanceId, variant, i, key);
    valid = config->getBlobSize(key) == size &&
            config->getBlob(
                key, reinterpret_cast<char *>(buffer), size);
    if (valid) {
      crc = updateCrc32(crc, buffer, size);
    }
  }
  delete[] buffer;
  return valid && (crc ^ 0xFFFFFFFFUL) == expectedCrc32;
}

bool Storage::copyActiveArtifact(const InstanceRecord &record,
                                 uint8_t targetVariant) {
  if (!eraseArtifactChunks(record.instanceId, targetVariant)) {
    return false;
  }
  if (record.artifactSize == 0) {
    return record.artifactCrc32 == 0;
  }

  uint8_t activeVariant = kDeletedSlot;
  if (!getActiveVariant(record.instanceId, &activeVariant) ||
      !validateArtifact(record.instanceId,
                        activeVariant,
                        record.artifactSize,
                        record.artifactCrc32)) {
    return false;
  }

  uint8_t *buffer = new uint8_t[SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE];
  if (buffer == nullptr) {
    return false;
  }
  const uint16_t chunkCount = artifactChunkCount(record.artifactSize);
  bool copied = true;
  for (uint16_t i = 0; copied && i < chunkCount; i++) {
    const uint16_t size = artifactChunkSize(record.artifactSize, i);
    char sourceKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    char targetKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    makeArtifactChunkKey(record.instanceId, activeVariant, i, sourceKey);
    makeArtifactChunkKey(record.instanceId, targetVariant, i, targetKey);
    copied = config->getBlob(
                 sourceKey, reinterpret_cast<char *>(buffer), size) &&
             config->setBlob(
                 targetKey, reinterpret_cast<const char *>(buffer), size);
  }
  delete[] buffer;
  return copied;
}

bool Storage::readArtifact(uint8_t instanceId,
                           uint32_t offset,
                           uint8_t *data,
                           uint16_t size) const {
  if (data == nullptr || size == 0) {
    return false;
  }
  uint8_t variant = kDeletedSlot;
  InstanceRecord record = {};
  if (!getActiveVariant(instanceId, &variant)) {
    return false;
  }
  if (loadVariantMetadata(instanceId, variant, &record, false)) {
    if (offset > record.artifactSize ||
        size > record.artifactSize - offset) {
      return false;
    }
    if (readArtifactFromVariant(instanceId,
                                variant,
                                record,
                                offset,
                                data,
                                size)) {
      return true;
    }
  }
  variant = otherVariant(variant);
  if (!loadVariant(instanceId, variant, &record, false) ||
      !readArtifactFromVariant(
          instanceId, variant, record, offset, data, size)) {
    return false;
  }
  return true;
}

bool Storage::readArtifactFromVariant(uint8_t instanceId,
                                      uint8_t variant,
                                      const InstanceRecord &record,
                                      uint32_t offset,
                                      uint8_t *data,
                                      uint16_t size) const {
  if (offset > record.artifactSize || size > record.artifactSize - offset) {
    return false;
  }

  uint8_t *chunk = new uint8_t[SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE];
  if (chunk == nullptr) {
    return false;
  }
  uint16_t copied = 0;
  bool result = true;
  while (result && copied < size) {
    const uint32_t currentOffset = offset + copied;
    const uint16_t chunkIndex = static_cast<uint16_t>(
        currentOffset / SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE);
    const uint16_t offsetInChunk = static_cast<uint16_t>(
        currentOffset % SUPLA_SUPLET_ARTIFACT_STORAGE_CHUNK_SIZE);
    const uint16_t storedSize =
        artifactChunkSize(record.artifactSize, chunkIndex);
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    makeArtifactChunkKey(instanceId, variant, chunkIndex, key);
    if (config->getBlobSize(key) != storedSize ||
        !config->getBlob(key, reinterpret_cast<char *>(chunk), storedSize)) {
      result = false;
      break;
    }
    uint16_t toCopy = storedSize - offsetInChunk;
    if (toCopy > size - copied) {
      toCopy = size - copied;
    }
    memcpy(data + copied, chunk + offsetInChunk, toCopy);
    copied += toCopy;
  }
  delete[] chunk;
  return result;
}

bool Storage::eraseVariant(uint8_t instanceId, uint8_t variant) {
  if (config == nullptr || (variant != kVariantA && variant != kVariantB)) {
    return false;
  }
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeHeaderKey(instanceId, variant, key);
  config->eraseKey(key);
  makeChannelMapKey(instanceId, variant, key);
  config->eraseKey(key);
  makeConfigKey(instanceId, variant, key);
  config->eraseKey(key);
  return eraseArtifactChunks(instanceId, variant);
}

bool Storage::eraseArtifactChunks(uint8_t instanceId, uint8_t variant) {
  if (config == nullptr || instanceId == 0 ||
      (variant != kVariantA && variant != kVariantB)) {
    return false;
  }
  const uint16_t maxChunkCount = artifactChunkCount(
      static_cast<uint32_t>(SUPLA_SUPLET_MAX_ARTIFACT_SIZE));
  for (uint16_t i = 0; i < maxChunkCount; i++) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    makeArtifactChunkKey(instanceId, variant, i, key);
    config->eraseKey(key);
  }
  return true;
}

bool Storage::eraseInstance(uint8_t instanceId) {
  if (config == nullptr || instanceId == 0) {
    return false;
  }
  char actKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeActKey(instanceId, actKey);
  config->setUInt8(actKey, kDeletedSlot);
  config->commit();
  eraseVariant(instanceId, kVariantA);
  eraseVariant(instanceId, kVariantB);
  config->eraseKey(actKey);
  config->commit();
  return true;
}

bool Storage::cleanupLoadedInstance(uint8_t instanceId,
                                    uint8_t activeVariant,
                                    uint8_t loadedVariant) {
  if (config == nullptr || instanceId == 0) {
    return false;
  }

  char actKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeActKey(instanceId, actKey);

  if (loadedVariant == kDeletedSlot) {
    config->setUInt8(actKey, kDeletedSlot);
    config->commit();
    eraseVariant(instanceId, kVariantA);
    eraseVariant(instanceId, kVariantB);
    config->eraseKey(actKey);
    config->commit();
    return true;
  }

  if (activeVariant != loadedVariant) {
    config->setUInt8(actKey, loadedVariant);
    config->commit();
  }

  eraseVariant(instanceId, otherVariant(loadedVariant));
  config->commit();
  return true;
}

bool Storage::slotExists(uint8_t instanceId) const {
  if (config == nullptr || instanceId == 0) {
    return false;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeActKey(instanceId, key);
  uint8_t act = 0;
  if (config->getUInt8(key, &act)) {
    return true;
  }
  makeHeaderKey(instanceId, kVariantA, key);
  if (config->getBlobSize(key) >= 0) {
    return true;
  }
  makeHeaderKey(instanceId, kVariantB, key);
  if (config->getBlobSize(key) >= 0) {
    return true;
  }
  return false;
}

void Storage::makeActKey(uint8_t instanceId, char *output) const {
  char suffix[16] = {};
  snprintf(suffix, sizeof(suffix), "splt_act");
  Supla::Config::generateKey(output, instanceId, suffix);
}

void Storage::makeHeaderKey(uint8_t instanceId,
                            uint8_t variant,
                            char *output) const {
  char suffix[16] = {};
  snprintf(suffix, sizeof(suffix), "splt_%u", variant);
  Supla::Config::generateKey(output, instanceId, suffix);
}

void Storage::makeChannelMapKey(uint8_t instanceId,
                                uint8_t variant,
                                char *output) const {
  char suffix[16] = {};
  snprintf(suffix, sizeof(suffix), "splt_%u_ch", variant);
  Supla::Config::generateKey(output, instanceId, suffix);
}

void Storage::makeConfigKey(uint8_t instanceId,
                            uint8_t variant,
                            char *output) const {
  char suffix[16] = {};
  snprintf(suffix, sizeof(suffix), "splt_%u_cfg", variant);
  Supla::Config::generateKey(output, instanceId, suffix);
}

void Storage::makeArtifactChunkKey(uint8_t instanceId,
                                   uint8_t variant,
                                   uint16_t chunkIndex,
                                   char *output) const {
  char suffix[16] = {};
  snprintf(suffix, sizeof(suffix), "splt_%u_a%u", variant, chunkIndex);
  Supla::Config::generateKey(output, instanceId, suffix);
}

bool Storage::readBlobExact(const char *key,
                            char *output,
                            size_t expectedSize) const {
  if (config == nullptr || key == nullptr || output == nullptr ||
      config->getBlobSize(key) != static_cast<int>(expectedSize)) {
    return false;
  }
  return config->getBlob(key, output, expectedSize);
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
