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

#include <stdio.h>
#include <string.h>
#include <supla/storage/config.h>
#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <supla/suplet/storage.h>

namespace Supla {
namespace Suplet {

namespace {

constexpr uint8_t kSupletStorageVersion = 1;
constexpr uint8_t kDeletedSlot = 0;
constexpr uint8_t kVariantA = 1;
constexpr uint8_t kVariantB = 2;

uint8_t otherVariant(uint8_t variant) {
  return variant == kVariantA ? kVariantB : kVariantA;
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
  subDeviceId = other.subDeviceId;
  state = other.state;
  configSize = 0;
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
            static_cast<uint8_t>(i), &activeVariant, &record, false)) {
      if (!table->add(record)) {
        table->clear();
        return false;
      }
      loadedAny = true;
    }
  }
  return loadedAny;
}

bool Storage::save(const InstanceTable &table) {
  if (config == nullptr) {
    return false;
  }

  bool present[SUPLA_SUPLET_MAX_INSTANCE_ID + 1] = {};
  for (uint8_t i = 0; i < table.getCount(); i++) {
    auto record = table.getRecord(i);
    if (record == nullptr || record->instanceId == 0 ||
        record->subDeviceId != record->instanceId ||
        record->configSize > SUPLA_SUPLET_MAX_CONFIG_SIZE) {
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

    if (!saveVariant(*record, targetVariant)) {
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
  return loadActiveVariant(instanceId, &activeVariant, record, true);
}

bool Storage::loadActiveVariant(uint8_t instanceId,
                                uint8_t *activeVariant,
                                InstanceRecord *record,
                                bool loadConfig) {
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
    if (slotExists(instanceId)) {
      cleanupLoadedInstance(instanceId, kDeletedSlot, kDeletedSlot);
    }
    return false;
  }

  if (loadVariant(instanceId, currentActiveVariant, record, loadConfig)) {
    cleanupLoadedInstance(
        instanceId, currentActiveVariant, currentActiveVariant);
    if (activeVariant != nullptr) {
      *activeVariant = currentActiveVariant;
    }
    return true;
  }

  uint8_t fallbackVariant = otherVariant(currentActiveVariant);
  if (loadVariant(instanceId, fallbackVariant, record, loadConfig)) {
    cleanupLoadedInstance(instanceId, currentActiveVariant, fallbackVariant);
    if (activeVariant != nullptr) {
      *activeVariant = fallbackVariant;
    }
    return true;
  }

  cleanupLoadedInstance(instanceId, currentActiveVariant, kDeletedSlot);
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
  if (config == nullptr || record == nullptr ||
      (variant != kVariantA && variant != kVariantB)) {
    return false;
  }

  char headerKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeHeaderKey(instanceId, variant, headerKey);
  StoredInstanceHeader header = {};
  if (!readBlobExact(
          headerKey, reinterpret_cast<char *>(&header), sizeof(header))) {
    return false;
  }

  if (header.version != kSupletStorageVersion || header.definitionId == 0 ||
      header.definitionVersion == 0 ||
      header.channelCount > SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE ||
      header.configSize > SUPLA_SUPLET_MAX_CONFIG_SIZE) {
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
  loaded.state = static_cast<InstanceState>(header.state);
  loaded.configSize = header.configSize;

  if (channelMapSize > 0) {
    StoredChannelMapping stored[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
    if (!config->getBlob(
            channelMapKey, reinterpret_cast<char *>(stored), channelMapSize)) {
      return false;
    }
    for (uint8_t i = 0; i < header.channelCount; i++) {
      if (stored[i].channelKey == kInvalidChannelKey ||
          !loaded.channelMap.add(stored[i].channelKey,
                                 stored[i].channelNumber)) {
        return false;
      }
    }
  }

  if (loadConfig && header.configSize > 0) {
    uint8_t buffer[SUPLA_SUPLET_MAX_CONFIG_SIZE] = {};
    if (!config->getBlob(
            configKey, reinterpret_cast<char *>(buffer), header.configSize) ||
        !loaded.setConfig(buffer, header.configSize)) {
      return false;
    }
  } else if (!loadConfig) {
    loaded.configSize = header.configSize;
  }

  *record = loaded;
  return true;
}

bool Storage::saveVariant(const InstanceRecord &record, uint8_t variant) {
  if (config == nullptr || (variant != kVariantA && variant != kVariantB) ||
      record.instanceId == 0 || record.subDeviceId != record.instanceId ||
      record.configSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
      record.channelMap.getCount() > SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE) {
    return false;
  }

  StoredChannelMapping stored[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  for (uint8_t i = 0; i < record.channelMap.getCount(); i++) {
    auto mapping = record.channelMap.getMapping(i);
    if (mapping == nullptr || mapping->channelKey == kInvalidChannelKey ||
        mapping->channelNumber < 0 || mapping->channelNumber > 255) {
      return false;
    }
    stored[i].channelKey = mapping->channelKey;
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
  const uint8_t *configData = record.config;
  InstanceRecord loadedConfig = {};
  if (configData == nullptr && record.configSize > 0) {
    uint8_t activeVariant = kDeletedSlot;
    if (!loadActiveVariant(
            record.instanceId, &activeVariant, &loadedConfig, true) ||
        loadedConfig.config == nullptr ||
        loadedConfig.configSize != record.configSize) {
      return false;
    }
    configData = loadedConfig.config;
  }
  if (configData == nullptr) {
    configData = &emptyConfig;
  }
  if (!config->setBlob(configKey,
                       reinterpret_cast<const char *>(configData),
                       record.configSize)) {
    return false;
  }

  StoredInstanceHeader header = {};
  header.version = kSupletStorageVersion;
  header.state = static_cast<uint8_t>(record.state);
  header.definitionId = record.definitionId;
  header.definitionVersion = record.definitionVersion;
  header.channelCount = record.channelMap.getCount();
  header.configSize = record.configSize;

  char headerKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  makeHeaderKey(record.instanceId, variant, headerKey);
  return config->setBlob(
      headerKey, reinterpret_cast<const char *>(&header), sizeof(header));
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
