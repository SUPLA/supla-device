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

#include "em_phase_led.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/element.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/storage/config_tags.h>
#include <stdio.h>

using Supla::Html::EmPhaseLedParameters;

EmPhaseLedParameters::EmPhaseLedParameters(Supla::Sensor::ElectricityMeter* em)
    : em(em) {
}

EmPhaseLedParameters::~EmPhaseLedParameters() {
}

void EmPhaseLedParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!em || !cfg) {
    return;
  }

  int8_t value = em->getPhaseLedType();
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(
      key, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedTag);

  // form-field BEGIN
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Phase indicator LED");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(" onChange=\"emPhaseChange(this.value)\">");
  if (em->isPhaseLedTypeSupported(EM_PHASE_LED_TYPE_OFF)) {
    sender->send("<option value=\"0\"");
    sender->send(selected(value == 0));
    sender->send(">OFF</option>");
  }
  if (em->isPhaseLedTypeSupported(EM_PHASE_LED_TYPE_VOLTAGE_PRESENCE)) {
    sender->send("<option value=\"1\"");
    sender->send(selected(value == 1));
    sender->send(">Voltage indicator</option>");
  }
  if (em->isPhaseLedTypeSupported(
          EM_PHASE_LED_TYPE_VOLTAGE_PRESENCE_INVERTED)) {
    sender->send("<option value=\"2\"");
    sender->send(selected(value == 2));
    sender->send(">No voltage indicator</option>");
  }
  if (em->isPhaseLedTypeSupported(EM_PHASE_LED_TYPE_VOLTAGE_LEVEL)) {
    sender->send("<option value=\"3\"");
    sender->send(selected(value == 3));
    sender->send(">Voltage level</option>");
  }
  if (em->isPhaseLedTypeSupported(EM_PHASE_LED_TYPE_POWER_ACTIVE_DIRECTION)) {
    sender->send("<option value=\"4\"");
    sender->send(selected(value == 4));
    sender->send(">Active power direction</option>");
  }
  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // js for emPhaseChange
  sender->send(
      "<script>"
      "function emPhaseChange(value) {"
      "  if (value == 3) {"
      "    document.getElementById('em_phase_voltage').style.display = "
      "'block';"
      "  } else {"
      "    document.getElementById('em_phase_voltage').style.display = "
      "'none';"
      "  }"
      "  if (value == 4) {"
      "    document.getElementById('em_phase_power').style.display = 'block';"
      "  } else {"
      "    document.getElementById('em_phase_power').style.display = 'none';"
      "  }"
      "}"
      "</script>");

  // Voltage div
  sender->send("<div id=\"em_phase_voltage\" ");
  if (value == 3) {
    sender->send("style=\"display:block;\">");
  } else {
    sender->send("style=\"display:none;\">");
  }

  // form-field BEGIN
  Supla::Config::generateKey(
      key, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedVoltageLowTag);
  int32_t voltageLow = em->getLedVoltageLow();
  cfg->getInt32(key, &voltageLow);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Low voltage level [V]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  char buf[100] = {};
  snprintf(buf, sizeof(buf), "%.1f", voltageLow / 100.0);
  sender->send(buf);
  sender->send("\">");
  sender->send("</div></div>");
  // form-field END

  // form-field BEGIN
  Supla::Config::generateKey(
      key, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedVoltageHighTag);
  int32_t voltageHigh = em->getLedVoltageHigh();
  cfg->getInt32(key, &voltageHigh);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "High voltage level [V]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  snprintf(buf, sizeof(buf), "%.1f", voltageHigh / 100.0);
  sender->send(buf);
  sender->send("\">");
  sender->send("</div></div>");
  // form-field END

  sender->send("</div>");

  // Power div
  sender->send("<div id=\"em_phase_power\" ");
  if (value == 4) {
    sender->send("style=\"display:block;\">");
  } else {
    sender->send("style=\"display:none;\">");
  }

  // form-field BEGIN
  Supla::Config::generateKey(
      key, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedPowerLowTag);
  int32_t powerLow = em->getLedPowerLow();
  cfg->getInt32(key, &powerLow);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Low power level [W]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  snprintf(buf, sizeof(buf), "%.1f", powerLow / 100.0);
  sender->send(buf);
  sender->send("\">");
  sender->send("</div></div>");
  // form-field END

  // form-field BEGIN
  Supla::Config::generateKey(
      key, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedPowerHighTag);
  int32_t powerHigh = em->getLedPowerHigh();
  cfg->getInt32(key, &powerHigh);
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "High power level [W]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  snprintf(buf, sizeof(buf), "%.1f", powerHigh / 100.0);
  sender->send(buf);
  sender->send("\">");
  sender->send("</div></div>");
  // form-field END

  sender->send("</div>");
}

bool EmPhaseLedParameters::handleResponse(const char* key, const char* value) {
  if (!em) {
    return false;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  char keyTag[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(
      keyTag, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedTag);
  if (strcmp(keyTag, key) == 0) {
    int ledType = stringToUInt(value);
    int8_t valueInCfg = em->getPhaseLedType();
    cfg->getInt8(keyTag, &valueInCfg);
    if (valueInCfg != ledType) {
      if (ledType > 4) {
        ledType = 1;
      }
      cfg->setInt8(keyTag, ledType);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(keyTag,
                             em->getChannelNumber(),
                             Supla::ConfigTag::EmPhaseLedVoltageLowTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t voltageLow = floatStringToInt(value, 2);
    int32_t valueInCfg = em->getLedVoltageLow();
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != voltageLow) {
      cfg->setInt32(keyTag, voltageLow);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(keyTag,
                             em->getChannelNumber(),
                             Supla::ConfigTag::EmPhaseLedVoltageHighTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t voltageHigh = floatStringToInt(value, 2);
    int32_t valueInCfg = em->getLedVoltageHigh();
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != voltageHigh) {
      cfg->setInt32(keyTag, voltageHigh);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(
      keyTag, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedPowerLowTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t powerLow = floatStringToInt(value, 2);
    int32_t valueInCfg = em->getLedPowerLow();
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != powerLow) {
      cfg->setInt32(keyTag, powerLow);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(
      keyTag, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedPowerHighTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t powerHigh = floatStringToInt(value, 2);
    int32_t valueInCfg = em->getLedPowerHigh();
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != powerHigh) {
      cfg->setInt32(keyTag, powerHigh);
      channelConfigChanged = true;
    }
    return true;
  }

  return false;
}

void EmPhaseLedParameters::onProcessingEnd() {
  if (channelConfigChanged && em) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setChannelConfigChangeFlag(em->getChannelNumber());
    }
    if (em) {
      em->onLoadConfig(nullptr);
    }
  }
  channelConfigChanged = false;
}
