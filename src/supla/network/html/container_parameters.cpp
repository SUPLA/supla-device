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

#include "container_parameters.h"

#include <supla/storage/config.h>
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
  char tmp[100] = {};
  snprintf(tmp, sizeof(tmp), "Container #%d", container->getChannelNumber());

  sender->send("</div><div class=\"box\">");
  sender->send("<h3>");
  sender->send(tmp);
  sender->send("</h3>");

  // form-field BEGIN
  container->generateKey(key, WarningAboveKey);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Warning above level");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"1\" min=\"0\" max=\"100\" ");
  sender->sendNameAndId(key);
  sender->send(" placeholder=\"not set\"");
  sender->send(" value=\"");
  auto warningAbove = container->getWarningAboveLevel();
  if (warningAbove >= 0 && warningAbove <= 100) {
    sender->send(warningAbove);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  container->generateKey(key, AlarmAboveKey);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Alarm above level");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"1\" min=\"0\" max=\"100\" ");
  sender->sendNameAndId(key);
  sender->send(" placeholder=\"not set\"");
  sender->send(" value=\"");
  auto alarmAbove = container->getAlarmAboveLevel();
  if (alarmAbove >= 0 && alarmAbove <= 100) {
    sender->send(alarmAbove);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  container->generateKey(key, WarningBelowKey);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Warning below level");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"1\" min=\"0\" max=\"100\" ");
  sender->sendNameAndId(key);
  sender->send(" placeholder=\"not set\"");
  sender->send(" value=\"");
  auto warningBelow = container->getWarningBelowLevel();
  if (warningBelow >= 0 && warningBelow <= 100) {
    sender->send(warningBelow);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

    // form-field BEGIN
  container->generateKey(key, AlarmBelowKey);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Alarm below level");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"1\" min=\"0\" max=\"100\" ");
  sender->sendNameAndId(key);
  sender->send(" placeholder=\"not set\"");
  sender->send(" value=\"");
  auto alarmBelow = container->getAlarmBelowLevel();
  if (alarmBelow >= 0 && alarmBelow <= 100) {
    sender->send(alarmBelow);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  container->generateKey(key, MuteKey);
  sender->send("<div class=\"form-field right-checkbox\">");
  sender->sendLabelFor(key, "Mute alarm sound without additional auth");
  sender->send("<label>");
  sender->send("<span class=\"switch\">");
  sender->send("<input type=\"checkbox\" value=\"on\" ");
  sender->send(
      checked(container->isMuteAlarmSoundWithoutAdditionalAuth()));
  sender->sendNameAndId(key);
  sender->send(">");
  sender->send("<span class=\"slider\"></span>");
  sender->send("</span>");
  sender->send("</label>");
  sender->send("</div>");
  // form-field END

  if (allowSensors) {
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
          sender->send("<hr>");
        }
        char buffer[100] = {};
        snprintf(buffer, sizeof(buffer), "Sensor %d", i + 1);
        sender->send("<h3>");
        sender->send(buffer);
        sender->send("</h3>");

        // form-field BEGIN
        sender->send("<div class=\"form-field\">");
        generateSensorKey(key, SensorIdKey, i);
        sender->sendLabelFor(key, "Sensor channel");
        sender->send("<div>");
        sender->send("<select ");
        sender->sendNameAndId(key);
        sender->send(" class='");
        sender->send(containerClassId);
        sender->send("'>");
        sender->sendSelectItem(
            0,
            "Not used",
            container->config.sensorData[i].channelNumber == 255,
            true);
        for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
            ch = ch->next()) {
          auto channelType = ch->getChannelType();
          if (channelType == SUPLA_CHANNELTYPE_BINARYSENSOR) {
            char buf[100] = {};
            auto channelNumber = ch->getChannelNumber();
            snprintf(buf, sizeof(buf), "Use sensor #%d", channelNumber);
            sender->sendSelectItem(
                channelNumber,
                buf,
                container->config.sensorData[i].channelNumber ==
                    channelNumber);
          }
        }
        sender->send("</select>");
        sender->send("</div>");
        sender->send("</div>");
        // form-field END

        // form-field BEGIN
        generateSensorKey(key, SensorLevelKey, i);
        sender->send("<div class=\"form-field\">");
        sender->sendLabelFor(key, "Sensor level");
        sender->send("<div>");
        sender->send(
            "<input type=\"number\" step=\"1\" min=\"0\" max=\"100\" "
            "required ");
        sender->sendNameAndId(key);
        sender->send(" value=\"");
        auto fillLevel = container->config.sensorData[i].fillLevel;
        if (fillLevel > 100) {
          fillLevel = 100;
        }
        sender->send(fillLevel);
        sender->send("\">");
        sender->send("</div>");
        sender->send("</div>");
        // form-field END
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
