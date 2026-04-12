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
#include "container_parameters.h"

#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/network/web_sender.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/channels/channel.h>
#include <supla/sensor/container.h>

#include <stdio.h>
#include <string.h>

using Supla::Html::ContainerParameters;

namespace {
const char WarningAboveKey[] = "wrn_above";
const char WarningBelowKey[] = "wrn_below";
const char AlarmAboveKey[] = "alm_above";
const char AlarmBelowKey[] = "alm_below";
const char MuteKey[] = "mute";
const char SensorIdKey[] = "sns_id";
const char SensorLevelKey[] = "sns_lvl";
}

ContainerParameters::ContainerParameters(bool allowSensors,
                                         Supla::Sensor::Container* container)
    : HtmlElement(HTML_SECTION_FORM),
      container(container),
      allowSensors(allowSensors) {
}

ContainerParameters::~ContainerParameters() {
}

void ContainerParameters::setContainerPtr(Supla::Sensor::Container* container) {
  this->container = container;
}

void ContainerParameters::generateSensorKey(char* key,
                                            const char* prefix, int index) {
  char tmp[16] = {};
  snprintf(tmp, sizeof(tmp), "%s_%d", prefix, index);
  container->generateKey(key, tmp);
}

void ContainerParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (container == nullptr || cfg == nullptr) {
    return;
  }

  char key[16] = {};
  auto emitNumericField = [&](const char* fieldKey,
                              const char* label,
                              int value) {
    Supla::NumericInputSpec spec;
    spec.min = 0;
    spec.max = 100;
    spec.step = 1;
    spec.value = value;
    sender->labeledField(
        fieldKey,
        label,
        [&]() {
          auto input = sender->voidTag("input");
          input.attr("type", "number");
          input.attr("step", 1);
          input.attr("min", 0);
          input.attr("max", 100);
          input.attr("name", fieldKey);
          input.attr("id", fieldKey);
          input.attr("placeholder", "not set");
          input.attr("value", value);
          input.finish();
        });
  };

  sender->send("</div><div class=\"box\">");
  sender->tag("h3").body([&]() {
    char tmp[32] = {};
    snprintf(tmp, sizeof(tmp), "Container #%d", container->getChannelNumber());
    sender->send(tmp);
  });

  container->generateKey(key, WarningAboveKey);
  emitNumericField(
      key, "Warning above level",
      (container->getWarningAboveLevel() >= 0 &&
       container->getWarningAboveLevel() <= 100)
          ? container->getWarningAboveLevel()
          : 0);

  container->generateKey(key, AlarmAboveKey);
  emitNumericField(
      key, "Alarm above level",
      (container->getAlarmAboveLevel() >= 0 &&
       container->getAlarmAboveLevel() <= 100)
          ? container->getAlarmAboveLevel()
          : 0);

  container->generateKey(key, WarningBelowKey);
  emitNumericField(
      key, "Warning below level",
      (container->getWarningBelowLevel() >= 0 &&
       container->getWarningBelowLevel() <= 100)
          ? container->getWarningBelowLevel()
          : 0);

  container->generateKey(key, AlarmBelowKey);
  emitNumericField(
      key, "Alarm below level",
      (container->getAlarmBelowLevel() >= 0 &&
       container->getAlarmBelowLevel() <= 100)
          ? container->getAlarmBelowLevel()
          : 0);

  container->generateKey(key, MuteKey);
  sender->labeledField(
      key,
      "Mute alarm sound without additional auth",
      [&]() {
        sender->tag("label").body([&]() {
          sender->tag("span").attr("class", "switch").body([&]() {
            auto input = sender->voidTag("input");
            input.attr("type", "checkbox");
            input.attr("value", "on");
            input.attrIf("checked",
                         container->isMuteAlarmSoundWithoutAdditionalAuth());
            input.attr("name", key);
            input.attr("id", key);
            input.finish();
            sender->tag("span").attr("class", "slider").body([]() {});
          });
        });
      },
      "form-field right-checkbox");

  if (allowSensors) {
    auto emitSensorSelectField = [&](const char* fieldKey,
                                     const char* label,
                                     const char* cssClass,
                                     int selectedChannel) {
      sender->labeledField(fieldKey, label, [&]() {
        auto select = sender->selectTag(fieldKey, fieldKey);
        if (cssClass) {
          select.attr("class", cssClass);
        }
        select.body([&]() {
          sender->selectOption("", "Not used", selectedChannel == 255);
          for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
               ch = ch->next()) {
            if (ch->getChannelType() == SUPLA_CHANNELTYPE_BINARYSENSOR) {
              char buf[100] = {};
              auto channelNumber = ch->getChannelNumber();
              snprintf(buf, sizeof(buf), "Use sensor #%d", channelNumber);
              sender->selectOption(
                  channelNumber, buf, selectedChannel == channelNumber);
            }
          }
        });
      });
    };

    auto emitSensorLevelField = [&](const char* fieldKey, int fillLevel) {
      fillLevel = fillLevel > 100 ? 100 : fillLevel;
      Supla::NumericInputSpec spec;
      spec.min = 0;
      spec.max = 100;
      spec.step = 1;
      spec.value = fillLevel;
      sender->labeledField(
          fieldKey,
          "Sensor level",
          [&]() {
            auto input = sender->voidTag("input");
            input.attr("type", "number");
            input.attr("step", 1);
            input.attr("min", 0);
            input.attr("max", 100);
            input.attr("required", true);
            input.attr("name", fieldKey);
            input.attr("id", fieldKey);
            input.attr("value", fillLevel);
            input.finish();
          });
    };

    int countSensors = 0;
    for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
        ch = ch->next()) {
      auto channelType = ch->getChannelType();
      if (channelType == SUPLA_CHANNELTYPE_BINARYSENSOR) {
        countSensors++;
      }
    }
      if (countSensors > 0 || container->isSensorDataUsed()) {
      char containerClassId[32] = {};
      snprintf(containerClassId,
               32,
               "mutual-exclusive_%d",
               container->getChannelNumber());
      sender->send(
          "<script>"
          "document.addEventListener('DOMContentLoaded', () => {"
          "const selects = document.querySelectorAll('.");
      sender->send(containerClassId);
      sender->send("');"
            "const updateOptions = () => {"
            "const selectedValues = new Set([...selects]."
              "map(select => select.value).filter(value => value !== ''));"
            "selects.forEach(select => {"
              "[...select.options].forEach(option => {"
              "if (selectedValues.has(option.value) && "
                  "option.value !== select.value) {"
                "option.disabled = true;"
              "} else {"
                "option.disabled = false;"
              "}"
            "});});};"

            "selects.forEach(select => {"
              "select.addEventListener('change', updateOptions);"
            "});"
            "updateOptions();"
            "});"
          "</script>");

      for (int i = 0; i < 10; i++) {
        if (i >= countSensors && config.sensorData[i].channelNumber == 255) {
          continue;
        }
        if (i > 0) {
          sender->voidTag("hr").finish();
        }
        char buffer[100] = {};
        char sensorIdKey[16] = {};
        char sensorLevelKey[16] = {};
        snprintf(buffer, sizeof(buffer), "Sensor %d", i + 1);
        sender->tag("h3").body(buffer);
        generateSensorKey(sensorIdKey, SensorIdKey, i);
        generateSensorKey(sensorLevelKey, SensorLevelKey, i);
        emitSensorSelectField(
            sensorIdKey,
            "Sensor channel",
            containerClassId,
            container->config.sensorData[i].channelNumber);
        emitSensorLevelField(sensorLevelKey,
                             container->config.sensorData[i].fillLevel);
      }
    }
  }
}

