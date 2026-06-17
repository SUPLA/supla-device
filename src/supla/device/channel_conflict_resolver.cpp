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

#include <supla/device/channel_conflict_resolver.h>

namespace Supla {
namespace Device {

ChannelConflictResolverList::~ChannelConflictResolverList() {
  clear();
}

bool ChannelConflictResolverList::add(ChannelConflictResolver *resolver) {
  if (resolver == nullptr || contains(resolver)) {
    return false;
  }

  auto item = new ChannelConflictResolverListItem;
  if (item == nullptr) {
    return false;
  }

  item->resolver = resolver;
  item->next = nullptr;

  if (first == nullptr) {
    first = item;
    return true;
  }

  auto current = first;
  while (current->next != nullptr) {
    current = current->next;
  }
  current->next = item;
  return true;
}

bool ChannelConflictResolverList::remove(ChannelConflictResolver *resolver) {
  ChannelConflictResolverListItem *previous = nullptr;
  auto current = first;

  while (current != nullptr) {
    if (current->resolver == resolver) {
      if (previous == nullptr) {
        first = current->next;
      } else {
        previous->next = current->next;
      }
      delete current;
      return true;
    }
    previous = current;
    current = current->next;
  }

  return false;
}

void ChannelConflictResolverList::clear() {
  while (first != nullptr) {
    auto current = first;
    first = first->next;
    delete current;
  }
}

bool ChannelConflictResolverList::contains(
    ChannelConflictResolver *resolver) const {
  for (auto current = first; current != nullptr; current = current->next) {
    if (current->resolver == resolver) {
      return true;
    }
  }
  return false;
}

bool ChannelConflictResolverList::isEmpty() const {
  return first == nullptr;
}

bool ChannelConflictResolverList::onChannelConflictReport(
    uint8_t *channelReport,
    uint8_t channelReportSize,
    bool hasConfilictInvalidType,
    bool hasConfilictChannelMissingOnServer,
    bool hasConflictChannelMissingOnDevice) {
  bool result = false;

  auto current = first;
  while (current != nullptr) {
    auto resolver = current->resolver;
    current = current->next;
    if (resolver != nullptr) {
      result |= resolver->onChannelConflictReport(
          channelReport,
          channelReportSize,
          hasConfilictInvalidType,
          hasConfilictChannelMissingOnServer,
          hasConflictChannelMissingOnDevice);
    }
  }

  return result;
}

}  // namespace Device
}  // namespace Supla
