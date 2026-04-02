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
#include "disable_user_interface_parameter.h"

#include <SuplaDevice.h>
#include <string.h>
#include <supla/element.h>
#include <supla/log_wrapper.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

using Supla::Html::DisableUserInterfaceParameter;

DisableUserInterfaceParameter::DisableUserInterfaceParameter()
    : HtmlElement(HTML_SECTION_FORM) {
}

DisableUserInterfaceParameter::~DisableUserInterfaceParameter() {
}

void DisableUserInterfaceParameter::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint8_t value = 0;  // default value
    cfg->getUInt8(Supla::ConfigTag::DisableUserInterfaceCfgTag, &value);

    sender->labeledField(
        Supla::ConfigTag::DisableUserInterfaceCfgTag,
        "Local interface restriction",
        [&]() {
          auto select =
              sender->selectTag(Supla::ConfigTag::DisableUserInterfaceCfgTag,
                                Supla::ConfigTag::DisableUserInterfaceCfgTag);
          select.attr("onchange", "disableUserInterfaceChange()");
          select.body([&]() {
            sender->selectOption(0, "NONE", value == 0);
            sender->selectOption(1, "FULL", value == 1);
            sender->selectOption(2, "Allow temperature change", value == 2);
          });
          sender->tag("p").body(
              "Warning: you can enter config mode only from Cloud, local user "
              "interface, and by power cycling the device 3 times.");
        });

    sender->send(
        "<script>"
        "function disableUserInterfaceChange(){"
        "var e=document.getElementById(\"disable_ui\"),"
        "c=document.getElementById(\"min_max_temp_ui\"),"
        "l=\"2\"==e.value?\"block\":\"none\";"
        "c.style.display=l;}"
        "</script>");

    auto roundToTenth = [](int32_t valueInHundredths) {
      return valueInHundredths >= 0 ? (valueInHundredths + 5) / 10
                                    : (valueInHundredths - 5) / 10;
    };

    auto emitTempField = [&](const char* id, const char* label, int32_t raw) {
      Supla::NumericInputSpec spec;
      spec.min = Supla::fixed(roundToTenth(INT16_MIN), 1);
      spec.max = Supla::fixed(roundToTenth(INT16_MAX), 1);
      spec.step = Supla::fixed(1, 1);
      spec.value = Supla::fixed(roundToTenth(raw), 1);
      sender->labeledField(
          id, label, [&]() { sender->numberInput(id, id, spec); });
    };

    sender->toggleBox("min_max_temp_ui", value == 2, [&]() {
      int32_t minTempUI = 0;
      cfg->getInt32(Supla::ConfigTag::MinTempUICfgTag, &minTempUI);
      if (minTempUI < INT16_MIN) {
        minTempUI = INT16_MIN;
      }
      if (minTempUI > INT16_MAX) {
        minTempUI = INT16_MAX;
      }

      emitTempField(Supla::ConfigTag::MinTempUICfgTag,
                    "Interface minimum temperature",
                    minTempUI);

      int32_t maxTempUI = 0;
      cfg->getInt32(Supla::ConfigTag::MaxTempUICfgTag, &maxTempUI);
      if (maxTempUI < INT16_MIN) {
        maxTempUI = INT16_MIN;
      }
      if (maxTempUI > INT16_MAX) {
        maxTempUI = INT16_MAX;
      }
      emitTempField(Supla::ConfigTag::MaxTempUICfgTag,
                    "Interface maximum temperature",
                    maxTempUI);
    });
  }
}

bool DisableUserInterfaceParameter::handleResponse(const char* key,
                                                   const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return false;
  }
  if (strcmp(key, Supla::ConfigTag::DisableUserInterfaceCfgTag) == 0) {
    uint8_t currentValue = 0;  // default value
    cfg->getUInt8(Supla::ConfigTag::DisableUserInterfaceCfgTag, &currentValue);
    uint8_t newValue = stringToUInt(value);
    if (newValue > 2) {
      newValue = 2;
    }

    if (newValue != currentValue) {
      cfg->setUInt8(Supla::ConfigTag::DisableUserInterfaceCfgTag, newValue);
      change = true;
    }
    return true;
  }
  if (strcmp(key, Supla::ConfigTag::MinTempUICfgTag) == 0) {
    int32_t currentValue = 0;
    cfg->getInt32(Supla::ConfigTag::MinTempUICfgTag, &currentValue);
    int32_t minTempUI = floatStringToInt(value, 2);
    if (minTempUI < INT16_MIN) {
      minTempUI = INT16_MIN;
    }
    if (minTempUI > INT16_MAX) {
      minTempUI = INT16_MAX;
    }
    if (minTempUI != currentValue) {
      cfg->setInt32(Supla::ConfigTag::MinTempUICfgTag, minTempUI);
      change = true;
    }
    return true;
  }
  if (strcmp(key, Supla::ConfigTag::MaxTempUICfgTag) == 0) {
    int32_t currentValue = 0;
    cfg->getInt32(Supla::ConfigTag::MaxTempUICfgTag, &currentValue);
    int32_t maxTempUI = floatStringToInt(value, 2);

    if (maxTempUI < INT16_MIN) {
      maxTempUI = INT16_MIN;
    }
    if (maxTempUI > INT16_MAX) {
      maxTempUI = INT16_MAX;
    }
    if (maxTempUI != currentValue) {
      cfg->setInt32(Supla::ConfigTag::MaxTempUICfgTag, maxTempUI);
      change = true;
    }
  }
  return false;
}

void DisableUserInterfaceParameter::onProcessingEnd() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (change && cfg) {
    cfg->setDeviceConfigChangeFlag();
    Supla::Element::NotifyElementsAboutConfigChange(
        SUPLA_DEVICE_CONFIG_FIELD_DISABLE_USER_INTERFACE);
  }
  change = false;
}

#endif  // ARDUINO_ARCH_AVR
