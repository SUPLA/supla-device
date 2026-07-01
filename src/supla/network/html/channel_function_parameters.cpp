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

#ifndef ARDUINO_ARCH_AVR
#include <supla/network/html/channel_function_parameters.h>

#include <stdio.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/control/relay_roller_shutter_pair.h>
#include <supla/element.h>
#include <supla/log_wrapper.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

namespace {

const uint32_t selectableFunctions[] = {
    SUPLA_CHANNELFNC_POWERSWITCH,
    SUPLA_CHANNELFNC_LIGHTSWITCH,
    SUPLA_CHANNELFNC_STAIRCASETIMER,
    SUPLA_CHANNELFNC_CONTROLLINGTHEGATE,
    SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK,
    SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR,
    SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK,
    SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH,
    SUPLA_CHANNELFNC_PUMPSWITCH,
    SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER,
    SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW,
    SUPLA_CHANNELFNC_TERRACE_AWNING,
    SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR,
    SUPLA_CHANNELFNC_CURTAIN,
    SUPLA_CHANNELFNC_PROJECTOR_SCREEN,
    SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND,
    SUPLA_CHANNELFNC_VERTICAL_BLIND,
};

}  // namespace

using Supla::Html::ChannelFunctionParameters;

ChannelFunctionParameters::ChannelFunctionParameters(
    Supla::Element *element,
    const char *primaryLabel,
    const char *secondaryLabel)
    : HtmlElement(HTML_SECTION_FORM),
      element(element),
      primaryLabel(primaryLabel),
      secondaryLabel(secondaryLabel) {
}

ChannelFunctionParameters::ChannelFunctionParameters(
    Supla::Control::RelayRollerShutterPair *pair,
    const char *primaryLabel,
    const char *secondaryLabel,
    ChannelScope channelScope)
    : HtmlElement(HTML_SECTION_FORM),
      element(pair),
      pair(pair),
      primaryLabel(primaryLabel),
      secondaryLabel(secondaryLabel),
      channelScope(channelScope) {
}

ChannelFunctionParameters::ChannelFunctionParameters(
    Supla::Control::RelayRollerShutterPair *pair,
    ChannelScope channelScope)
    : ChannelFunctionParameters(pair, nullptr, nullptr, channelScope) {
}

ChannelFunctionParameters::~ChannelFunctionParameters() {
}

bool ChannelFunctionParameters::isKnownSelectableFunction(uint32_t function) {
  for (uint32_t supportedFunction : selectableFunctions) {
    if (supportedFunction == function) {
      return true;
    }
  }
  return false;
}

uint32_t selectableFunctionBit(uint32_t function) {
  switch (function) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK:
      return SUPLA_BIT_FUNC_CONTROLLINGTHEGATEWAYLOCK;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATE:
      return SUPLA_BIT_FUNC_CONTROLLINGTHEGATE;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR:
      return SUPLA_BIT_FUNC_CONTROLLINGTHEGARAGEDOOR;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK:
      return SUPLA_BIT_FUNC_CONTROLLINGTHEDOORLOCK;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
      return SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW:
      return SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW;
    case SUPLA_CHANNELFNC_POWERSWITCH:
      return SUPLA_BIT_FUNC_POWERSWITCH;
    case SUPLA_CHANNELFNC_LIGHTSWITCH:
      return SUPLA_BIT_FUNC_LIGHTSWITCH;
    case SUPLA_CHANNELFNC_STAIRCASETIMER:
      return SUPLA_BIT_FUNC_STAIRCASETIMER;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
      return SUPLA_BIT_FUNC_CONTROLLINGTHEFACADEBLIND;
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
      return SUPLA_BIT_FUNC_TERRACE_AWNING;
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
      return SUPLA_BIT_FUNC_PROJECTOR_SCREEN;
    case SUPLA_CHANNELFNC_CURTAIN:
      return SUPLA_BIT_FUNC_CURTAIN;
    case SUPLA_CHANNELFNC_VERTICAL_BLIND:
      return SUPLA_BIT_FUNC_VERTICAL_BLIND;
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR:
      return SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR;
    case SUPLA_CHANNELFNC_PUMPSWITCH:
      return SUPLA_BIT_FUNC_PUMPSWITCH;
    case SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH:
      return SUPLA_BIT_FUNC_HEATORCOLDSOURCESWITCH;
  }
  return 0;
}

bool isSelectableFunctionSupported(Supla::Channel *channel,
                                   uint32_t function) {
  if (channel == nullptr) {
    return false;
  }

  const uint32_t functionBit = selectableFunctionBit(function);
  return functionBit != 0 && (channel->getFuncList() & functionBit);
}

bool ChannelFunctionParameters::renderOptions(Supla::WebSender *sender,
                                              Supla::Channel *channel) {
  if (sender == nullptr || channel == nullptr) {
    return false;
  }

  bool anyOptionRendered = false;
  const uint32_t currentFunction = channel->getDefaultFunction();
  for (uint32_t function : selectableFunctions) {
    if (isSelectableFunctionSupported(channel, function)) {
      sender->selectOption(function,
                           Supla::getRelayChannelName(function),
                           currentFunction == function);
      anyOptionRendered = true;
    }
  }
  return anyOptionRendered;
}

