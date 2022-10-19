/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <SuplaDevice.h>

#include "protocol_layer.h"

namespace Supla {
namespace Protocol {

ProtocolLayer *ProtocolLayer::firstPtr = nullptr;

bool ProtocolLayer::IsAnyUpdatePending() {
  auto *proto = first();
  while (proto != nullptr) {
    if (proto->isEnabled() && proto->isUpdatePending()) {
      return true;
    }
    proto = proto->next();
  }
  return false;
}

bool ProtocolLayer::isUpdatePending() {
  return false;
}

ProtocolLayer::ProtocolLayer(SuplaDeviceClass *sdc) : sdc(sdc) {
  if (firstPtr == nullptr) {
    firstPtr = this;
  } else {
    last()->nextPtr = this;
  }
}

ProtocolLayer::~ProtocolLayer() {
  if (first() == this) {
    firstPtr = next();
    return;
  }

  auto ptr = first();
  while (ptr->next() != this) {
    ptr = ptr->next();
  }

  ptr->nextPtr = ptr->next()->next();
}

ProtocolLayer *ProtocolLayer::first() {
  return firstPtr;
}

ProtocolLayer *ProtocolLayer::last() {
  ProtocolLayer *ptr = firstPtr;
  while (ptr && ptr->nextPtr) {
    ptr = ptr->nextPtr;
  }
  return ptr;
}

ProtocolLayer *ProtocolLayer::next() {
  return nextPtr;
}

SuplaDeviceClass *ProtocolLayer::getSdc() {
  return sdc;
}

bool ProtocolLayer::isConnectionError() {
  return false;
}

bool ProtocolLayer::isConnecting() {
  return false;
}

void ProtocolLayer::getUserLocaltime() {
}

void ProtocolLayer::getChannelConfig(uint8_t channelNumber) {
  (void)(channelNumber);
}

bool ProtocolLayer::isConfigEmpty() {
  return configEmpty;
}

}  // namespace Protocol
}  // namespace Supla
