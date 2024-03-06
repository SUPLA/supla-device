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
#include <stdio.h>

using Supla::Html::EmPhaseLedParameters;

EmPhaseLedParameters::EmPhaseLedParameters(int channelNo,
                                           Supla::Element* notify)
    : channelNo(channelNo), notify(notify) {
}

EmPhaseLedParameters::~EmPhaseLedParameters() {
}

void EmPhaseLedParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  int8_t value = 1;  // default
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(key, channelNo, EmPhaseLedTag);

  if (!cfg) {
    return;
  }
  cfg->getInt8(key, &value);

  // form-field BEGIN
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Phase indicator LED");
  sender->send("<div>");
  sender->send(
      "<select ");
  sender->sendNameAndId(key);
  sender->send(
      " onChange=\"emPhaseChange(this.value)\">"
      "<option value=\"0\"");
  sender->send(selected(value == 0));
  sender->send(
      ">OFF</option>"
      "<option value=\"1\"");
  sender->send(selected(value == 1));
  sender->send(
      ">Voltage indicator</option>"
      "<option value=\"2\"");
  sender->send(selected(value == 2));
  sender->send(
      ">No voltage indicator</option>"
      "<option value=\"3\"");
  sender->send(selected(value == 3));
  sender->send(
      ">Voltage level</option>"
      "<option value=\"4\"");
  sender->send(selected(value == 4));
  sender->send(
      ">Active power direction</option>"
      "</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // js for emPhaseChange
  sender->send(
      "<script>"
      "function emPhaseChange(value) {"
      "  if (value == 3) {"
      "    document.getElementById('em_phase_voltage').style.display = 'block';"
      "  } else {"
      "    document.getElementById('em_phase_voltage').style.display = 'none';"
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
  Supla::Config::generateKey(key, channelNo, EmPhaseLedVoltageLowTag);
  int32_t voltageLow = 21000;  // 210.00
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
  Supla::Config::generateKey(key, channelNo, EmPhaseLedVoltageHighTag);
  int32_t voltageHigh = 25000;  // 250.00
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
  Supla::Config::generateKey(key, channelNo, EmPhaseLedPowerLowTag);
  int32_t powerLow = -5000;  // -50.00 W
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
  Supla::Config::generateKey(key, channelNo, EmPhaseLedPowerHighTag);
  int32_t powerHigh = 5000;  // 50.00 W
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
  auto cfg = Supla::Storage::ConfigInstance();
  char keyTag[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(keyTag, channelNo, EmPhaseLedTag);
  if (strcmp(keyTag, key) == 0) {
    int ledType = stringToUInt(value);
    int8_t valueInCfg = 1;
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

  Supla::Config::generateKey(keyTag, channelNo, EmPhaseLedVoltageLowTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t voltageLow = floatStringToInt(value, 2);
    int32_t valueInCfg = 21000;
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != voltageLow) {
      cfg->setInt32(keyTag, voltageLow);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(keyTag, channelNo, EmPhaseLedVoltageHighTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t voltageHigh = floatStringToInt(value, 2);
    int32_t valueInCfg = 25000;
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != voltageHigh) {
      cfg->setInt32(keyTag, voltageHigh);
      channelConfigChanged = true;
    }
    return true;
  }

    Supla::Config::generateKey(keyTag, channelNo, EmPhaseLedPowerLowTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t powerLow = floatStringToInt(value, 2);
    int32_t valueInCfg = -5000;
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != powerLow) {
      cfg->setInt32(keyTag, powerLow);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(keyTag, channelNo, EmPhaseLedPowerHighTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t powerHigh = floatStringToInt(value, 2);
    int32_t valueInCfg = 5000;
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
  if (channelConfigChanged) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setChannelConfigChangeFlag(channelNo);
    }
    if (notify) {
      notify->onLoadConfig(nullptr);
    }
  }
  channelConfigChanged = false;
}
