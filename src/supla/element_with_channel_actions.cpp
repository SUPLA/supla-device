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

#include "element_with_channel_actions.h"
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/channels/channel.h>
#include <supla/condition.h>
#include <supla/element.h>
#include <supla/storage/storage.h>

namespace Supla {
class ActionHandler;
namespace Protocol {
class SuplaSrpc;
}  // namespace Protocol
}  // namespace Supla

using Supla::ConfigTypesBitmap;
using Supla::ApplyConfigResult;

void ConfigTypesBitmap::clear(int configType) {
  set(configType, false);
}

void ConfigTypesBitmap::clearAll() {
  all = 0;
}

void ConfigTypesBitmap::setAll(uint8_t values) {
  SUPLA_LOG_DEBUG("ConfigTypesBitmap: Set all to 0x%X", values);
  all = values;
}

uint8_t ConfigTypesBitmap::getAll() const {
  return all;
}

void ConfigTypesBitmap::setConfigFinishedReceived() {
  configFinishedReceived = 1;
}

void ConfigTypesBitmap::clearConfigFinishedReceived() {
  configFinishedReceived = 0;
}

bool ConfigTypesBitmap::isConfigFinishedReceived() const {
  return configFinishedReceived == 1;
}

void ConfigTypesBitmap::set(int configType, bool value) {
//  SUPLA_LOG_DEBUG(
//      "ConfigTypesBitmap: Set config type %d to %d", configType, value);
  uint8_t v = value ? 1 : 0;
  switch (configType) {
    case SUPLA_CONFIG_TYPE_DEFAULT: {
      defaultConfig = v;
      break;
    }
    case SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE: {
      weeklySchedule = v;
      break;
    }
    case SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE: {
      altWeeklySchedule = v;
      break;
    }
    case SUPLA_CONFIG_TYPE_OCR: {
      ocrConfig = v;
      break;
    }
    case SUPLA_CONFIG_TYPE_EXTENDED: {
      extendedDefaultConfig = v;
      break;
    }
    default: {
      SUPLA_LOG_ERROR("ConfigTypesBitmap: Unknown config type: %d", configType);
      break;
    }
  }
}

bool ConfigTypesBitmap::isSet(int configType) const {
  switch (configType) {
    case SUPLA_CONFIG_TYPE_DEFAULT: {
      return defaultConfig == 1;
    }
    case SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE: {
      return weeklySchedule == 1;
    }
    case SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE: {
      return altWeeklySchedule == 1;
    }
    case SUPLA_CONFIG_TYPE_OCR: {
      return ocrConfig == 1;
    }
    case SUPLA_CONFIG_TYPE_EXTENDED: {
      return extendedDefaultConfig == 1;
    }
    case 1: {
      // 1 is used in some older devices, so we just ignore it here
      return false;
    }
    default: {
      SUPLA_LOG_ERROR("ConfigTypesBitmap: Unknown config type: %d", configType);
      return false;
    }
  }
}

bool ConfigTypesBitmap::operator!=(const ConfigTypesBitmap &other) const {
  // ignore first bit
  return (all & (~1)) != (other.all & (~1));
}

void Supla::ElementWithChannelActions::addAction(uint16_t action,
    Supla::ActionHandler &client,
    uint16_t event,
    bool alwaysEnabled) {
  auto channel = getChannel();
  if (channel) {
    channel->addAction(action, client, event, alwaysEnabled);
  }
}

void Supla::ElementWithChannelActions::addAction(uint16_t action,
    Supla::ActionHandler *client,
    uint16_t event,
    bool alwaysEnabled) {
  ElementWithChannelActions::addAction(action, *client, event, alwaysEnabled);
}

void Supla::ElementWithChannelActions::runAction(uint16_t event) const {
  auto channel = getChannel();
  if (channel) {
    channel->runAction(event);
  }
}

bool Supla::ElementWithChannelActions::isEventAlreadyUsed(
    uint16_t event, bool ignoreAlwaysEnabled) {
  auto channel = getChannel();
  if (channel) {
    return channel->isEventAlreadyUsed(event, ignoreAlwaysEnabled);
  }
  return false;
}

void Supla::ElementWithChannelActions::addAction(uint16_t action,
    Supla::ActionHandler &client,
    Supla::Condition *condition,
    bool alwaysEnabled) {
  condition->setClient(client);
  condition->setSource(this);
  auto channel = getChannel();
  if (channel) {
    channel->addAction(action, condition, Supla::ON_CHANGE, alwaysEnabled);
  }
}

void Supla::ElementWithChannelActions::addAction(uint16_t action,
    Supla::ActionHandler *client,
    Supla::Condition *condition,
    bool alwaysEnabled) {
  ElementWithChannelActions::addAction(
      action, *client, condition, alwaysEnabled);
}

