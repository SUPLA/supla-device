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

#include "element.h"

#include <supla-common/proto.h>
#include <supla/channels/channel.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/time.h>

namespace Supla {

namespace Protocol {
class SuplaSrpc;
}  // namespace Protocol

Element *Element::firstPtr = nullptr;
bool Element::invalidatePtr = false;

Element::Element() {
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

Element *Element::getOwnerOfSubDeviceId(int subDeviceId) {
  if (subDeviceId <= 0) {
    return nullptr;
  }

  Element *element = begin();
  while (element != nullptr && !element->isOwnerOfSubDeviceId(subDeviceId)) {
    element = element->next();
  }

  return element;
}

bool Element::IsAnyUpdatePending() {
  Element *element = begin();
  while (element != nullptr) {
    if (element->isAnyUpdatePending()) {
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

void Element::purgeConfig() {}

void Element::onLoadState() {}

void Element::onSaveState() {}

void Element::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  if (suplaSrpc == nullptr) {
    return;
  }

  if (getChannel()) {
    getChannel()->onRegistered();
  }

  if (getSecondaryChannel()) {
    getSecondaryChannel()->onRegistered();
  }
}

bool Element::isChannelStateEnabled() const {
  if (getChannel() == nullptr) {
    return false;
  }
  return getChannel()->isChannelStateEnabled();
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
  if (secondaryChannel && secondaryChannel->isUpdateReady()) {
    secondaryChannel->lastCommunicationTimeMs = timestamp;
    secondaryChannel->sendUpdate();
    response = false;
  }

  Channel *channel = getChannel();
  if (channel && channel->isUpdateReady()) {
    channel->lastCommunicationTimeMs = timestamp;
    channel->sendUpdate();
    response = false;
  }
  return response;
}

void Element::onTimer() {}

void Element::onFastTimer() {}

int32_t Element::handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) {
  (void)(newValue);
  return -1;
}

void Element::fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) {
  (void)(value);
}

int Element::getChannelNumber() const {
  int result = -1;
  auto channel = getChannel();
  if (channel) {
    result = channel->getChannelNumber();
  }
  return result;
}

int Element::getSecondaryChannelNumber() const {
  int result = -1;
  auto channel = getSecondaryChannel();
  if (channel) {
    result = channel->getChannelNumber();
  }
  return result;
}

const Channel *Element::getChannel() const {
  return nullptr;
}

const Channel *Element::getSecondaryChannel() const {
  return nullptr;
}

Channel *Element::getChannel() {
  return nullptr;
}

Channel *Element::getSecondaryChannel() {
  return nullptr;
}

void Element::handleGetChannelState(TDSC_ChannelState *channelState) {
  if (channelState == nullptr) {
    return;
  }
  Channel *channel = getChannel();
  while (channel) {
    if (channelState->ChannelNumber == channel->getChannelNumber()) {
      if (channel->isBatteryPoweredFieldEnabled()) {
        channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_BATTERYPOWERED;
        channelState->BatteryPowered = channel->isBatteryPowered() ? 1 : 0;
      }

      channelState->BatteryLevel = channel->getBatteryLevel();
      if (channelState->BatteryLevel <= 100) {
        channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_BATTERYLEVEL;
      } else {
        channelState->BatteryLevel = 0;
      }

      if (channel->isBridgeSignalStrengthAvailable()) {
        channelState->Fields |=
            SUPLA_CHANNELSTATE_FIELD_BRIDGENODESIGNALSTRENGTH;
        channelState->BridgeNodeSignalStrength =
            channel->getBridgeSignalStrength();
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


void Element::generateKey(char *output, const char *key) const {
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

bool Element::isAnyUpdatePending() {
  return false;
}

void Element::setInitialCaption(const char *caption, bool secondaryChannel) {
  Supla::Channel *ch = nullptr;
  if (!secondaryChannel) {
    ch = getChannel();
  } else {
    ch = getSecondaryChannel();
  }

  if (ch) {
    ch->setInitialCaption(caption);
  }
}

void Element::setDefaultFunction(int32_t defaultFunction) {
  Supla::Channel *ch = getChannel();
  if (ch) {
    ch->setDefaultFunction(defaultFunction);
  }
}

bool Element::isOwnerOfSubDeviceId(int) const {
  return false;
}

};  // namespace Supla
