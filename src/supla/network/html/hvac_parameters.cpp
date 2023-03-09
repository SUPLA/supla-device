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

#include "hvac_parameters.h"

#include <supla/storage/storage.h>
#include <supla/network/web_sender.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>

#include <stdio.h>
#include <string.h>

using Supla::Html::HvacParameters;

HvacParameters::HvacParameters(Supla::Control::HvacBase* hvac)
    : HtmlElement(HTML_SECTION_FORM), hvac(hvac) {
}

HvacParameters::~HvacParameters() {
}

void HvacParameters::setHvacPtr(Supla::Control::HvacBase* hvac) {
  this->hvac = hvac;
}

void HvacParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (hvac == nullptr || cfg == nullptr) {
    return;
  }

  char key[16] = {};
  int32_t channelFunc = hvac->getChannel()->getDefaultFunction();

  char tmp[100] = {};
  snprintf(tmp, sizeof(tmp), "Thermostat #%d", hvac->getChannelNumber());

  sender->send("<h3>");
  sender->send(tmp);
  sender->send("</h3>");

  // form-field BEGIN
  hvac->generateKey(key, "fnc");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Channel function");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(">");
  if (hvac->isFunctionSupported(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT,
        "Heat",
        channelFunc == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);
  }
  if (hvac->isFunctionSupported(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL,
        "Cool",
        channelFunc == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL);
  }
  if (hvac->isFunctionSupported(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO,
        "Auto (heat + cool)",
        channelFunc == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO);
  }
  if (hvac->isFunctionSupported(
          SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL,
        "Differential",
        channelFunc == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);
  }
  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  auto hvacMode = hvac->getChannel()->getHvacMode();
  hvac->generateKey(key, "hvac_mode");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Mode");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(">");
  if (hvac->isModeSupported(SUPLA_HVAC_MODE_OFF)) {
    sender->sendSelectItem(
        SUPLA_HVAC_MODE_OFF,
        "Off",
        hvacMode == SUPLA_HVAC_MODE_OFF &&
        !hvac->getChannel()->isHvacFlagWeeklySchedule());
  }
  if (hvac->isModeSupported(SUPLA_HVAC_MODE_HEAT)) {
    sender->sendSelectItem(
        SUPLA_HVAC_MODE_HEAT,
        "Heat",
        hvacMode == SUPLA_HVAC_MODE_HEAT &&
        !hvac->getChannel()->isHvacFlagWeeklySchedule());
  }
  if (hvac->isModeSupported(SUPLA_HVAC_MODE_COOL)) {
    sender->sendSelectItem(
        SUPLA_HVAC_MODE_COOL,
        "Cool",
        hvacMode == SUPLA_HVAC_MODE_COOL);
  }
  if (hvac->isModeSupported(SUPLA_HVAC_MODE_AUTO)) {
    sender->sendSelectItem(
        SUPLA_HVAC_MODE_AUTO,
        "Auto (heat + cool)",
        hvacMode == SUPLA_HVAC_MODE_AUTO &&
        !hvac->getChannel()->isHvacFlagWeeklySchedule());
  }
  if (hvac->isModeSupported(SUPLA_HVAC_MODE_CMD_TURN_ON)) {
    sender->sendSelectItem(
        SUPLA_HVAC_MODE_CMD_TURN_ON,
        "Turn on",
        false);
  }
  if (hvac->isModeSupported(SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE)) {
    sender->sendSelectItem(
        SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE,
        "Weekly schedule",
        hvac->getChannel()->isHvacFlagWeeklySchedule());
  }

  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_min");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Setpoint min. temperature");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tMin = hvac->getTemperatureSetpointMin();
  if (tMin != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%d.%1d", tMin / 100, (tMin / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_max");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Setpoint max. temperature");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tMax = hvac->getTemperatureSetpointMax();
  if (tMax != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%d.%1d", tMax / 100, (tMax / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_main");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Main thermomemter");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(">");
  for (int i = 0; i < Supla::Channel::reg_dev.channel_count; i++) {
    if (Supla::Channel::reg_dev.channels[i].Type ==
            SUPLA_CHANNELTYPE_THERMOMETER ||
        Supla::Channel::reg_dev.channels[i].Type ==
            SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR) {
      char buf[100] = {};
      snprintf(buf, sizeof(buf), "%d", i);
      sender->sendSelectItem(
          i,
          buf,
          hvac->getMainThermometerChannelNo() == i);
    }
  }
  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_sec");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Secondary thermomemter");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(">");
  for (int i = 0; i < Supla::Channel::reg_dev.channel_count; i++) {
    if (Supla::Channel::reg_dev.channels[i].Type ==
            SUPLA_CHANNELTYPE_THERMOMETER ||
        Supla::Channel::reg_dev.channels[i].Type ==
            SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR) {
      char buf[100] = {};
      snprintf(buf, sizeof(buf), "%d", i);
      sender->sendSelectItem(
          i,
          buf,
          hvac->getAuxThermometerChannelNo() == i);
    }
  }
  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_sec_type");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Secondary thermomemter type");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(">");
  auto tSecType = hvac->getAuxThermometerType();
  sender->sendSelectItem(
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET,
      "Not set",
      tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET);
  sender->sendSelectItem(
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED,
      "Disabled",
      tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED);
  sender->sendSelectItem(
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR,
      "Floor",
      tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR);
  sender->sendSelectItem(
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_WATER,
      "Water",
      tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_WATER);
  sender->sendSelectItem(
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_COOLER,
      "Generic cooler",
      tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_COOLER);
  sender->sendSelectItem(
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER,
      "Generic heater",
      tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END
}

