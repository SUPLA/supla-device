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

#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <supla/channels/channel.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/runtime.h>

namespace Supla {
namespace Suplet {

namespace {

bool channelMapsEqual(const ChannelMap &a, const ChannelMap &b) {
  if (a.getCount() != b.getCount()) {
    return false;
  }
  for (uint8_t i = 0; i < a.getCount(); i++) {
    auto mapping = a.getMapping(i);
    if (mapping == nullptr ||
        b.getChannelNumber(mapping->channelKey) != mapping->channelNumber) {
      return false;
    }
  }
  return true;
}

bool isSubDeviceIdUsedByChannel(uint8_t subDeviceId) {
  for (auto channel = Supla::Channel::Begin(); channel != nullptr;
       channel = channel->next()) {
    if (channel->getSubDeviceId() == subDeviceId) {
      return true;
    }
  }
  return false;
}

uint8_t findFirstFreeInstanceId(const InstanceTable &table) {
  for (int i = 1; i <= 255; i++) {
    uint8_t instanceId = static_cast<uint8_t>(i);
    if (table.findByInstanceId(instanceId) == nullptr &&
        table.findBySubDeviceId(instanceId) == nullptr &&
        !isSubDeviceIdUsedByChannel(instanceId)) {
      return instanceId;
    }
  }
  return 0;
}

bool normalizeInstanceSlot(InstanceRecord *record,
                           const InstanceRecord *existing,
                           const InstanceTable &table) {
  if (record == nullptr) {
    return false;
  }

  if (existing != nullptr) {
    record->instanceId = existing->instanceId;
    record->subDeviceId = existing->instanceId;
    return true;
  }

  if (record->instanceId == 0) {
    record->instanceId = findFirstFreeInstanceId(table);
  }

  record->subDeviceId = record->instanceId;
  return record->instanceId != 0 &&
         table.findByInstanceId(record->instanceId) == nullptr &&
         table.findBySubDeviceId(record->instanceId) == nullptr &&
         !isSubDeviceIdUsedByChannel(record->instanceId);
}

}  // namespace

Manager::Manager(Supla::Config *config) : storage(config) {
}

bool Manager::load() {
  return storage.load(&table);
}

bool Manager::save() {
  return storage.save(table);
}

bool Manager::erase() {
  table.clear();
  return storage.erase();
}

InstanceTable *Manager::getInstanceTable() {
  return &table;
}

const InstanceTable *Manager::getInstanceTable() const {
  return &table;
}

bool Manager::addInstance(const InstanceRecord &record) {
  InstanceRecord normalized = record;
  if (!normalizeInstanceSlot(&normalized, nullptr, table)) {
    return false;
  }
  if (!table.add(normalized)) {
    return false;
  }
  if (!save()) {
    table.removeByInstanceId(normalized.instanceId);
    return false;
  }
  return true;
}

bool Manager::addInstanceWithAllocatedChannels(
    InstanceRecord record,
    const uint32_t *requiredChannelKeys,
    uint8_t requiredChannelKeyCount,
    const ChannelAllocator &occupied) {
  if (!normalizeInstanceSlot(&record, table.findByInstanceId(record.instanceId),
                             table)) {
    return false;
  }

  ChannelAllocator allocator = occupied;
  if (!markExistingSupletChannels(&allocator)) {
    return false;
  }
  if (!allocator.allocateMissing(
          &record.channelMap, requiredChannelKeys, requiredChannelKeyCount)) {
    return false;
  }

  return addInstance(record);
}

bool Manager::addInstanceFromDefinition(InstanceRecord record,
                                        const Definition &definition,
                                        const ChannelAllocator &occupied) {
  if (!Runtime::validateDefinition(definition)) {
    return false;
  }

  uint32_t keys[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  if (!getRequiredChannelKeys(
          definition, keys, sizeof(keys) / sizeof(keys[0]))) {
    return false;
  }

  record.definitionId = definition.definitionId;
  record.definitionVersion = definition.definitionVersion;
  return addInstanceWithAllocatedChannels(
      record, keys, definition.channelCount, occupied);
}

bool Manager::canUpsertInstanceFromDefinition(
    InstanceRecord record,
    const Definition &definition,
    const ChannelAllocator &occupied) const {
  if (!Runtime::validateDefinition(definition)) {
    return false;
  }

  uint32_t keys[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  if (!getRequiredChannelKeys(
          definition, keys, sizeof(keys) / sizeof(keys[0]))) {
    return false;
  }

  InstanceRecord oldRecord = {};
  bool hadOldRecord = false;
  auto existing = table.findByInstanceId(record.instanceId);
  if (existing != nullptr) {
    oldRecord = *existing;
    hadOldRecord = true;
  }
  if (!normalizeInstanceSlot(&record, existing, table)) {
    return false;
  }

  ChannelMap inputMap = record.channelMap;
  if (hadOldRecord) {
    for (uint8_t i = 0; i < definition.channelCount; i++) {
      uint32_t channelKey = keys[i];
      if (!inputMap.containsKey(channelKey)) {
        int channelNumber = oldRecord.channelMap.getChannelNumber(channelKey);
        if (channelNumber != kInvalidChannelNumber &&
            !inputMap.add(channelKey, channelNumber)) {
          return false;
        }
      }
    }
  }

  record.definitionId = definition.definitionId;
  record.definitionVersion = definition.definitionVersion;
  record.channelMap = inputMap;

  ChannelAllocator allocator = occupied;
  if (!markExistingSupletChannelsExcept(&allocator, record.instanceId)) {
    return false;
  }

  uint8_t missingChannelCount = 0;
  for (uint8_t i = 0; i < definition.channelCount; i++) {
    if (!record.channelMap.containsKey(keys[i])) {
      missingChannelCount++;
    }
  }
  if (allocator.getFreeChannelCount() < missingChannelCount) {
    return false;
  }

  return true;
}

bool Manager::upsertInstanceFromDefinition(InstanceRecord record,
                                           const Definition &definition,
                                           const ChannelAllocator &occupied) {
  (void)(occupied);
  if (!canUpsertInstanceFromDefinition(record, definition, occupied)) {
    return false;
  }

  uint32_t keys[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  if (!getRequiredChannelKeys(
          definition, keys, sizeof(keys) / sizeof(keys[0]))) {
    return false;
  }

  InstanceRecord oldRecord = {};
  bool hadOldRecord = false;
  auto existing = table.findByInstanceId(record.instanceId);
  if (existing != nullptr) {
    oldRecord = *existing;
    hadOldRecord = true;
  }
  if (!normalizeInstanceSlot(&record, existing, table)) {
    return false;
  }

  ChannelMap inputMap = record.channelMap;
  if (hadOldRecord) {
    for (uint8_t i = 0; i < definition.channelCount; i++) {
      uint32_t channelKey = keys[i];
      if (!inputMap.containsKey(channelKey)) {
        int channelNumber = oldRecord.channelMap.getChannelNumber(channelKey);
        if (channelNumber != kInvalidChannelNumber &&
            !inputMap.add(channelKey, channelNumber)) {
          return false;
        }
      }
    }
  }

  record.definitionId = definition.definitionId;
  record.definitionVersion = definition.definitionVersion;
  record.channelMap = inputMap;

  if (hadOldRecord && !table.removeByInstanceId(record.instanceId)) {
    return false;
  }
  if (!table.add(record)) {
    if (hadOldRecord) {
      table.add(oldRecord);
    }
    return false;
  }
  if (!save()) {
    table.removeByInstanceId(record.instanceId);
    if (hadOldRecord) {
      table.add(oldRecord);
    }
    return false;
  }

  return true;
}

bool Manager::createElementsFromRegistry(const Registry &registry,
                                         Supla::Element **created,
                                         uint16_t createdSize,
                                         uint16_t *createdCount) {
  if (created == nullptr) {
    return false;
  }

  uint16_t count = 0;
  bool tableChanged = false;
  for (uint8_t i = 0; i < table.getCount(); i++) {
    auto record = table.getRecord(i);
    if (record == nullptr || record->state != InstanceState::Active) {
      continue;
    }

    const Definition *definition = registry.findDefinition(
        record->definitionId, record->definitionVersion);
    if (definition == nullptr ||
        count + definition->channelCount > createdSize) {
      if (createdCount != nullptr) {
        *createdCount = 0;
      }
      return false;
    }

    ChannelMap createdChannelMap;
    if (!Runtime::createElements(*definition,
                                 *record,
                                 &created[count],
                                 definition->channelCount,
                                 &createdChannelMap)) {
      for (uint16_t j = 0; j < count; j++) {
        delete created[j];
        created[j] = nullptr;
      }
      if (createdCount != nullptr) {
        *createdCount = 0;
      }
      return false;
    }
    if (!channelMapsEqual(record->channelMap, createdChannelMap)) {
      record->channelMap = createdChannelMap;
      tableChanged = true;
    }
    count += definition->channelCount;
  }

  if (tableChanged && !save()) {
    for (uint16_t j = 0; j < count; j++) {
      delete created[j];
      created[j] = nullptr;
    }
    if (createdCount != nullptr) {
      *createdCount = 0;
    }
    return false;
  }

  if (createdCount != nullptr) {
    *createdCount = count;
  }
  return true;
}

bool Manager::removeInstance(uint8_t instanceId) {
  if (!table.removeByInstanceId(instanceId)) {
    return false;
  }
  return save();
}

uint8_t Manager::getFirstFreeSubDeviceId() const {
  return findFirstFreeInstanceId(table);
}

bool Manager::onChannelConflictReport(uint8_t *channelReport,
                                      uint8_t channelReportSize,
                                      bool hasConfilictInvalidType,
                                      bool hasConfilictChannelMissingOnServer,
                                      bool hasConflictChannelMissingOnDevice) {
  if (hasConfilictInvalidType || hasConflictChannelMissingOnDevice ||
      !hasConfilictChannelMissingOnServer) {
    return false;
  }

  bool removed = false;
  for (int i = table.getCount() - 1; i >= 0; i--) {
    auto record = table.getRecord(i);
    if (record == nullptr || record->channelMap.getCount() == 0) {
      continue;
    }

    uint8_t missingCount = 0;
    for (uint8_t j = 0; j < record->channelMap.getCount(); j++) {
      auto mapping = record->channelMap.getMapping(j);
      if (mapping != nullptr &&
          isChannelMissingOnServer(
              channelReport, channelReportSize, mapping->channelNumber)) {
        missingCount++;
      }
    }

    if (missingCount == record->channelMap.getCount()) {
      table.removeByInstanceId(record->instanceId);
      removed = true;
    }
  }

  if (removed) {
    save();
  }

  return false;
}

bool Manager::isChannelMissingOnServer(uint8_t *channelReport,
                                       uint8_t channelReportSize,
                                       int channelNumber) const {
  if (channelNumber < 0 || channelNumber >= SUPLA_CHANNELMAXCOUNT) {
    return false;
  }
  return channelNumber >= channelReportSize ||
         channelReport[channelNumber] == 0;
}

bool Manager::markExistingSupletChannels(ChannelAllocator *allocator) const {
  return markExistingSupletChannelsExcept(allocator, 0);
}

bool Manager::markExistingSupletChannelsExcept(ChannelAllocator *allocator,
                                               uint8_t instanceId) const {
  if (allocator == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < table.getCount(); i++) {
    auto record = table.getRecord(i);
    if (record != nullptr && record->instanceId != instanceId &&
        !allocator->markFromMap(record->channelMap)) {
      return false;
    }
  }
  return true;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