bool Supla::ElementWithChannelActions::loadFunctionFromConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  auto channel = getChannel();
  if (cfg && channel) {
    int32_t channelFunc = cfg->getChannelFunction(getChannelNumber());
    if (channelFunc >= 0) {
      if (channel->isFunctionValid(channelFunc)) {
        SUPLA_LOG_INFO("Channel[%d] loaded function: %d",
                       channel->getChannelNumber(),
                       channelFunc);
        setFunction(channelFunc);
      } else {
        SUPLA_LOG_INFO("Channel[%d] invalid function: %d",
                       channel->getChannelNumber(),
                       channelFunc);
      }
      return true;
    } else {
      SUPLA_LOG_DEBUG("Channel[%d] function missing. Using SW defaults",
                     channel->getChannelNumber());
    }
  }
  return false;
}

bool Supla::ElementWithChannelActions::loadConfigChangeFlag() {
  if (getChannelNumber() < 0) {
    return false;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    if (cfg->isChannelConfigChangeFlagSet(getChannelNumber())) {
      SUPLA_LOG_INFO("Channel[%d] config changed offline flag is set",
                     getChannelNumber());
      channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    } else {
      channelConfigState = Supla::ChannelConfigState::None;
    }
    return true;
  }
  return false;
}

bool Supla::ElementWithChannelActions::saveConfigChangeFlag() const {
  if (getChannelNumber() < 0) {
    return false;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    if (channelConfigState == Supla::ChannelConfigState::None ||
        channelConfigState ==
            Supla::ChannelConfigState::WaitForConfigFinished) {
      cfg->clearChannelConfigChangeFlag(getChannelNumber(), 0);
    } else {
      cfg->setChannelConfigChangeFlag(getChannelNumber(), 0);
    }
    cfg->saveWithDelay(5000);
    return true;
  }
  return false;
}

bool Supla::ElementWithChannelActions::setAndSaveFunction(
    uint32_t channelFunction) {
  auto channel = getChannel();
  if (!channel) {
    return false;
  }
  if (setFunction(channelFunction)) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setChannelFunction(getChannelNumber(), channelFunction);
      cfg->saveWithDelay(5000);
    }
    return true;
  }
  return false;
}

bool Supla::ElementWithChannelActions::isAnyUpdatePending() const {
  auto channel = getChannel();
  if (!channel) {
    return false;
  }

  if (channelConfigState == Supla::ChannelConfigState::LocalChangePending ||
      channelConfigState == Supla::ChannelConfigState::SetChannelConfigSend ||
      channelConfigState == Supla::ChannelConfigState::LocalChangeSent ||
      channelConfigState == Supla::ChannelConfigState::WaitForConfigFinished ||
      channelConfigState == Supla::ChannelConfigState::ResendConfig) {
    return true;
  }

  if (channel->isUpdateReady()) {
    return true;
  }

  return false;
}

void Supla::ElementWithChannelActions::clearChannelConfigChangedFlag() {
  if (channelConfigState != Supla::ChannelConfigState::None &&
      channelConfigState != Supla::ChannelConfigState::SetChannelConfigFailed) {
    channelConfigState = Supla::ChannelConfigState::None;
    saveConfigChangeFlag();
  }
}

void Supla::ElementWithChannelActions::onRegistered(
    Supla::Protocol::SuplaSrpc *suplaSrpc) {
  receivedConfigTypes.clearAll();
  setChannelConfigAttempts = 0;
  Supla::Element::onRegistered(suplaSrpc);
  switch (channelConfigState) {
    case Supla::ChannelConfigState::None:
    case Supla::ChannelConfigState::WaitForConfigFinished: {
      channelConfigState = Supla::ChannelConfigState::WaitForConfigFinished;
      break;
    }
    case Supla::ChannelConfigState::LocalChangePending: {
      break;
    }
    default: {
      channelConfigState = Supla::ChannelConfigState::ResendConfig;
      break;
    }
  }
}

void Supla::ElementWithChannelActions::handleChannelConfigFinished() {
  receivedConfigTypes.setConfigFinishedReceived();
  setChannelConfigAttempts = 0;
  if (channelConfigState == Supla::ChannelConfigState::WaitForConfigFinished) {
    channelConfigState = Supla::ChannelConfigState::None;
  }
  if (receivedConfigTypes != usedConfigTypes) {
    SUPLA_LOG_INFO(
        "Channel[%d] some config is missing on server... (rcv: 0x%X != used: "
        "0x%X)",
        getChannelNumber(),
        receivedConfigTypes.getAll(),
        usedConfigTypes.getAll());
    channelConfigState = Supla::ChannelConfigState::ResendConfig;
  }
}

