/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include <supla/log_wrapper.h>
#include <supla/storage/config.h>

#include "supla/network/network.h"
#include "time.h"
#include "element.h"

namespace Supla {
Element *Element::firstPtr = nullptr;

Element::Element() : nextPtr(nullptr) {
  if (firstPtr == nullptr) {
    firstPtr = this;
  } else {
    last()->nextPtr = this;
  }
}

Element::~Element() {
  if (begin() == this) {
    firstPtr = next();
    return;
  }

  auto ptr = begin();
  while (ptr->next() != this) {
    ptr = ptr->next();
  }

  ptr->nextPtr = ptr->next()->next();
}

Element *Element::begin() {
  return firstPtr;
}

Element *Element::last() {
  Element *ptr = firstPtr;
  while (ptr && ptr->nextPtr) {
    ptr = ptr->nextPtr;
  }
  return ptr;
}

Element *Element::getElementByChannelNumber(int channelNumber) {
  Element *element = begin();
  while (element != nullptr && element->getChannelNumber() != channelNumber) {
    element = element->next();
  }

  return element;
}

bool Element::IsAnyUpdatePending() {
  Element *element = begin();
  while (element != nullptr) {
    auto ch = element->getChannel();
    if (ch && ch->isUpdateReady()) {
      return true;
    }
    element = element->next();
  }
  return false;
}

Element *Element::next() {
  return nextPtr;
}

void Element::onInit() {}

void Element::onLoadConfig() {}

void Element::onLoadState() {}

void Element::onSaveState() {}

void Element::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  if (suplaSrpc == nullptr) {
    return;
  }
  auto ch = getChannel();

  while (ch != nullptr) {
    if (ch->isSleepingEnabled()) {
      suplaSrpc->sendChannelStateResult(0, ch->getChannelNumber());
    }
    ch = getSecondaryChannel();
  }
}

void Element::iterateAlways() {}

bool Element::iterateConnected(void *srpc) {
  bool response = true;
  uint64_t timestamp = millis();
  Channel *secondaryChannel = getSecondaryChannel();
  if (secondaryChannel && secondaryChannel->isUpdateReady() &&
      timestamp - secondaryChannel->lastCommunicationTimeMs > 100) {
    secondaryChannel->lastCommunicationTimeMs = timestamp;
    secondaryChannel->sendUpdate(srpc);
    response = false;
  }

  Channel *channel = getChannel();
  if (channel && channel->isUpdateReady() &&
      timestamp - channel->lastCommunicationTimeMs > 100) {
    channel->lastCommunicationTimeMs = timestamp;
    channel->sendUpdate(srpc);
    response = false;
  }
  return response;
}

void Element::onTimer() {}

void Element::onFastTimer() {}

int Element::handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) {
  (void)(newValue);
  return -1;
}

void Element::fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) {
  (void)(value);
}

int Element::getChannelNumber() {
  int result = -1;
  Channel *channel = getChannel();
  if (channel) {
    result = channel->getChannelNumber();
  }
  return result;
}

Channel *Element::getChannel() {
  return nullptr;
}

Channel *Element::getSecondaryChannel() {
  return nullptr;
}

void Element::handleGetChannelState(TDSC_ChannelState *channelState) {
  (void)(channelState);
  return;
}

int Element::handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) {
  (void)(request);
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

Element & Element::disableChannelState() {
  if (getChannel()) {
    getChannel()->unsetFlag(SUPLA_CHANNEL_FLAG_CHANNELSTATE);
  }
  return *this;
}

void Element::handleChannelConfig(TSD_ChannelConfig *result) {
  (void)(result);
  SUPLA_LOG_DEBUG(
      "Element: received channel config reply, but handling is missing");
}

void Element::generateKey(char *output, const char *key) {
  Supla::Config::generateKey(output, getChannelNumber(), key);
}

};  // namespace Supla
