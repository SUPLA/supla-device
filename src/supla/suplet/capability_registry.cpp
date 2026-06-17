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

#include <supla/suplet/capability_registry.h>

namespace Supla {
namespace Suplet {

bool CapabilityRegistry::add(const Capability &capability) {
  if (capability.category == Category::Unknown ||
      capability.kind == Kind::Unknown || capability.minSchemaVersion == 0 ||
      capability.maxSchemaVersion == 0 || capability.handlerVersion == 0 ||
      capability.maxInstances == 0 ||
      capability.minSchemaVersion > capability.maxSchemaVersion ||
      contains(
          capability.category, capability.kind, capability.handlerVersion)) {
    return false;
  }

  auto entry = new Entry;
  if (entry == nullptr) {
    return false;
  }

  entry->capability = capability;
  entry->capability.definitionId = 0;
  entry->capability.minDefinitionVersion = 0;
  entry->capability.maxDefinitionVersion = 0;
  entry->capability.supportsDownloadedDefinition =
      capability.supportsDownloadedDefinition ? 1 : 0;

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

void CapabilityRegistry::clear() {
  while (first != nullptr) {
    auto current = first;
    first = first->next;
    delete current;
  }
}

uint8_t CapabilityRegistry::getCount() const {
  uint8_t count = 0;
  for (auto current = first; current != nullptr; current = current->next) {
    count++;
  }
  return count;
}

bool CapabilityRegistry::getCapability(uint8_t index,
                                       Capability *capability) const {
  if (capability == nullptr) {
    return false;
  }

  uint8_t currentIndex = 0;
  for (auto current = first; current != nullptr; current = current->next) {
    if (currentIndex == index) {
      *capability = current->capability;
      return true;
    }
    currentIndex++;
  }

  return false;
}

bool CapabilityRegistry::contains(Category category,
                                  Kind kind,
                                  uint8_t handlerVersion) const {
  for (auto current = first; current != nullptr; current = current->next) {
    if (current->capability.category == category &&
        current->capability.kind == kind &&
        current->capability.handlerVersion == handlerVersion) {
      return true;
    }
  }
  return false;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