bool Supla::ElementWithChannelActions::iterateConnected() {
  if (!Element::iterateConnected()) {
    return false;
  }

  if (!iterateConfigExchange()) {
    return false;
  }

  return true;
}

uint8_t Supla::ElementWithChannelActions::handleChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  SUPLA_LOG_DEBUG(
      "Channel[%d] handleChannelConfig, func %d, configtype %d, configsize %d",
      getChannelNumber(),
      result->Func,
      result->ConfigType,
      result->ConfigSize);

  // Apply channel function setting
  auto newFunction = static_cast<uint32_t>(result->Func);
  if (newFunction != getChannel()->getDefaultFunction()) {
    SUPLA_LOG_INFO("Channel[%d] function changed to %d",
                   getChannelNumber(),
                   newFunction);
    setAndSaveFunction(newFunction);
    for (auto proto = Supla::Protocol::ProtocolLayer::first();
        proto != nullptr; proto = proto->next()) {
      proto->notifyConfigChange(getChannelNumber());
    }
  }

  // Channel disabled on server
  if (result->Func == 0) {
    SUPLA_LOG_DEBUG("Channel[%d] disabled on server", getChannelNumber());
    channelConfigState = Supla::ChannelConfigState::None;
    receivedConfigTypes = usedConfigTypes;
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  // Reject unexpected ConfigType
  if (usedConfigTypes.isSet(result->ConfigType) == false) {
    SUPLA_LOG_WARNING("Channel[%d] unexpected ConfigType %d",
                      getChannelNumber(),
                      result->ConfigType);
    return SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
  }

  // Skip config if local config changed (except for OCR which is always
  // accepted)
  if (result->ConfigType != SUPLA_CONFIG_TYPE_OCR) {
    if (channelConfigState == Supla::ChannelConfigState::LocalChangePending &&
        !local) {
      SUPLA_LOG_INFO(
          "Channel[%d] Ignoring config (local config changed offline)",
          getChannelNumber());
      return SUPLA_CONFIG_RESULT_TRUE;
    }
  }

  auto applyResult = applyChannelConfig(result, local);
  switch (applyResult) {
    case ApplyConfigResult::Success: {
      SUPLA_LOG_INFO("Channel[%d] ConfigType %d applied",
                     getChannelNumber(),
                     result->ConfigType);
      receivedConfigTypes.set(result->ConfigType);
      return SUPLA_CONFIG_RESULT_TRUE;
    }
    case ApplyConfigResult::NotSupported: {
      SUPLA_LOG_WARNING("Channel[%d] ConfigType %d not supported",
                        getChannelNumber(),
                        result->ConfigType);
      receivedConfigTypes.set(result->ConfigType);
      return SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
    }
    case ApplyConfigResult::SetChannelConfigNeeded: {
      SUPLA_LOG_INFO("Channel[%d] ConfigType %d SetChannelConfigNeeded",
                     getChannelNumber(),
                     result->ConfigType);
      triggerSetChannelConfig(result->ConfigType);
      return SUPLA_CONFIG_RESULT_TRUE;
    }
    case ApplyConfigResult::DataError: {
      SUPLA_LOG_WARNING("Channel[%d] ConfigType %d data error",
                        getChannelNumber(),
                        result->ConfigType);
      return SUPLA_CONFIG_RESULT_DATA_ERROR;
    }
  }
  return SUPLA_CONFIG_RESULT_FALSE;
}

ApplyConfigResult Supla::ElementWithChannelActions::applyChannelConfig(
    TSD_ChannelConfig *, bool) {
  SUPLA_LOG_WARNING("Channel[%d] applyChannelConfig missing",
                    getChannelNumber());
  return ApplyConfigResult::NotSupported;
}

void Supla::ElementWithChannelActions::fillChannelConfig(void *,
                                                        int *size,
                                                        uint8_t) {
  if (size) {
    *size = 0;
  }
}