bool HvacParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (hvac == nullptr || cfg == nullptr) {
    return false;
  }

  if (hvacValue == nullptr) {
    hvacValue = new THVACValue;
    memcpy(
        hvacValue,
        &Supla::Channel::reg_dev.channels[hvac->getChannelNumber()].hvacValue,
        sizeof(THVACValue));
  }

  if (config == nullptr) {
    config = new TSD_ChannelConfig;
    memset(config, 0, sizeof(TSD_ChannelConfig));
    config->ChannelNumber = hvac->getChannelNumber();
    config->Func = hvac->getChannel()->getDefaultFunction();
    config->ConfigSize = sizeof(TSD_ChannelConfig_HVAC);
    config->ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
    hvacConfig = reinterpret_cast<TSD_ChannelConfig_HVAC*>(&(config->Config));
  }

  if (config == nullptr || hvacValue == nullptr) {
    SUPLA_LOG_ERROR("Memory allocation failed");
    return false;
  }

  char keyMatch[16] = {};
  hvac->generateKey(keyMatch, "fnc");

  // channel function
  if (strcmp(key, keyMatch) == 0) {
    SUPLA_LOG_DEBUG("Processing fnc");
    int32_t channelFunc = stringToUInt(value);
    config->Func = channelFunc;
//    if (hvac->isFunctionSupported(channelFunc)) {
//      hvac->changeFunction(channelFunc, true);
//    }
    return true;
  }

  hvac->generateKey(keyMatch, "hvac_mode");
  // channel mode
  if (strcmp(key, keyMatch) == 0) {
    SUPLA_LOG_DEBUG("Processing hvac_mode");
    int32_t mode = stringToUInt(value);
    hvacValue->Mode = mode;
//    if (hvac->isModeSupported(mode)) {
//      hvac->setTargetMode(mode, false);
//    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_min");
  // setpoint min temperature
  if (strcmp(key, keyMatch) == 0) {
    SUPLA_LOG_DEBUG("Processing t_min");
    if (strnlen(value, 10) > 0) {
      int16_t tMin = floatStringToInt(value, 1);
      tMin *= 10;
      Supla::Channel::setHvacSetpointTemperatureMin(hvacValue, tMin);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_max");
  // setpoint max temperature
  if (strcmp(key, keyMatch) == 0) {
    SUPLA_LOG_DEBUG("Processing t_max");
    if (strnlen(value, 10) > 0) {
      int16_t tMax = floatStringToInt(value, 1);
      SUPLA_LOG_DEBUG("tMax: %d", tMax);
      tMax *= 10;
      Supla::Channel::setHvacSetpointTemperatureMax(hvacValue, tMax);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_main");
  // main thermometer
  if (strcmp(key, keyMatch) == 0) {
    SUPLA_LOG_DEBUG("Processing t_main");
    int32_t tMain = stringToUInt(value);
    hvacConfig->MainThermometerChannelNo = tMain;
    return true;
  }
  return false;
}

void HvacParameters::onProcessingEnd() {
  // TODO(klew): implement
  hvacConfig = nullptr;
  delete config;
  config = nullptr;
  delete hvacValue;
  hvacValue = nullptr;
}
