// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "relay_hvac_aggregator.h"

#include <supla/control/hvac_base.h>
#include <supla/control/relay.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/channels/channel_state.h>
#include <supla/protocol/protocol_layer.h>
#ifndef ARDUINO_ARCH_AVR
#include <supla/suplan/resource_binding_manager.h>
#endif

using Supla::Control::RelayHvacAggregator;

namespace {
RelayHvacAggregator *FirstInstance = nullptr;

// 15 minutes
constexpr uint32_t IGNORE_OFFLINE_HVAC_TIMEOUT = 15UL * 60 * 1000;
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
  auto newHvac = new HvacPtr;
  newHvac->hvac = hvac;
  THVACValue value;
  if (ChannelState(hvac->getChannel()).hvac(&value)) {
    newHvac->lastSeenTimestamp = millis();
    newHvac->activeDemand = value.Flags & (SUPLA_HVAC_VALUE_FLAG_HEATING |
                                         SUPLA_HVAC_VALUE_FLAG_COOLING);
  }

  if (firstHvacPtr == nullptr) {
    firstHvacPtr = newHvac;
  } else {
    auto *ptr = firstHvacPtr;
    while (ptr->nextPtr != nullptr) {
      ptr = ptr->nextPtr;
    }
    ptr->nextPtr = newHvac;
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
      const ChannelState channelState(ptr->hvac->getChannel());
      THVACValue value;
      if (channelState.hvac(&value)) {
        ptr->lastSeenTimestamp = millis();
        ptr->activeDemand = value.Flags & (SUPLA_HVAC_VALUE_FLAG_HEATING |
                                          SUPLA_HVAC_VALUE_FLAG_COOLING);
      }
      if (!ptr->hvac->ignoreAggregatorForRelay(relayChannelNumber)) {
        ignore = false;
        if (ptr->activeDemand) {
          if (millis() - ptr->lastSeenTimestamp < IGNORE_OFFLINE_HVAC_TIMEOUT) {
            state = true;
          }
        }
      }
    }
    ptr = ptr->nextPtr;
  }

#ifndef ARDUINO_ARCH_AVR
  for (auto layer = Protocol::ProtocolLayer::first(); layer;
       layer = layer->next()) {
    if (auto bindings = layer->resourceBindings()) {
      bool configured = false;
      state = bindings->relayDemand(relayChannelNumber, &configured) || state;
      if (configured) ignore = false;
    }
  }
#endif

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