void Supla::ElementWithChannelActions::handleSetChannelConfigResult(
    TSDS_SetChannelConfigResult *result) {
  if (result == nullptr) {
    return;
  }

  bool success = (result->Result == SUPLA_CONFIG_RESULT_TRUE);

  SUPLA_LOG_INFO("Channel[%d] Set channel config %s (%d) for config type %d",
                 getChannelNumber(),
                 success ? "succeeded" : "failed",
                 result->Result,
                 result->ConfigType);

  receivedConfigTypes.set(result->ConfigType);

  if (channelConfigState == Supla::ChannelConfigState::SetChannelConfigSend ||
      channelConfigState == Supla::ChannelConfigState::LocalChangeSent) {
    if (receivedConfigTypes != usedConfigTypes) {
      setChannelConfigAttempts = 0;
      if (channelConfigState ==
          Supla::ChannelConfigState::SetChannelConfigSend) {
        channelConfigState = Supla::ChannelConfigState::ResendConfig;
      } else {
        channelConfigState = Supla::ChannelConfigState::LocalChangePending;
      }
    } else {
      clearChannelConfigChangedFlag();
    }
  }

  if (!success) {
    clearChannelConfigChangedFlag();
    channelConfigState = Supla::ChannelConfigState::SetChannelConfigFailed;
  }
}

void Supla::ElementWithChannelActions::purgeConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  auto channel = getChannel();
  if (cfg && channel) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::ChannelFunctionTag);
    cfg->eraseKey(key);
    generateKey(key, Supla::ConfigTag::ChannelConfigChangedFlagTag);
    cfg->eraseKey(key);
  }
}

void Supla::ElementWithChannelActions::triggerSetChannelConfig(int configType) {
  // don't trigger setChannelConfig if it failed in previous attempt
  if (channelConfigState != Supla::ChannelConfigState::SetChannelConfigFailed) {
    channelConfigState = Supla::ChannelConfigState::ResendConfig;
    receivedConfigTypes.clear(configType);
  }
}

bool Supla::ElementWithChannelActions::iterateConfigExchange() {
  if (!receivedConfigTypes.isConfigFinishedReceived()) {
    return true;
  }
  if (getChannel()->getDefaultFunction() == 0) {
    return true;
  }

  if (channelConfigState == Supla::ChannelConfigState::LocalChangePending ||
      channelConfigState == Supla::ChannelConfigState::ResendConfig) {
    int nextConfigType = getNextConfigType();
    if (nextConfigType == -1) {
      clearChannelConfigChangedFlag();
      return true;
    }
    if (nextConfigType < 0 || nextConfigType > 255) {
      SUPLA_LOG_ERROR("Channel[%d] unexepected config type %d",
                      getChannelNumber(),
                      nextConfigType);
      return true;
    }

    if (setChannelConfigAttempts < 3) {
      uint8_t channelConfig[SUPLA_CHANNEL_CONFIG_MAXSIZE] = {};
      int channelConfigSize = 0;
      setChannelConfigAttempts++;
      fillChannelConfig(reinterpret_cast<void *>(channelConfig),
          &channelConfigSize,
          nextConfigType);
      if (channelConfigSize > 0) {
        int defaultFunction = 0;
        if (getChannel()) {
          defaultFunction = getChannel()->getDefaultFunction();
        }
        bool sendResult = false;
        for (auto proto = Supla::Protocol::ProtocolLayer::first();
             proto != nullptr;
             proto = proto->next()) {
          if (proto->setChannelConfig(getChannelNumber(),
                                      defaultFunction,
                                      reinterpret_cast<void *>(channelConfig),
                                      channelConfigSize,
                                      nextConfigType)) {
            SUPLA_LOG_INFO(
                "Channel[%d] SetChannelConfig send, func %d, type %d",
                getChannelNumber(),
                defaultFunction,
                nextConfigType);
            if (channelConfigState ==
                Supla::ChannelConfigState::LocalChangePending) {
              channelConfigState = Supla::ChannelConfigState::LocalChangeSent;
            } else {
              channelConfigState =
                  Supla::ChannelConfigState::SetChannelConfigSend;
            }
            sendResult = true;
          }
        }
        if (sendResult) {
          return false;
        }
      } else {
        SUPLA_LOG_WARNING(
            "Channel[%d] fillChannelConfig returned empty config for "
            "ConfigType %d",
            getChannelNumber(),
            nextConfigType);
        receivedConfigTypes.set(nextConfigType);
      }
    } else {
      SUPLA_LOG_WARNING(
          "Channel[%d] SetChannelConfig failed 2 times for ConfigType %d, "
          "giving up",
          getChannelNumber(),
          nextConfigType);
      receivedConfigTypes.set(nextConfigType);
      setChannelConfigAttempts = 0;
    }
  }

  return true;
}

int Supla::ElementWithChannelActions::getNextConfigType() const {
  if (receivedConfigTypes != usedConfigTypes) {
    ConfigTypesBitmap notRecieved;
    notRecieved.setAll((~receivedConfigTypes.getAll()) &
                       usedConfigTypes.getAll());
    for (uint8_t i = 0; i < 8; i++) {
      if (notRecieved.isSet(i)) {
        return i;
      }
    }
  }

  return -1;
}
