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
#include "supla/control/hvac_base.h"

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

  sender->send("</div><div class=\"box\">");
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
  if (hvac->isFunctionSupported(SUPLA_CHANNELFNC_HVAC_THERMOSTAT)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
        "Room thermostat",
        channelFunc == SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  }
  if (hvac->isFunctionSupported(SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER)) {
    sender->sendSelectItem(
        SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER,
        "Domestic hot water",
        channelFunc == SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER);
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
  if (hvac->getChannelFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    hvac->generateKey(key, "subfnc");
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(key, "Room thermostat function");
    sender->send("<div>");
    sender->send("<select ");
    sender->sendNameAndId(key);
    sender->send(">");
    sender->sendSelectItem(
        SUPLA_HVAC_SUBFUNCTION_HEAT,
        "Heat",
        hvac->isHeatingSubfunction());
    sender->sendSelectItem(
        SUPLA_HVAC_SUBFUNCTION_COOL,
        "Cool",
        hvac->isCoolingSubfunction());
    sender->send("</select>");
    sender->send("</div>");
    sender->send("</div>");
  }
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
  sender->sendLabelFor(key, "Heating temperature setpoint [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tHeat = hvac->getTemperatureSetpointHeat();
  if (tHeat != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(tHeat) / 100.0);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_max");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Cooling temperature setpoint [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tCool = hvac->getTemperatureSetpointCool();
  if (tCool != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(tCool) / 100.0);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  sender->send("<h2>Thermometers configuration</h2>");
  // form-field BEGIN
  hvac->generateKey(key, "t_main");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Main thermometer channel number");
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
  hvac->generateKey(key, "t_aux");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Auxiliary thermometer channel number");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(">");
  sender->sendSelectItem(hvac->getChannelNumber(), "Not set",
      hvac->getAuxThermometerChannelNo() == hvac->getChannelNumber());
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
  hvac->generateKey(key, "t_aux_type");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Auxiliary thermometer type");
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

  // form-field BEGIN
  hvac->generateKey(key, "aux_ctrl");
  sender->send("<div class=\"form-field right-checkbox\">");
  sender->sendLabelFor(key,
                       "Enable auxiliary min and max setpoints");
  sender->send("<label>");
  sender->send("<div class=\"switch\">");
  sender->send("<input type=\"checkbox\" value=\"on\" ");
  sender->send(
      checked(hvac->isAuxMinMaxSetpointEnabled()));
  sender->sendNameAndId(key);
  sender->send(" onchange=\"auxSetpointEnabledChange();\">");
  sender->send("<span class=\"slider\"></span>");
  sender->send("</div>");
  sender->send("</label>");
  sender->send("</div>");
  // form-field END

  // hide/show for aux min/max setpoints
  sender->send(
      "<script>"
      "function auxSetpointEnabledChange(){"
      "var e=document.getElementById(\"");
  sender->send(key);
  sender->send("\"),"
      "c=document.getElementById(\"aux_settings\"),"
      "l=e.checked?\"block\":\"none\";"
      "c.style.display=l;}"
      "</script>");

  // form-field BEGIN
  sender->send("<div id=\"aux_settings\" ");
  if (hvac->isAuxMinMaxSetpointEnabled()) {
    sender->send("style=\"display:block;\">");
  } else {
    sender->send("style=\"display:none;\">");
  }

  // form-field BEGIN
  hvac->generateKey(key, "t_aux_min");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Aux min. temperature setpoint [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tAuxMinSetpoint = hvac->getTemperatureAuxMinSetpoint();
  if (tAuxMinSetpoint != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%.1f",
             static_cast<double>(tAuxMinSetpoint) / 100.0);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div></div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_aux_max");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Aux max temperature setpoint [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tAuxMaxSetpoint = hvac->getTemperatureAuxMaxSetpoint();
  if (tAuxMaxSetpoint != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%.1f",
             static_cast<double>(tAuxMaxSetpoint) / 100.0);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div></div></div>");
  // form-field END

  sender->send("<h2>Anti freeze and overheat protection</h2>");
  // form-field BEGIN
  hvac->generateKey(key, "anti_freeze");
  sender->send("<div class=\"form-field right-checkbox\">");
  sender->sendLabelFor(key, "Enable anti-freeze and overheat protection");
  sender->send("<label>");
  sender->send("<div class=\"switch\">");
  sender->send("<input type=\"checkbox\" value=\"on\" ");
  sender->send(checked(hvac->isAntiFreezeAndHeatProtectionEnabled()));
  sender->sendNameAndId(key);
  sender->send(" onchange=\"antiFreezeAndHeatProtectionChange();\">");
  sender->send("<span class=\"slider\"></span>");
  sender->send("</div>");
  sender->send("</label>");
  sender->send("</div>");
  // form-field END

  // hide/show for antifreeze/overheat
  sender->send(
      "<script>"
      "function antiFreezeAndHeatProtectionChange(){"
      "var e=document.getElementById(\"");
  sender->send(key);
  sender->send("\"),"
      "c=document.getElementById(\"antifreeze_settings\"),"
      "l=e.checked?\"block\":\"none\";"
      "c.style.display=l;}"
      "</script>");

  // form-field BEGIN
  sender->send("<div id=\"antifreeze_settings\" ");
  if (hvac->isAuxMinMaxSetpointEnabled()) {
    sender->send("style=\"display:block;\">");
  } else {
    sender->send("style=\"display:none;\">");
  }

  // form-field BEGIN
  hvac->generateKey(key, "t_freeze");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Freeze protection [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tFreeze = hvac->getTemperatureFreezeProtection();
  if (tFreeze != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%d.%1d", tFreeze / 100, (tFreeze / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_heat");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Overheat protection [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tHeatProtection = hvac->getTemperatureHeatProtection();
  if (tHeatProtection != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf,
             sizeof(buf),
             "%d.%1d",
             tHeatProtection / 100,
             (tHeatProtection / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div></div></div>");
  // form-field END


  sender->send("<h2>Behavior settings</h2>");

  // form-field BEGIN
  int countSensors = 0;
  int sensorId = 0;
  for (int i = 0; i < Supla::Channel::reg_dev.channel_count; i++) {
    if (Supla::Channel::reg_dev.channels[i].Type ==
        SUPLA_CHANNELTYPE_BINARYSENSOR) {
      countSensors++;
      sensorId = i;
    }
  }
  if (countSensors > 0) {
    hvac->generateKey(key, "sensor");
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(key, "Turn off based on sensor state");
    sender->send("<div>");
    sender->send("<select ");
    sender->sendNameAndId(key);
    sender->send(">");
    sender->sendSelectItem(
        hvac->getChannelNumber(),
        "Disabled",
        hvac->getBinarySensorChannelNo() == hvac->getChannelNumber());
    if (countSensors == 1) {
      sender->sendSelectItem(
          sensorId,
          "Enabled",
          hvac->getBinarySensorChannelNo() == sensorId);
    } else {
      for (int i = 0; i < Supla::Channel::reg_dev.channel_count; i++) {
        char buf[100] = {};
        snprintf(buf, sizeof(buf), "Use sensor #%d", i);
        sender->sendSelectItem(i, buf, hvac->getBinarySensorChannelNo() == i);
      }
    }
    sender->send("</select>");
    sender->send("</div>");
    sender->send("</div>");
  }
  // form-field END

  // Currently on/off is the only supported algorithm, so there is nothing
  // to change. Uncomment when more algorithms are supported.
  // form-field BEGIN
  hvac->generateKey(key, "algorithm");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Algorithm");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(">");
  if (hvac->isAlgorithmValid(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE)) {
    sender->sendSelectItem(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE,
                           "On/Off middle",
                           hvac->getUsedAlgorithm() ==
                               SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  }
  if (hvac->isAlgorithmValid(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST)) {
    sender->sendSelectItem(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST,
                           "On/Off at most",
                           hvac->getUsedAlgorithm() ==
                               SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);
  }
  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_hister");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Histeresis [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tHister = hvac->getTemperatureHisteresis();
  if (tHister != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%d.%1d", tHister / 100, (tHister / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "min_on_s");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key,
                       "Minimum ON time before output can be turned off [s]");
  sender->send("<div>");
  sender->send("<input type=\"number\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto minOnTimeS = hvac->getMinOnTimeS();
  char buf[100] = {};
  snprintf(buf, sizeof(buf), "%d", minOnTimeS);
  sender->send(buf);
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "min_off_s");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key,
                       "Minimum OFF time before output can be turned on [s]");
  sender->send("<div>");
  sender->send("<input type=\"number\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto minOffTimeS = hvac->getMinOffTimeS();
  snprintf(buf, sizeof(buf), "%d", minOffTimeS);
  sender->send(buf);
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "error_val");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Output value on error");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(key);
  sender->send(">");
  sender->sendSelectItem(0, "Off", hvac->getOutputValueOnError() == 0);
  sender->sendSelectItem(100, "Heat", hvac->getOutputValueOnError() == 100);
  sender->sendSelectItem(-100, "Cool", hvac->getOutputValueOnError() == -100);
  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

// "Temperature setpoint change switches to manual mode"
  // form-field BEGIN
  hvac->generateKey(key, "t_chng_keeps");
  sender->send("<div class=\"form-field right-checkbox\">");
  sender->sendLabelFor(key,
                       "Temperature setpoint change switches to manual mode");
  sender->send("<label>");
  sender->send("<div class=\"switch\">");
  sender->send("<input type=\"checkbox\" value=\"on\" ");
  sender->send(
      checked(hvac->isTemperatureSetpointChangeSwitchesToManualMode()));
  sender->sendNameAndId(key);
  sender->send(">");
  sender->send("<span class=\"slider\"></span>");
  sender->send("</div>");
  sender->send("</label>");
  sender->send("</div>");
  // form-field END

  /*
  sender->send("<h2>Temperatures configuration</h2>");
  // form-field BEGIN
  hvac->generateKey(key, "t_eco");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Eco [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tEco = hvac->getTemperatureEco();
  if (tEco != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%d.%1d", tEco / 100, (tEco / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_comfort");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Comfort [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tComfort = hvac->getTemperatureComfort();
  if (tComfort != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%d.%1d", tComfort / 100, (tComfort / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_boost");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Boost [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tBoost = hvac->getTemperatureBoost();
  if (tBoost != INT16_MIN) {
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%d.%1d", tBoost / 100, (tBoost / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_below");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Below alarm [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tBelowAlarm = hvac->getTemperatureBelowAlarm();
  if (tBelowAlarm != INT16_MIN) {
    char buf[100] = {};
    snprintf(
        buf, sizeof(buf), "%d.%1d", tBelowAlarm / 100, (tBelowAlarm / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  // form-field BEGIN
  hvac->generateKey(key, "t_above");
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(key, "Above alarm [°C]");
  sender->send("<div>");
  sender->send("<input type=\"number\" step=\"0.1\" ");
  sender->sendNameAndId(key);
  sender->send(" value=\"");
  auto tAboveAlarm = hvac->getTemperatureAboveAlarm();
  if (tAboveAlarm != INT16_MIN) {
    char buf[100] = {};
    snprintf(
        buf, sizeof(buf), "%d.%1d", tAboveAlarm / 100, (tAboveAlarm / 10) % 10);
    sender->send(buf);
  }
  sender->send("\">");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END
  */
}  // NOLINT(readability/fn_size)

bool HvacParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (hvac == nullptr || cfg == nullptr) {
    return false;
  }

  if (newValue == nullptr) {
    newValue = new TSD_SuplaChannelNewValue;
    memset(newValue, 0, sizeof(TSD_SuplaChannelNewValue));
    hvacValue = reinterpret_cast<THVACValue*>(newValue->value);
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
    config->ConfigSize = sizeof(TChannelConfig_HVAC);
    config->ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
    hvacConfig = reinterpret_cast<TChannelConfig_HVAC*>(&(config->Config));
    hvac->copyFullChannelConfigTo(hvacConfig);
    // anti free will be enabled when checkbox is checked, otherwise
    // form field won't be send, so we disable it here
    hvacConfig->AntiFreezeAndOverheatProtectionEnabled = false;
    // as above
    hvacConfig->TemperatureSetpointChangeSwitchesToManualMode = false;
    hvacConfig->AuxMinMaxSetpointEnabled = false;
  }

  if (config == nullptr || newValue == nullptr) {
    SUPLA_LOG_ERROR("Memory allocation failed");
    return false;
  }

  char keyMatch[16] = {};
  hvac->generateKey(keyMatch, "fnc");

  // channel function
  if (strcmp(key, keyMatch) == 0) {
    int32_t channelFunc = stringToUInt(value);
    config->Func = channelFunc;
    return true;
  }

  hvac->generateKey(keyMatch, "subfnc");

  // channel subfunction
  if (strcmp(key, keyMatch) == 0) {
    int32_t subfunction = stringToUInt(value);
    if (subfunction >= 0 && subfunction <= 2) {
      hvacConfig->Subfunction = subfunction;
    }
    return true;
  }


  hvac->generateKey(keyMatch, "hvac_mode");
  // channel mode
  if (strcmp(key, keyMatch) == 0) {
    int32_t mode = stringToUInt(value);
    hvacValue->Mode = mode;
    return true;
  }

  hvac->generateKey(keyMatch, "t_min");
  // setpoint min temperature
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t tHeat = floatStringToInt(value, 1);
      tHeat *= 10;
      Supla::Channel::setHvacSetpointTemperatureHeat(hvacValue, tHeat);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_max");
  // setpoint max temperature
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t tCool = floatStringToInt(value, 1);
      tCool *= 10;
      Supla::Channel::setHvacSetpointTemperatureCool(hvacValue, tCool);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_main");
  // main thermometer
  if (strcmp(key, keyMatch) == 0) {
    int32_t tMain = stringToUInt(value);
    hvacConfig->MainThermometerChannelNo = tMain;
    return true;
  }

  hvac->generateKey(keyMatch, "t_aux");
  // aux thermometer
  if (strcmp(key, keyMatch) == 0) {
    int32_t tAux = stringToUInt(value);
    hvacConfig->AuxThermometerChannelNo = tAux;
    return true;
  }

  hvac->generateKey(keyMatch, "t_aux_type");
  // aux thermometer type
  if (strcmp(key, keyMatch) == 0) {
    int32_t tSecType = stringToUInt(value);
    hvacConfig->AuxThermometerType = tSecType;
    return true;
  }

  hvac->generateKey(keyMatch, "anti_freeze");
  // anti freeze temperature
  if (strcmp(key, keyMatch) == 0) {
    bool antiFreeze = (strcmp(value, "on") == 0);
    hvacConfig->AntiFreezeAndOverheatProtectionEnabled = antiFreeze;
    return true;
  }

  hvac->generateKey(keyMatch, "t_chng_keeps");
  // temperature setpoint change switch to manual mode
  if (strcmp(key, keyMatch) == 0) {
    bool tChangeSwitch = (strcmp(value, "on") == 0);
    hvacConfig->TemperatureSetpointChangeSwitchesToManualMode = tChangeSwitch;
    return true;
  }

  hvac->generateKey(keyMatch, "sensor");
  // sensor
  if (strcmp(key, keyMatch) == 0) {
    int32_t sensor = stringToUInt(value);
    hvacConfig->BinarySensorChannelNo = sensor;
    return true;
  }

  hvac->generateKey(keyMatch, "algorithm");
  // algorithm
  if (strcmp(key, keyMatch) == 0) {
    int32_t algorithm = stringToUInt(value);
    hvacConfig->UsedAlgorithm = algorithm;
    return true;
  }

  hvac->generateKey(keyMatch, "min_on_s");
  // min on time
  if (strcmp(key, keyMatch) == 0) {
    int32_t minOnTimeS = stringToInt(value);
    hvacConfig->MinOnTimeS = minOnTimeS;
    return true;
  }

  hvac->generateKey(keyMatch, "min_off_s");
  // min off time
  if (strcmp(key, keyMatch) == 0) {
    int32_t minOffTimeS = stringToInt(value);
    hvacConfig->MinOffTimeS = minOffTimeS;
    return true;
  }

  hvac->generateKey(keyMatch, "t_freeze");
  // temperature freeze
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_FREEZE_PROTECTION,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_eco");
  // temperature eco
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_ECO,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_comfort");
  // temperature comfort
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_COMFORT,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_boost");
  // temperature boost
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_BOOST,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_heat");
  // temperature heat
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_HEAT_PROTECTION,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_hister");
  // temperature hister
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_HISTERESIS,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_below");
  // temperature below alarm
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_BELOW_ALARM,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_above");
  // temperature above alarm
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_ABOVE_ALARM,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "aux_ctrl");
  // aux min/max setpoint enabled
  if (strcmp(key, keyMatch) == 0) {
    bool auxMinMaxEnabled = (strcmp(value, "on") == 0);
    hvacConfig->AuxMinMaxSetpointEnabled = auxMinMaxEnabled;
    return true;
  }

  hvac->generateKey(keyMatch, "t_aux_min");
  // temperature aux min setpoint
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_AUX_MIN_SETPOINT,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_aux_max");
  // temperature aux max setpoint
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_AUX_MAX_SETPOINT,
          temperature);
    }
    return true;
  }


  hvac->generateKey(keyMatch, "error_val");
  // output on error value
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      signed char errorValue = stringToInt(value);
      hvacConfig->OutputValueOnError = errorValue;
    }
    return true;
  }

  return false;
}

void HvacParameters::onProcessingEnd() {
  if (config != nullptr) {
    hvac->handleChannelConfig(config, true);

    hvacConfig = nullptr;
    delete config;
    config = nullptr;
  }

  if (newValue != nullptr) {
    hvac->handleNewValueFromServer(newValue);

    delete newValue;
    hvacValue = nullptr;
    newValue = nullptr;
  }
}
