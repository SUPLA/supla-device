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

#include <supla/channels/channel.h>
#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <string.h>
#include <supla/suplet/runtime.h>
#include <supla/suplet/thermometer_group.h>
#include <supla/suplet/virtual_channel.h>

namespace Supla {
namespace Suplet {

bool Runtime::validateDefinition(const Definition &definition) {
  if (definition.definitionId == 0 || definition.definitionVersion == 0 ||
      definition.category == Category::Unknown ||
      definition.kind == Kind::Unknown || definition.schemaVersion == 0 ||
      definition.handlerVersion == 0 || definition.maxInstances == 0 ||
      definition.channelCount == 0 ||
      definition.channelCount > SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE ||
      definition.channels == nullptr) {
    return false;
  }

  uint8_t ids[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  if (!getDefinitionChannelIds(
          definition, ids, sizeof(ids) / sizeof(ids[0]))) {
    return false;
  }

  if (definition.kind == Kind::ThermometerGroup &&
      (definition.category != Category::Aggregate ||
       definition.channelCount != 1 ||
       definition.channels[0].kind != ChannelKind::VirtualThermometer)) {
    return false;
  }

  if (definition.parameterCount > 0 && definition.parameters == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < definition.parameterCount; i++) {
    const auto &parameter = definition.parameters[i];
    if (parameter.key == nullptr || parameter.key[0] == '\0' ||
        parameter.type == ParameterType::Unknown ||
        parameter.min > parameter.max) {
      return false;
    }
    if ((parameter.type == ParameterType::Enum ||
         parameter.type == ParameterType::ChannelList) &&
        parameter.defaultText != nullptr && parameter.defaultText[0] != '\0') {
      // enum defaults are supported as strings; channelList defaults are not.
      if (parameter.type == ParameterType::ChannelList) {
        return false;
      }
    }
    for (uint8_t j = i + 1; j < definition.parameterCount; j++) {
      if (definition.parameters[j].key != nullptr &&
          strcmp(parameter.key, definition.parameters[j].key) == 0) {
        return false;
      }
    }
  }

  return true;
}

Supla::Element *Runtime::createElement(const ChannelDefinition &channel,
                                       const InstanceRecord &instance) {
  Definition definition = {};
  definition.definitionId = 1;
  definition.definitionVersion = 1;
  definition.schemaVersion = 1;
  definition.handlerVersion = 1;
  definition.kind = Kind::Unknown;
  definition.category = Category::Unknown;
  definition.channels = &channel;
  definition.channelCount = 1;
  return createElement(definition, channel, instance);
}

Supla::Element *Runtime::createElement(const Definition &definition,
                                       const ChannelDefinition &channel,
                                       const InstanceRecord &instance) {
  int channelNumber = instance.channelMap.getChannelNumber(channel.channelId);
  if (instance.subDeviceId == 0) {
    return nullptr;
  }
  if (channelNumber >= 0 &&
      Supla::Channel::GetByChannelNumber(channelNumber) != nullptr) {
    return nullptr;
  }

  Supla::Element *element = nullptr;
  switch (channel.kind) {
    case ChannelKind::VirtualRelay:
      element =
          new Supla::Suplet::VirtualRelay(instance.subDeviceId, channelNumber);
      break;
    case ChannelKind::VirtualBinarySensor:
      element = new Supla::Suplet::VirtualBinarySensor(instance.subDeviceId,
                                                       channelNumber);
      break;
    case ChannelKind::VirtualThermometer:
      if (definition.kind == Kind::ThermometerGroup) {
        ThermometerGroupConfig config = {};
        if (!parseThermometerGroupConfig(
                instance.config, instance.configSize, &config)) {
          return nullptr;
        }
        element = new Supla::Suplet::ThermometerGroup(
            instance.subDeviceId, channelNumber, config);
      } else {
        element = new Supla::Suplet::VirtualThermometer(instance.subDeviceId,
                                                        channelNumber);
      }
      break;
    default:
      return nullptr;
  }

  if (element != nullptr && channel.defaultFunction != 0 &&
      element->getChannel() != nullptr) {
    element->getChannel()->setDefaultFunction(channel.defaultFunction);
  }
  if (element != nullptr && channel.caption != nullptr &&
      channel.caption[0] != '\0') {
    element->setInitialCaption(channel.caption);
  }

  return element;
}

bool Runtime::createElements(const Definition &definition,
                             const InstanceRecord &instance,
                             Supla::Element **created,
                             uint8_t createdSize,
                             ChannelMap *createdChannelMap) {
  if (!validateDefinition(definition) ||
      definition.channelCount > createdSize ||
      (definition.channelCount > 0 && created == nullptr)) {
    return false;
  }

  for (uint8_t i = 0; i < definition.channelCount; i++) {
    created[i] = nullptr;
  }

  for (uint8_t i = 0; i < definition.channelCount; i++) {
    created[i] = createElement(definition, definition.channels[i], instance);
    if (created[i] == nullptr || created[i]->getChannel() == nullptr ||
        created[i]->getChannel()->getChannelNumber() < 0) {
      for (uint8_t j = 0; j < i; j++) {
        delete created[j];
        created[j] = nullptr;
      }
      return false;
    }
  }

  if (createdChannelMap != nullptr) {
    ChannelMap map;
    for (uint8_t i = 0; i < definition.channelCount; i++) {
      if (!map.add(definition.channels[i].channelId,
                   created[i]->getChannel()->getChannelNumber())) {
        for (uint8_t j = 0; j < definition.channelCount; j++) {
          delete created[j];
          created[j] = nullptr;
        }
        return false;
      }
    }
    *createdChannelMap = map;
  }

  return true;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED
