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

void Supla::ElementWithChannelActions::runAction(uint16_t event) {
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
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::ChannelFunctionTag);
    int32_t channelFunc = 0;
    if (cfg->getInt32(key, &channelFunc)) {
      if (channel->isFunctionValid(channelFunc)) {
        SUPLA_LOG_INFO("Channel[%d] function loaded successfully (%d)",
                       channel->getChannelNumber(),
                       channelFunc);
        channel->setDefault(channelFunc);
      } else {
        SUPLA_LOG_INFO("Channel[%d] function invalid (%d)",
                       channel->getChannelNumber(),
                       channelFunc);
      }
      return true;
    } else {
      SUPLA_LOG_INFO("Channel[%d] function missing. Using SW defaults",
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

bool Supla::ElementWithChannelActions::saveConfigChangeFlag() {
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
    _supla_int_t channelFunction) {
  auto channel = getChannel();
  if (!channel) {
    return false;
  }
  if (channel->getDefaultFunction() != channelFunction) {
    channel->setDefault(channelFunction);
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
      generateKey(key, Supla::ConfigTag::ChannelFunctionTag);
      cfg->setInt32(key, channelFunction);
      cfg->saveWithDelay(5000);
    }
    return true;
  }
  return false;
}

bool Supla::ElementWithChannelActions::isAnyUpdatePending() {
  auto channel = getChannel();
  if (!channel) {
    return false;
  }

  if (channelConfigState == Supla::ChannelConfigState::LocalChangePending ||
      channelConfigState == Supla::ChannelConfigState::SetChannelConfigSend ||
      channelConfigState ==
          Supla::ChannelConfigState::OcrConfigPending ||
      channelConfigState ==
          Supla::ChannelConfigState::SetChannelOcrConfigSend ||
      channelConfigState == Supla::ChannelConfigState::WaitForConfigFinished) {
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
  clearOcrConfig();
  configFinishedReceived = false;
  Supla::Element::onRegistered(suplaSrpc);
  switch (channelConfigState) {
    case Supla::ChannelConfigState::None:
    case Supla::ChannelConfigState::OcrConfigPending:
    case Supla::ChannelConfigState::SetChannelOcrConfigSend:
    case Supla::ChannelConfigState::WaitForConfigFinished: {
      channelConfigState = Supla::ChannelConfigState::WaitForConfigFinished;
      break;
    }
    default: {
      channelConfigState = Supla::ChannelConfigState::LocalChangePending;
      break;
    }
  }
}

void Supla::ElementWithChannelActions::handleChannelConfigFinished() {
  configFinishedReceived = true;
  if (channelConfigState == Supla::ChannelConfigState::WaitForConfigFinished) {
    channelConfigState = Supla::ChannelConfigState::None;
  }
}

bool Supla::ElementWithChannelActions::iterateConnected() {
  auto result = Element::iterateConnected();

  if (!result) {
    return result;
  }

  if (configFinishedReceived) {
    if (channelConfigState == Supla::ChannelConfigState::LocalChangePending) {
      for (auto proto = Supla::Protocol::ProtocolLayer::first();
           proto != nullptr;
           proto = proto->next()) {
        uint8_t channelConfig[SUPLA_CHANNEL_CONFIG_MAXSIZE] = {};
        int channelConfigSize = 0;
        fillChannelConfig(reinterpret_cast<void *>(channelConfig),
                          &channelConfigSize);
        if (channelConfigSize > 0) {
          int defaultFunction = 0;
          if (getChannel()) {
            defaultFunction = getChannel()->getDefaultFunction();
          }
          if (proto->setChannelConfig(getChannelNumber(),
                                      defaultFunction,
                                      reinterpret_cast<void *>(channelConfig),
                                      channelConfigSize,
                                      SUPLA_CONFIG_TYPE_DEFAULT)) {
            SUPLA_LOG_INFO("Channel[%d]: channel config send",
                           getChannelNumber());
            channelConfigState =
                Supla::ChannelConfigState::SetChannelConfigSend;

            return true;
          }
        } else {
          SUPLA_LOG_WARNING(
              "Channel[%d]: set channel config implementation missing",
              getChannelNumber());
          channelConfigState = Supla::ChannelConfigState::None;
        }
      }
    } else if (channelConfigState ==
               Supla::ChannelConfigState::OcrConfigPending) {
      for (auto proto = Supla::Protocol::ProtocolLayer::first();
           proto != nullptr;
           proto = proto->next()) {
        uint8_t channelConfig[SUPLA_CHANNEL_CONFIG_MAXSIZE] = {};
        int channelConfigSize = 0;
        fillChannelOcrConfig(reinterpret_cast<void *>(channelConfig),
                          &channelConfigSize);
        if (channelConfigSize > 0) {
          int defaultFunction = 0;
          if (getChannel()) {
            defaultFunction = getChannel()->getDefaultFunction();
          }
          if (proto->setChannelConfig(getChannelNumber(),
                                      defaultFunction,
                                      reinterpret_cast<void *>(channelConfig),
                                      channelConfigSize,
                                      SUPLA_CONFIG_TYPE_OCR)) {
            SUPLA_LOG_INFO("Channel[%d]: channel OCR config send",
                           getChannelNumber());
            channelConfigState =
                Supla::ChannelConfigState::SetChannelOcrConfigSend;

            return true;
          }
        } else {
          SUPLA_LOG_WARNING(
              "Channel[%d]: set channel OCR config implementation missing",
              getChannelNumber());
          channelConfigState = Supla::ChannelConfigState::None;
        }
      }
    }
    if (channelConfigState == Supla::ChannelConfigState::None &&
        isOcrConfigMissing()) {
      channelConfigState = Supla::ChannelConfigState::OcrConfigPending;
    }
  }

  return result;
}

uint8_t Supla::ElementWithChannelActions::handleChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  SUPLA_LOG_DEBUG(
      "Channel[%d]: handleChannelConfig, func %d, configtype %d, configsize %d",
      getChannelNumber(),
      result->Func,
      result->ConfigType,
      result->ConfigSize);

  if (result->Func == 0) {
    SUPLA_LOG_DEBUG("Channel[%d] disabled on server", getChannelNumber());
    channelConfigState = Supla::ChannelConfigState::None;
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  auto newFunction = result->Func;
  if (newFunction != getChannel()->getDefaultFunction() && newFunction != 0) {
    SUPLA_LOG_INFO("Channel[%d]: function changed to %d",
                   getChannelNumber(),
                   newFunction);
    setAndSaveFunction(newFunction);
    for (auto proto = Supla::Protocol::ProtocolLayer::first();
        proto != nullptr; proto = proto->next()) {
      proto->notifyConfigChange(getChannelNumber());
    }
  }

  if (channelConfigState == Supla::ChannelConfigState::LocalChangePending &&
      !local &&
      result->ConfigType != SUPLA_CONFIG_TYPE_OCR) {
    SUPLA_LOG_INFO(
        "Channel[%d]: Ignoring config (local config changed offline)",
        getChannelNumber());
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  if (result->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT &&
      hasOcrConfig() && result->ConfigType != SUPLA_CONFIG_TYPE_OCR) {
    SUPLA_LOG_DEBUG("Channel[%d] invalid configtype", getChannelNumber());
    return SUPLA_CONFIG_RESULT_FALSE;
  }

  if (result->ConfigSize == 0) {
    // server doesn't have channel configuration, so we'll send it
    // But don't send it if it failed in previous attempt
    if (channelConfigState !=
        Supla::ChannelConfigState::SetChannelConfigFailed) {
      SUPLA_LOG_DEBUG("Channel[%d] no config on server", getChannelNumber());
      channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    }
  }

  if (result->ConfigType == SUPLA_CONFIG_TYPE_OCR) {
    if (channelConfigState ==
        Supla::ChannelConfigState::SetChannelOcrConfigSend) {
      channelConfigState = Supla::ChannelConfigState::None;
    }
  }
  return applyChannelConfig(result, local);
}

uint8_t Supla::ElementWithChannelActions::applyChannelConfig(
    TSD_ChannelConfig *, bool) {
  SUPLA_LOG_WARNING("Channel[%d]: applyChannelConfig missing",
                    getChannelNumber());
  return SUPLA_CONFIG_RESULT_TRUE;
}

void Supla::ElementWithChannelActions::fillChannelConfig(void *, int *size) {
  if (size) {
    *size = 0;
  }
}

void Supla::ElementWithChannelActions::fillChannelOcrConfig(void *, int *size) {
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

  if (!success) {
    channelConfigState = Supla::ChannelConfigState::SetChannelConfigFailed;
  }

  switch (result->ConfigType) {
    case SUPLA_CONFIG_TYPE_DEFAULT: {
      SUPLA_LOG_INFO("Channel[%d] Set channel config %s (%d)",
                     getChannelNumber(),
                     success ? "succeeded" : "failed",
                     result->Result);
      clearChannelConfigChangedFlag();
      break;
    }
    case SUPLA_CONFIG_TYPE_OCR: {
      SUPLA_LOG_INFO("Channel[%d] Set channel OCR config %s (%d)",
                     getChannelNumber(),
                     success ? "succeeded" : "failed",
                     result->Result);
      break;
    }
    default:
      break;
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

bool Supla::ElementWithChannelActions::hasOcrConfig() {
  return false;
}

bool Supla::ElementWithChannelActions::isOcrConfigMissing() {
  return false;
}

void Supla::ElementWithChannelActions::clearOcrConfig() {
  return;
}
