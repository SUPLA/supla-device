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
#include <supla/device/register_device.h>
#include <supla/element.h>
#include <supla/log_wrapper.h>
#include <supla/storage/storage.h>
#include <supla/suplet/calcfg_session.h>
#include <supla/suplet/capability_registry.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/runtime.h>
#include <supla/suplet/server_config.h>
#include <supla/time.h>

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
        b.getChannelNumber(mapping->channelId) != mapping->channelNumber) {
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

class ScopedJsonDefinition {
 public:
  ScopedJsonDefinition() = default;
  ScopedJsonDefinition(const ScopedJsonDefinition &) = delete;
  ScopedJsonDefinition &operator=(const ScopedJsonDefinition &) = delete;

  ~ScopedJsonDefinition() {
    delete definition;
  }

  bool allocate() {
    if (definition == nullptr) {
      definition = new JsonDefinition();
    }
    return definition != nullptr;
  }

  JsonDefinition *get() {
    return definition;
  }

 private:
  JsonDefinition *definition = nullptr;
};

const Definition *findDefinitionForRecord(
    const Registry &registry,
    const ServerConfigHandler *serverConfigHandler,
    const InstanceRecord &record,
    ScopedJsonDefinition *downloadedDefinition,
    bool *ramError) {
  if (ramError != nullptr) {
    *ramError = false;
  }

  const Definition *definition =
      registry.findDefinition(record.definitionId, record.definitionVersion);
  if (definition != nullptr) {
    return definition;
  }

  if (serverConfigHandler == nullptr || downloadedDefinition == nullptr) {
    return nullptr;
  }
  if (!downloadedDefinition->allocate()) {
    if (ramError != nullptr) {
      *ramError = true;
    }
    return nullptr;
  }
  if (downloadedDefinition->get() == nullptr ||
      !serverConfigHandler->loadDownloadedDefinition(
          record.definitionId,
          record.definitionVersion,
          downloadedDefinition->get())) {
    return nullptr;
  }
  return downloadedDefinition->get()->getDefinition();
}

}  // namespace

Manager::Manager(Supla::Config *config) : storage(config) {
}

Manager::~Manager() {
  deleteRuntimeElements();
  clearInstanceCalcfgSession();
  clearDefinitionCalcfgSession();
}

bool Manager::load() {
  return storage.loadIndex(&table);
}

