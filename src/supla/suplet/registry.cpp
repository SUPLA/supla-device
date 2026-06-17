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

#include <supla/suplet/registry.h>
#include <supla/suplet/runtime.h>

namespace Supla {
namespace Suplet {

bool Registry::add(const Definition *definition,
                   uint8_t maxInstances,
                   bool supportsDownloadedDefinition) {
  if (definition == nullptr ||
      maxInstances == 0 ||
      !Runtime::validateDefinition(*definition) ||
      contains(definition->definitionId, definition->definitionVersion)) {
    return false;
  }

  auto entry = new Entry;
  if (entry == nullptr) {
    return false;
  }
  entry->definition = definition;
  entry->maxInstances = maxInstances;
  entry->supportsDownloadedDefinition = supportsDownloadedDefinition ? 1 : 0;

  if (first == nullptr) {
    first = entry;
    return true;
  }

  auto current = first;
  while (current->next != nullptr) {
    current = current->next;
  }
  current->next = entry;
  return true;
}

bool Registry::remove(uint32_t definitionId, uint16_t definitionVersion) {
  bool removed = false;
  Entry *previous = nullptr;
  auto current = first;
  while (current != nullptr) {
    auto next = current->next;
    if (current->definition != nullptr &&
        current->definition->definitionId == definitionId &&
        (definitionVersion == 0 ||
         current->definition->definitionVersion == definitionVersion)) {
      if (previous == nullptr) {
        first = next;
      } else {
        previous->next = next;
      }
      delete current;
      removed = true;
      current = next;
      continue;
    }
    previous = current;
    current = next;
  }
  return removed;
}

void Registry::clear() {
  while (first != nullptr) {
    auto current = first;
    first = first->next;
    delete current;
  }
}

uint8_t Registry::getCount() const {
  uint8_t count = 0;
  for (auto current = first; current != nullptr; current = current->next) {
    count++;
  }
  return count;
}

const Definition *Registry::findDefinition(uint32_t definitionId,
                                           uint16_t definitionVersion) const {
  for (auto current = first; current != nullptr; current = current->next) {
    if (current->definition != nullptr &&
        current->definition->definitionId == definitionId &&
        (definitionVersion == 0 ||
         current->definition->definitionVersion == definitionVersion)) {
      return current->definition;
    }
  }
  return nullptr;
}

bool Registry::getCapability(uint8_t index, Capability *capability) const {
  if (capability == nullptr) {
    return false;
  }

  uint8_t currentIndex = 0;
  for (auto current = first; current != nullptr; current = current->next) {
    if (currentIndex == index) {
      if (current->definition == nullptr) {
        return false;
      }
      capability->category = current->definition->category;
      capability->kind = current->definition->kind;
      capability->definitionId = current->definition->definitionId;
      capability->minDefinitionVersion =
          current->definition->definitionVersion;
      capability->maxDefinitionVersion =
          current->definition->definitionVersion;
      capability->minSchemaVersion = current->definition->schemaVersion;
      capability->maxSchemaVersion = current->definition->schemaVersion;
      capability->handlerVersion = current->definition->handlerVersion;
      capability->maxInstances = current->maxInstances;
      capability->supportsDownloadedDefinition =
          current->supportsDownloadedDefinition;
      return true;
    }
    currentIndex++;
  }

  return false;
}

bool Registry::getCapability(uint32_t definitionId,
                             uint16_t definitionVersion,
                             Capability *capability) const {
  if (capability == nullptr || definitionId == 0) {
    return false;
  }

  for (auto current = first; current != nullptr; current = current->next) {
    if (current->definition != nullptr &&
        current->definition->definitionId == definitionId &&
        (definitionVersion == 0 ||
         current->definition->definitionVersion == definitionVersion)) {
      capability->category = current->definition->category;
      capability->kind = current->definition->kind;
      capability->definitionId = current->definition->definitionId;
      capability->minDefinitionVersion =
          current->definition->definitionVersion;
      capability->maxDefinitionVersion =
          current->definition->definitionVersion;
      capability->minSchemaVersion = current->definition->schemaVersion;
      capability->maxSchemaVersion = current->definition->schemaVersion;
      capability->handlerVersion = current->definition->handlerVersion;
      capability->maxInstances = current->maxInstances;
      capability->supportsDownloadedDefinition =
          current->supportsDownloadedDefinition;
      return true;
    }
  }

  return false;
}

bool Registry::contains(uint32_t definitionId,
                        uint16_t definitionVersion) const {
  return findDefinition(definitionId, definitionVersion) != nullptr;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
