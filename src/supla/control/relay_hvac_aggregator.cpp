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

#include <supla/control/hvac_base.h>
#include <supla/control/relay.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

using Supla::Control::RelayHvacAggregator;

namespace {
RelayHvacAggregator *FirstInstance = nullptr;

// 15 minutes
constexpr uint32_t IGNORE_OFFLINE_HVAC_TIMEOUT = 15 * 60 * 1000;
}  // namespace

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
    if (hvac->getChannel()->isStateOnline()) {
      ptr->nextPtr->lastSeenTimestamp = millis();
    }
  }
  SUPLA_LOG_DEBUG("RelayHvacAggregator[%d] hvac[%d @ %X] registered (%s)",
                  relayChannelNumber,
                  hvac->getChannelNumber(),
                  hvac,
                  hvac->getChannel()->isStateOnline() ? "online" : "offline");
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

  bool state = false;
  bool ignore = true;
  auto *ptr = firstHvacPtr;
  while (ptr != nullptr) {
    if (ptr->hvac != nullptr && ptr->hvac->getChannel()) {
      if (ptr->hvac->getChannel()->isStateOnline()) {
        ptr->lastSeenTimestamp = millis();
      }
      if (!ptr->hvac->ignoreAggregatorForRelay(relayChannelNumber)) {
        ignore = false;
        if (ptr->hvac->getChannel()->isHvacFlagHeating() ||
            ptr->hvac->getChannel()->isHvacFlagCooling()) {
          if (millis() - ptr->lastSeenTimestamp < IGNORE_OFFLINE_HVAC_TIMEOUT) {
            state = true;
            break;
          }
        }
      }
    }
    ptr = ptr->nextPtr;
  }

  if (millis() - lastStateUpdateTimestamp > relayInternalStateCheckIntervalMs ||
      lastRelayState == -1) {
    if (relay->isOn()) {
      if (lastRelayState != 1 && lastValueSend != -1 &&
          (!ignore || turnOffSendOnEmpty == 0)) {
        lastValueSend = 1;
      }
      lastRelayState = 1;
    } else {
      if (lastRelayState != 0 && lastValueSend != -1) {
        lastValueSend = 0;
      }
      lastRelayState = 0;
    }
    lastStateUpdateTimestamp = millis();
  }

  lastUpdateTimestamp = millis();

  if (ignore && (!turnOffWhenEmpty || state == lastValueSend)) {
    return;
  }

  if (!ignore && !turnOffWhenEmpty) {
    lastValueSend = lastRelayState;
  }

  if (state) {
    if (lastValueSend != 1) {
      lastValueSend = 1;
      SUPLA_LOG_INFO("RelayHvacAggregator[%d] turn on", relayChannelNumber);
      lastStateUpdateTimestamp = millis();
      relay->turnOn();
      lastRelayState = 1;
      turnOffSendOnEmpty = 0;
    }
  } else {
    if (lastValueSend != 0) {
      lastValueSend = 0;
      SUPLA_LOG_INFO("RelayHvacAggregator[%d] turn off", relayChannelNumber);
      lastStateUpdateTimestamp = millis();
      relay->turnOff();
      lastRelayState = 0;
      if (ignore) {
        turnOffSendOnEmpty++;
      }
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

void RelayHvacAggregator::setInternalStateCheckInterval(uint32_t intervalMs) {
  relayInternalStateCheckIntervalMs = intervalMs;
}
