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

#include <supla/suplet/assignment_applier.h>
#include <supla/suplet/json_instance_config.h>

namespace Supla {
namespace Suplet {

namespace {

bool isInstanceLimitAvailable(const Manager *manager,
                              const Registry *registry,
                              const InstanceRecord &record,
                              uint32_t definitionId,
                              uint16_t definitionVersion) {
  if (manager == nullptr || registry == nullptr || record.instanceId == 0) {
    return false;
  }

  Capability capability = {};
  if (!registry->getCapability(definitionId, definitionVersion, &capability)) {
    return false;
  }

  const InstanceTable *table = manager->getInstanceTable();
  if (table == nullptr) {
    return false;
  }

  uint8_t count = 0;
  for (uint8_t i = 0; i < table->getCount(); i++) {
    const InstanceRecord *existing = table->getRecord(i);
    if (existing != nullptr && existing->instanceId != record.instanceId &&
        existing->definitionId == definitionId &&
        existing->definitionVersion == definitionVersion) {
      count++;
    }
  }

  return count < capability.maxInstances;
}

}  // namespace

AssignmentApplier::AssignmentApplier(Manager *manager, const Registry *registry)
    : manager(manager), registry(registry) {
}

AssignmentResult AssignmentApplier::applyJson(
    const char *json,
    uint32_t definitionId,
    uint16_t definitionVersion) {
  if (manager == nullptr || registry == nullptr || json == nullptr ||
      definitionId == 0 || definitionVersion == 0) {
    return AssignmentResult::InvalidArgument;
  }

  const Definition *definition =
      registry->findDefinition(definitionId, definitionVersion);
  if (definition == nullptr) {
    return AssignmentResult::DefinitionNotSupported;
  }

  InstanceRecord record = {};
  if (!JsonInstanceConfigParser::parse(json, *definition, &record)) {
    return AssignmentResult::InvalidConfig;
  }

  if (!isInstanceLimitAvailable(
          manager, registry, record, definitionId, definitionVersion)) {
    return AssignmentResult::InstanceLimitExceeded;
  }

  if (!manager->canUpsertInstanceFromDefinition(record, *definition)) {
    return AssignmentResult::ChannelLimitExceeded;
  }

  if (!manager->upsertInstanceFromDefinition(record, *definition)) {
    return AssignmentResult::StorageError;
  }

  return AssignmentResult::Applied;
}

AssignmentResult AssignmentApplier::validateJson(
    const char *json,
    uint32_t definitionId,
    uint16_t definitionVersion) const {
  if (manager == nullptr || registry == nullptr || json == nullptr ||
      definitionId == 0 || definitionVersion == 0) {
    return AssignmentResult::InvalidArgument;
  }

  const Definition *definition =
      registry->findDefinition(definitionId, definitionVersion);
  if (definition == nullptr) {
    return AssignmentResult::DefinitionNotSupported;
  }

  InstanceRecord record = {};
  if (!JsonInstanceConfigParser::parse(json, *definition, &record)) {
    return AssignmentResult::InvalidConfig;
  }

  if (!isInstanceLimitAvailable(
          manager, registry, record, definitionId, definitionVersion)) {
    return AssignmentResult::InstanceLimitExceeded;
  }

  if (!manager->canUpsertInstanceFromDefinition(record, *definition)) {
    return AssignmentResult::ChannelLimitExceeded;
  }

  return AssignmentResult::Applied;
}

AssignmentResult AssignmentApplier::remove(uint8_t instanceId) {
  if (manager == nullptr || instanceId == 0) {
    return AssignmentResult::InvalidArgument;
  }
  return manager->removeInstance(instanceId) ? AssignmentResult::Removed
                                             : AssignmentResult::StorageError;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