int ContainerParameters::parseValue(const char *value) const {
  int val = -1;
  if (strnlen(value, 10) == 0) {
    val = -1;
  } else {
    val = stringToInt(value);
    if (val < 0) {
      val = 0;
    } else if (val > 100) {
      val = 100;
    }
  }
  return val;
}

bool ContainerParameters::handleResponse(const char* key, const char* value) {
  if (container == nullptr) {
    return false;
  }

  char match[16] = {};

  container->generateKey(match, WarningAboveKey);
  if (strcmp(match, key) == 0) {
    int val = parseValue(value);
    if (container->getWarningAboveLevel() != val) {
      container->setWarningAboveLevel(val);
      configChanged = true;
    }
    return true;
  }

  container->generateKey(match, AlarmAboveKey);
  if (strcmp(match, key) == 0) {
    int val = parseValue(value);
    if (container->getAlarmAboveLevel() != val) {
      container->setAlarmAboveLevel(val);
      configChanged = true;
    }
    return true;
  }

  container->generateKey(match, WarningBelowKey);
  if (strcmp(match, key) == 0) {
    int val = parseValue(value);
    if (container->getWarningBelowLevel() != val) {
      container->setWarningBelowLevel(val);
      configChanged = true;
    }
    return true;
  }

  container->generateKey(match, AlarmBelowKey);
  if (strcmp(match, key) == 0) {
    int val = parseValue(value);
    if (container->getAlarmBelowLevel() != val) {
      container->setAlarmBelowLevel(val);
      configChanged = true;
    }
    return true;
  }

  container->generateKey(match, MuteKey);
  if (strcmp(match, key) == 0) {
    if (strcmp(value, "on") == 0) {
      muteSet = true;
    }
    return true;
  }

  // sensors
  if (allowSensors) {
    for (int i = 0; i < 10; i++) {
      generateSensorKey(match, SensorIdKey, i);
      if (strcmp(match, key) == 0) {
        int val = 255;
        if (strnlen(value, 10) > 0) {
          val = stringToInt(value);
        }
        if (val < 0) {
          val = 255;
        } else if (val > 255) {
          val = 255;
        }
        if (container->config.sensorData[i].channelNumber != val) {
          container->config.sensorData[i].channelNumber = val;
          if (val == 255) {
            container->config.sensorData[i].fillLevel = 0;
          }
          configChanged = true;
        }
        return true;
      }

      generateSensorKey(match, SensorLevelKey, i);
      if (strcmp(match, key) == 0) {
        int val = parseValue(value);
        if (container->config.sensorData[i].fillLevel != val) {
          container->config.sensorData[i].fillLevel = val;
          configChanged = true;
        }
        return true;
      }
    }
  }

  return false;
}

void ContainerParameters::onProcessingEnd() {
  if (container) {
    if (container->isMuteAlarmSoundWithoutAdditionalAuth() != muteSet) {
      container->setMuteAlarmSoundWithoutAdditionalAuth(muteSet);
      configChanged = true;
    }
  }
  muteSet = false;

  if (configChanged) {
    container->triggerSetChannelConfig();
    container->saveConfig();
    container->printConfig();
  }
}

#endif  // ARDUINO_ARCH_AVR
