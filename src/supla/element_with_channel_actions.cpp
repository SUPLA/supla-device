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
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "fnc");
    int32_t channelFunc = 0;
    if (cfg->getInt32(key, &channelFunc)) {
      SUPLA_LOG_INFO("Channel function loaded successfully (%d)",
                     channelFunc);
      auto channel = getChannel();
      if (channel) {
        channel->setDefault(channelFunc);
      }
      return true;
    } else {
      SUPLA_LOG_INFO("Channel function missing. Using SW defaults");
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
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "cfg_chng");
    uint8_t cfgChangeFlag = 0;
    if (cfg->getUInt8(key, &cfgChangeFlag)) {
      SUPLA_LOG_INFO("Channel[%d] config changed offline flag %d",
                     getChannelNumber(),
                     cfgChangeFlag);
      if (cfgChangeFlag) {
        channelConfigState = Supla::ChannelConfigState::LocalChangePending;
      } else {
        channelConfigState = Supla::ChannelConfigState::None;
      }
      return true;
    }
  }
  return false;
}

bool Supla::ElementWithChannelActions::saveConfigChangeFlag() {
  if (getChannelNumber() < 0) {
    return false;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "cfg_chng");
    if (channelConfigState == Supla::ChannelConfigState::None ||
        channelConfigState ==
            Supla::ChannelConfigState::WaitForConfigFinished) {
      cfg->setUInt8(key, 0);
    } else {
      cfg->setUInt8(key, 1);
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
      generateKey(key, "fnc");
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
      channelConfigState == Supla::ChannelConfigState::WaitForConfigFinished) {
    return true;
  }

  if (channel->isUpdateReady()) {
    return true;
  }

  return false;
}
