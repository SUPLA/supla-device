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

  char keyMatch[16] = {};
  hvac->generateKey(keyMatch, "fnc");

  // channel function
  if (strcmp(key, keyMatch) == 0) {
    int32_t channelFunc = stringToUInt(value);
    if (hvac->isFunctionSupported(channelFunc)) {
      hvac->changeFunction(channelFunc, true);
    }
    return true;
  }
  return false;
}
