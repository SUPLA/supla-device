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
bool Element::invalidatePtr = false;

Element::Element() : nextPtr(nullptr) {
  if (firstPtr == nullptr) {
    firstPtr = this;
  } else {
    last()->nextPtr = this;
  }
}

Element::~Element() {
  invalidatePtr = true;
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
  if (channelNumber < 0) {
    return nullptr;
  }

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

void Element::onLoadConfig(SuplaDeviceClass *) {}

void Element::onLoadState() {}

void Element::onSaveState() {}

void Element::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  if (suplaSrpc == nullptr) {
    return;
  }
  auto ch = getChannel();

  if (ch != nullptr && ch->isSleepingEnabled()) {
    suplaSrpc->sendChannelStateResult(0, ch->getChannelNumber());
    ch->setUpdateReady();
  }
  ch = getSecondaryChannel();
  if (ch != nullptr && ch->isSleepingEnabled()) {
    suplaSrpc->sendChannelStateResult(0, ch->getChannelNumber());
    ch->setUpdateReady();
  }
}

void Element::iterateAlways() {}

bool Element::iterateConnected(void *ptr) {
  (void)(ptr);
  return iterateConnected();
}

bool Element::iterateConnected() {
  bool response = true;
  uint32_t timestamp = millis();
  Channel *secondaryChannel = getSecondaryChannel();
  if (secondaryChannel && secondaryChannel->isUpdateReady() &&
      timestamp - secondaryChannel->lastCommunicationTimeMs > 50) {
    secondaryChannel->lastCommunicationTimeMs = timestamp;
    secondaryChannel->sendUpdate();
    response = false;
  }

  Channel *channel = getChannel();
  if (channel && channel->isUpdateReady() &&
      timestamp - channel->lastCommunicationTimeMs > 50) {
    channel->lastCommunicationTimeMs = timestamp;
    channel->sendUpdate();
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
  Channel *channel = getChannel();
  while (channel) {
    if (channelState->ChannelNumber == channel->getChannelNumber()) {
      if (channel->isBatteryPowered()) {
        channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_BATTERYLEVEL
          | SUPLA_CHANNELSTATE_FIELD_BATTERYPOWERED;

        channelState->BatteryPowered = 1;
        channelState->BatteryLevel = channel->getBatteryLevel();
      }
      return;
    }
    if (channel != getSecondaryChannel()) {
      channel = getSecondaryChannel();
    } else {
      return;
    }
  }
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

uint8_t Element::handleChannelConfig(TSD_ChannelConfig *result, bool local) {
  (void)(result);
  (void)(local);
  SUPLA_LOG_ERROR(
      "Element: received channel config reply, but handling is missing");
  return SUPLA_RESULTCODE_UNSUPORTED;
}

uint8_t Element::handleWeeklySchedule(TSD_ChannelConfig *newWeeklySchedule,
                                      bool altSchedule,
                                      bool local) {
  (void)(newWeeklySchedule);
  (void)(altSchedule);
  (void)(local);
  SUPLA_LOG_ERROR(
      "Element: received weekly schedly, but handling is missing");
  return SUPLA_RESULTCODE_UNSUPORTED;
}

void Element::handleSetChannelConfigResult(
    TSDS_SetChannelConfigResult *result) {
  (void)(result);
  SUPLA_LOG_ERROR(
      "Element: received set channel config reply, but handling is missing");
}

void Element::handleChannelConfigFinished() {
  SUPLA_LOG_ERROR(
      "Element: received channel config finished, but handling is missing");
}


void Element::generateKey(char *output, const char *key) {
  Supla::Config::generateKey(output, getChannelNumber(), key);
}

void Element::onSoftReset() {
}

void Element::onDeviceConfigChange(uint64_t fieldBit) {
  (void)(fieldBit);
}

void Element::NotifyElementsAboutConfigChange(
    uint64_t fieldBit) {
  for (auto element = Supla::Element::begin(); element != nullptr;
       element = element->next()) {
    element->onDeviceConfigChange(fieldBit);
    delay(0);
  }
}

bool Element::IsInvalidPtrSet() {
  return invalidatePtr;
}

void Element::ClearInvalidPtr() {
  invalidatePtr = false;
}

};  // namespace Supla