bool Manager::loadInstance(uint8_t instanceId, InstanceRecord *record) {
  return storage.loadInstance(instanceId, record);
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

void Manager::setRegistry(Registry *registry) {
  this->registry = registry;
}

Registry *Manager::getRegistry() {
  return registry;
}

const Registry *Manager::getRegistry() const {
  return registry;
}

void Manager::setCapabilityRegistry(CapabilityRegistry *registry) {
  capabilityRegistry = registry;
}

CapabilityRegistry *Manager::getCapabilityRegistry() {
  return capabilityRegistry;
}

const CapabilityRegistry *Manager::getCapabilityRegistry() const {
  return capabilityRegistry;
}

void Manager::setServerConfigHandler(ServerConfigHandler *handler) {
  serverConfigHandler = handler;
}

ServerConfigHandler *Manager::getServerConfigHandler() {
  return serverConfigHandler;
}

const ServerConfigHandler *Manager::getServerConfigHandler() const {
  return serverConfigHandler;
}

bool Manager::isServerConfigReady() const {
  return registry != nullptr && serverConfigHandler != nullptr;
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

bool Manager::addInstanceFromDefinition(InstanceRecord record,
                                        const Definition &definition) {
  return upsertInstanceFromDefinition(record, definition);
}

bool Manager::canUpsertInstanceFromDefinition(
    InstanceRecord record,
    const Definition &definition) const {
  if (!Runtime::validateDefinition(definition)) {
    return false;
  }

  uint8_t ids[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  if (!getDefinitionChannelIds(
          definition, ids, sizeof(ids) / sizeof(ids[0]))) {
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
      uint8_t channelId = ids[i];
      if (!inputMap.containsId(channelId)) {
        int channelNumber = oldRecord.channelMap.getChannelNumber(channelId);
        if (channelNumber != kInvalidChannelNumber &&
            !inputMap.add(channelId, channelNumber)) {
          return false;
        }
      }
    }
  }

  record.definitionId = definition.definitionId;
  record.definitionVersion = definition.definitionVersion;
  record.channelMap = inputMap;

  uint8_t missingChannelCount = 0;
  for (uint8_t i = 0; i < definition.channelCount; i++) {
    if (!record.channelMap.containsId(ids[i])) {
      missingChannelCount++;
    }
  }
  if (!Supla::RegisterDevice::hasFreeChannelCount(missingChannelCount)) {
    return false;
  }

  return true;
}

bool Manager::upsertInstanceFromDefinition(InstanceRecord record,
                                           const Definition &definition) {
  if (!canUpsertInstanceFromDefinition(record, definition)) {
    return false;
  }

  uint8_t ids[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  if (!getDefinitionChannelIds(
          definition, ids, sizeof(ids) / sizeof(ids[0]))) {
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
      uint8_t channelId = ids[i];
      if (!inputMap.containsId(channelId)) {
        int channelNumber = oldRecord.channelMap.getChannelNumber(channelId);
        if (channelNumber != kInvalidChannelNumber &&
            !inputMap.add(channelId, channelNumber)) {
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
    if (record == nullptr) {
      continue;
    }

    ScopedJsonDefinition downloadedDefinition;
    const Definition *definition = findDefinitionForRecord(
        registry, serverConfigHandler, *record, &downloadedDefinition, nullptr);
    if (definition == nullptr ||
        count + definition->channelCount > createdSize) {
      if (createdCount != nullptr) {
        *createdCount = 0;
      }
      return false;
    }

    InstanceRecord runtimeRecord = *record;
    if (runtimeRecord.config == nullptr && runtimeRecord.configSize > 0 &&
        !storage.loadInstance(record->instanceId, &runtimeRecord)) {
      for (uint16_t j = 0; j < count; j++) {
        delete created[j];
        created[j] = nullptr;
      }
      if (createdCount != nullptr) {
        *createdCount = 0;
      }
      return false;
    }

    ChannelMap createdChannelMap;
    if (!Runtime::createElements(*definition,
                                 runtimeRecord,
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

bool Manager::loadRuntimeElementsFromRegistry(const Registry &registry) {
  deleteRuntimeElements();

  if (!load()) {
    SUPLA_LOG_DEBUG("Suplet: no stored instance table");
    return true;
  }

  uint16_t requiredCount = 0;
  if (!getRequiredRuntimeElementCount(registry, &requiredCount)) {
    SUPLA_LOG_WARNING("Suplet: failed to count stored elements");
    return false;
  }
  if (requiredCount == 0) {
    return true;
  }

  runtimeElements = new Supla::Element *[requiredCount]();
  if (runtimeElements == nullptr) {
    SUPLA_LOG_WARNING("Suplet: failed to allocate element table");
    return false;
  }

  if (!createElementsFromRegistry(
          registry, runtimeElements, requiredCount, &runtimeElementCount)) {
    SUPLA_LOG_WARNING("Suplet: failed to create stored elements");
    deleteRuntimeElements();
    return false;
  }

  if (runtimeElementCount > 0) {
    SUPLA_LOG_INFO("Suplet: created %u element(s)",
                   runtimeElementCount);
  }
  return true;
}

bool Manager::loadRuntimeElements() {
  if (registry == nullptr) {
    deleteRuntimeElements();
    return true;
  }
  return loadRuntimeElementsFromRegistry(*registry);
}

void Manager::deleteRuntimeElements() {
  if (runtimeElements != nullptr) {
    for (uint16_t i = 0; i < runtimeElementCount; i++) {
      delete runtimeElements[i];
      runtimeElements[i] = nullptr;
    }
    delete[] runtimeElements;
    runtimeElements = nullptr;
  }
  runtimeElementCount = 0;
}

bool Manager::initRuntimeElements(SuplaDeviceClass *device) {
  if (device == nullptr) {
    return false;
  }
  for (uint16_t i = 0; i < runtimeElementCount; i++) {
    if (runtimeElements[i] != nullptr) {
      if (Supla::Storage::IsConfigStorageAvailable()) {
        runtimeElements[i]->onLoadConfig(device);
      }
      runtimeElements[i]->onInit();
    }
    delay(0);
  }
  return true;
}

uint16_t Manager::getRuntimeElementCount() const {
  return runtimeElementCount;
}

ServerConfigResult Manager::applyCommandJson(const char *commandJson) {
  if (serverConfigHandler == nullptr) {
    return ServerConfigResult::InvalidArgument;
  }
  return serverConfigHandler->applyCommandJson(commandJson);
}

ServerConfigResult Manager::validateCommandJson(const char *commandJson) const {
  if (serverConfigHandler == nullptr) {
    return ServerConfigResult::InvalidArgument;
  }
  return serverConfigHandler->validateCommandJson(commandJson);
}

InstanceCalcfgSession *Manager::getInstanceCalcfgSession() {
  return instanceCalcfgSession;
}

const InstanceCalcfgSession *Manager::getInstanceCalcfgSession() const {
  return instanceCalcfgSession;
}

InstanceCalcfgSession *Manager::beginInstanceCalcfgSession() {
  clearInstanceCalcfgSession();
  instanceCalcfgSession = new InstanceCalcfgSession();
  return instanceCalcfgSession;
}

void Manager::clearInstanceCalcfgSession() {
  if (instanceCalcfgSession != nullptr) {
    delete instanceCalcfgSession;
    instanceCalcfgSession = nullptr;
  }
}

DefinitionCalcfgSession *Manager::getDefinitionCalcfgSession() {
  return definitionCalcfgSession;
}

const DefinitionCalcfgSession *Manager::getDefinitionCalcfgSession() const {
  return definitionCalcfgSession;
}

DefinitionCalcfgSession *Manager::beginDefinitionCalcfgSession() {
  clearDefinitionCalcfgSession();
  definitionCalcfgSession = new DefinitionCalcfgSession();
  return definitionCalcfgSession;
}

void Manager::clearDefinitionCalcfgSession() {
  if (definitionCalcfgSession != nullptr) {
    if (serverConfigHandler != nullptr && definitionCalcfgSession->active) {
      serverConfigHandler->abortStagedDownloadedDefinition(
          definitionCalcfgSession->cacheSlot);
    }
    delete definitionCalcfgSession;
    definitionCalcfgSession = nullptr;
  }
}

void Manager::cleanupExpiredCalcfgSessions(uint32_t nowMs) {
  if (instanceCalcfgSession != nullptr && instanceCalcfgSession->active &&
      nowMs - instanceCalcfgSession->lastActivityMs >
          SUPLA_SUPLET_CALCFG_SESSION_TIMEOUT_MS) {
    SUPLA_LOG_WARNING("Suplet CALCFG instance session timeout: session=%u",
                      instanceCalcfgSession->sessionId);
    clearInstanceCalcfgSession();
  }

  if (definitionCalcfgSession != nullptr && definitionCalcfgSession->active &&
      nowMs - definitionCalcfgSession->lastActivityMs >
          SUPLA_SUPLET_CALCFG_SESSION_TIMEOUT_MS) {
    SUPLA_LOG_WARNING("Suplet CALCFG definition session timeout: session=%u",
                      definitionCalcfgSession->sessionId);
    clearDefinitionCalcfgSession();
  }
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

bool Manager::getRequiredRuntimeElementCount(const Registry &registry,
                                             uint16_t *count) const {
  if (count == nullptr) {
    return false;
  }
  *count = 0;
  for (uint8_t i = 0; i < table.getCount(); i++) {
    auto record = table.getRecord(i);
    if (record == nullptr) {
      continue;
    }

    ScopedJsonDefinition downloadedDefinition;
    const Definition *definition = findDefinitionForRecord(
        registry, serverConfigHandler, *record, &downloadedDefinition, nullptr);
    if (definition == nullptr) {
      return false;
    }
    if (static_cast<uint32_t>(*count) + definition->channelCount >
        SUPLA_CHANNELMAXCOUNT) {
      return false;
    }
    *count += definition->channelCount;
  }
  return true;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
