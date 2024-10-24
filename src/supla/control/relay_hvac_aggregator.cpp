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

#include "relay_hvac_aggregator.h"
#include <supla/control/relay.h>
#include <supla/control/hvac_base.h>
#include <supla/time.h>
#include <supla/log_wrapper.h>

using Supla::Control::RelayHvacAggregator;

namespace {
RelayHvacAggregator *FirstInstance = nullptr;
}


RelayHvacAggregator::RelayHvacAggregator(int relayChannelNumber,
                                         Supla::Control::Relay *relay)
    : relay(relay), relayChannelNumber(relayChannelNumber) {
  if (FirstInstance == nullptr) {
    FirstInstance = this;
  } else {
    auto *ptr = FirstInstance;
    while (ptr->nextPtr != nullptr) {
      ptr = ptr->nextPtr;
    }
    ptr->nextPtr = this;
  }
}

RelayHvacAggregator::~RelayHvacAggregator() {
  if (FirstInstance == this) {
    FirstInstance = nextPtr;
  } else {
    auto *ptr = FirstInstance;
    while (ptr->nextPtr != this && ptr->nextPtr != nullptr) {
      ptr = ptr->nextPtr;
    }
    if (ptr->nextPtr == this) {
      ptr->nextPtr = nextPtr;
    }
  }
  while (firstHvacPtr) {
    unregisterHvac(firstHvacPtr->hvac);
  }
}

RelayHvacAggregator *RelayHvacAggregator::Add(int relayChannelNumber,
                                              Relay *relay) {
  auto ptr = GetInstance(relayChannelNumber);
  if (ptr == nullptr) {
    SUPLA_LOG_INFO("RelayHvacAggregator[%d] created", relayChannelNumber);
    ptr = new RelayHvacAggregator(relayChannelNumber, relay);
  }
  return ptr;
}

bool RelayHvacAggregator::Remove(int relayChannelNumber) {
  auto *ptr = GetInstance(relayChannelNumber);
  if (ptr != nullptr) {
    delete ptr;
    SUPLA_LOG_INFO("RelayHvacAggregator[%d] removed", relayChannelNumber);
    return true;
  }
  return false;
}

RelayHvacAggregator *RelayHvacAggregator::GetInstance(int relayChannelNumber) {
  auto *ptr = FirstInstance;
  while (ptr != nullptr) {
    if (ptr->relayChannelNumber == relayChannelNumber) {
      return ptr;
    }
    ptr = ptr->nextPtr;
  }
  return nullptr;
}

void RelayHvacAggregator::UnregisterHvac(HvacBase *hvac) {
  auto *ptr = FirstInstance;
  while (ptr != nullptr) {
    ptr->unregisterHvac(hvac);
    ptr = ptr->nextPtr;
  }
}

void RelayHvacAggregator::registerHvac(HvacBase *hvac) {
  if (isHvacRegistered(hvac)) {
    SUPLA_LOG_DEBUG("RelayHvacAggregator[%d] hvac[%d @ %X] already registered",
                    relayChannelNumber,
                    hvac->getChannelNumber(),
                    hvac);
    return;
  }
  if (firstHvacPtr == nullptr) {
    firstHvacPtr = new HvacPtr;
    firstHvacPtr->hvac = hvac;
  } else {
    auto *ptr = firstHvacPtr;
    while (ptr->nextPtr != nullptr) {
      ptr = ptr->nextPtr;
    }
    ptr->nextPtr = new HvacPtr;
    ptr->nextPtr->hvac = hvac;
  }
  SUPLA_LOG_DEBUG("RelayHvacAggregator[%d] hvac[%d @ %X] registered",
                  relayChannelNumber,
                  hvac->getChannelNumber(),
                  hvac);
}

void RelayHvacAggregator::unregisterHvac(HvacBase *hvac) {
  SUPLA_LOG_DEBUG("RelayHvacAggregator[%d] hvac[%X] unregistered",
                  relayChannelNumber,
                  hvac);
  HvacPtr *ptr = firstHvacPtr;
  HvacPtr *prevPtr = nullptr;
  while (ptr != nullptr) {
    if (ptr->hvac == hvac) {
      if (prevPtr == nullptr) {
        firstHvacPtr = ptr->nextPtr;
      } else {
        prevPtr->nextPtr = ptr->nextPtr;
      }
      delete ptr;
      break;
    }
    prevPtr = ptr;
    ptr = ptr->nextPtr;
  }
}

void RelayHvacAggregator::iterateAlways() {
  if (relay == nullptr) {
    return;
  }
  if (millis() - lastUpdateTimestamp < 1000) {
    return;
  }

  if (millis() - lastStateUpdateTimestamp > 5000) {
    if (relay->isOn()) {
      lastValueSend = 1;
    } else {
      lastValueSend = 0;
    }
    lastStateUpdateTimestamp = millis();
  }

  lastUpdateTimestamp = millis();

  bool state = false;
  bool ignore = true;
  auto *ptr = firstHvacPtr;
  while (ptr != nullptr) {
    if (ptr->hvac != nullptr && ptr->hvac->getChannel()) {
      if (!ptr->hvac->ignoreAggregatorForRelay(relayChannelNumber)) {
        ignore = false;
        if (ptr->hvac->getChannel()->isHvacFlagHeating() ||
            ptr->hvac->getChannel()->isHvacFlagCooling()) {
          state = true;
          break;
        }
      }
    }
    ptr = ptr->nextPtr;
  }

  if (ignore && !turnOffWhenEmpty) {
    return;
  }

  if (state) {
    if (lastValueSend != 1) {
      lastValueSend = 1;
      lastStateUpdateTimestamp = millis();
      SUPLA_LOG_INFO("RelayHvacAggregator[%d] turn on", relayChannelNumber);
      relay->turnOn();
    }
  } else {
    if (lastValueSend != 0) {
      lastValueSend = 0;
      lastStateUpdateTimestamp = millis();
      SUPLA_LOG_INFO("RelayHvacAggregator[%d] turn off", relayChannelNumber);
      relay->turnOff();
    }
  }
}

void RelayHvacAggregator::setTurnOffWhenEmpty(bool turnOffWhenEmpty) {
  this->turnOffWhenEmpty = turnOffWhenEmpty;
}

bool RelayHvacAggregator::isHvacRegistered(HvacBase *hvac) const {
  auto *ptr = firstHvacPtr;
  while (ptr != nullptr) {
    if (ptr->hvac == hvac) {
      return true;
    }
    ptr = ptr->nextPtr;
  }
  return false;
}

int RelayHvacAggregator::getHvacCount() const {
  int count = 0;
  auto *ptr = firstHvacPtr;
  while (ptr != nullptr) {
    count++;
    ptr = ptr->nextPtr;
  }
  return count;
}