void ChannelFunctionParameters::renderSelectField(Supla::WebSender *sender,
                                                  Supla::Channel *channel,
                                                  const char *key,
                                                  const char *label) {
  if (sender == nullptr || channel == nullptr || key == nullptr ||
      label == nullptr) {
    return;
  }

  sender->labeledField(key, label, [&]() {
    sender->selectTag(key, key).body([&]() {
      renderOptions(sender, channel);
    });
  });
}

void ChannelFunctionParameters::send(Supla::WebSender *sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (sender == nullptr || element == nullptr || cfg == nullptr) {
    return;
  }

  Supla::Channel *primaryChannel = element->getChannel();
  if (channelScope != ChannelScope::Secondary && primaryChannel != nullptr &&
      primaryChannel->getChannelNumber() >= 0) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    char label[40] = {};
    Supla::Config::generateKey(key,
                               primaryChannel->getChannelNumber(),
                               Supla::ConfigTag::ChannelFunctionTag);
    if (primaryLabel == nullptr) {
      snprintf(label,
               sizeof(label),
               "Channel #%d default function",
               primaryChannel->getChannelNumber());
    }
    renderSelectField(sender,
                      primaryChannel,
                      key,
                      primaryLabel == nullptr ? label : primaryLabel);
  }

  Supla::Channel *secondaryChannel =
      pair == nullptr ? nullptr : pair->getSecondaryChannel();
  if (channelScope != ChannelScope::Primary && secondaryChannel != nullptr &&
      secondaryChannel->getChannelNumber() >= 0) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    char label[40] = {};
    Supla::Config::generateKey(key,
                               secondaryChannel->getChannelNumber(),
                               Supla::ConfigTag::ChannelFunctionTag);
    if (secondaryLabel == nullptr) {
      snprintf(label,
               sizeof(label),
               "Channel #%d default function",
               secondaryChannel->getChannelNumber());
    }
    renderSelectField(sender,
                      secondaryChannel,
                      key,
                      secondaryLabel == nullptr ? label : secondaryLabel);
  }
}

bool ChannelFunctionParameters::handleSingleChannelResponse(
    const char *key,
    const char *value,
    Supla::Channel *channel) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg == nullptr || element == nullptr || key == nullptr ||
      value == nullptr || channel == nullptr ||
      channel->getChannelNumber() < 0) {
    return false;
  }

  char expectedKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(expectedKey,
                             channel->getChannelNumber(),
                             Supla::ConfigTag::ChannelFunctionTag);
  if (strcmp(key, expectedKey) != 0) {
    return false;
  }

  const uint32_t function = stringToUInt(value);
  if (!isKnownSelectableFunction(function) ||
      !isSelectableFunctionSupported(channel, function)) {
    SUPLA_LOG_WARNING("ChannelFunctionHtml: unsupported function %d",
                      function);
    return true;
  }

  cfg->setChannelFunction(channel->getChannelNumber(), function);
  if (channel == element->getChannel()) {
    element->setDefaultFunction(function);
  } else {
    channel->setDefaultFunction(function);
  }
  return true;
}

bool ChannelFunctionParameters::handlePairResponse(const char *key,
                                                   const char *value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (pair == nullptr || cfg == nullptr || key == nullptr || value == nullptr) {
    return false;
  }

  Supla::Channel *primaryChannel = pair->getChannel();
  Supla::Channel *secondaryChannel = pair->getSecondaryChannel();
  if (primaryChannel == nullptr || secondaryChannel == nullptr) {
    return false;
  }

  char primaryKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  char secondaryKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(primaryKey,
                             primaryChannel->getChannelNumber(),
                             Supla::ConfigTag::ChannelFunctionTag);
  Supla::Config::generateKey(secondaryKey,
                             secondaryChannel->getChannelNumber(),
                             Supla::ConfigTag::ChannelFunctionTag);

  const bool primaryMatched = strcmp(key, primaryKey) == 0;
  const bool secondaryMatched = strcmp(key, secondaryKey) == 0;
  if (!primaryMatched && !secondaryMatched) {
    return false;
  }
  if ((primaryMatched && channelScope == ChannelScope::Secondary) ||
      (secondaryMatched && channelScope == ChannelScope::Primary)) {
    return false;
  }

  const uint32_t requestedFunction = stringToUInt(value);
  uint32_t primaryFunction = primaryChannel->getDefaultFunction();
  uint32_t secondaryFunction = secondaryChannel->getDefaultFunction();
  Supla::Channel *changedChannel = nullptr;

  if (primaryMatched) {
    primaryFunction = requestedFunction;
    changedChannel = primaryChannel;
  } else {
    secondaryFunction = requestedFunction;
    changedChannel = secondaryChannel;
  }

  if (!isKnownSelectableFunction(requestedFunction) ||
      !isSelectableFunctionSupported(changedChannel, requestedFunction) ||
      !pair->setDefaultFunctions(primaryFunction, secondaryFunction)) {
    SUPLA_LOG_WARNING("ChannelFunctionHtml: unsupported pair function %d",
                      requestedFunction);
    return true;
  }

  cfg->setChannelFunction(primaryChannel->getChannelNumber(),
                          primaryFunction);
  cfg->setChannelFunction(secondaryChannel->getChannelNumber(),
                          secondaryFunction);
  return true;
}

bool ChannelFunctionParameters::handleResponse(const char *key,
                                               const char *value) {
  if (element == nullptr) {
    return false;
  }

  if (handlePairResponse(key, value)) {
    return true;
  }

  return handleSingleChannelResponse(key, value, element->getChannel());
}

#endif  // ARDUINO_ARCH_AVR
