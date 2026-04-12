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
#include "screen_brightness_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/clock/clock.h>
#include <supla/storage/config_tags.h>

using Supla::Html::ScreenBrightnessParameters;

ScreenBrightnessParameters::ScreenBrightnessParameters()
    : HtmlElement(HTML_SECTION_FORM) {
}

ScreenBrightnessParameters::~ScreenBrightnessParameters() {
}

void ScreenBrightnessParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t cfgBrightness = -1;  // default value
    int32_t cfgAdjustmentForAutomatic = 0;  // default value
    cfg->getInt32(Supla::ConfigTag::ScreenBrightnessCfgTag, &cfgBrightness);
    cfg->getInt32(Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag,
                  &cfgAdjustmentForAutomatic);
    bool automatic = false;
    if (cfgBrightness == -1) {
      automatic = true;
      cfgBrightness = 100;
    }
    if (cfgBrightness <= 0) {
      cfgBrightness = 1;
    } else if (cfgBrightness > 100) {
      cfgBrightness = 100;
    }
    if (cfgAdjustmentForAutomatic < -100) {
      cfgAdjustmentForAutomatic = -100;
    } else if (cfgAdjustmentForAutomatic > 100) {
      cfgAdjustmentForAutomatic = 100;
    }

    sender->formField([&]() {
      sender->labelFor("auto_bright", "Automatic screen brightness");
      auto label = sender->tag("label");
      label.body([&]() {
        auto switchSpan = sender->tag("span");
        switchSpan.attr("class", "switch").body([&]() {
          auto input = sender->voidTag("input");
          input.attr("type", "checkbox")
              .attr("value", "on")
              .attrIf("checked", automatic)
              .attr("name", "auto_bright")
              .attr("id", "auto_bright")
              .attr("onclick", "showHideBrightnessSettingsToggle()")
              .finish();

          sender->tag("span").attr("class", "slider").body("");
        });
      });
    }, "form-field right-checkbox");

    // adjustment for auto
    sender->toggleBox("adjustment_auto_box", automatic, [&]() {
      sender->formField([&]() {
        sender->labelFor(
            Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag,
            "Brightness adjustment for automatic mode");
        sender->rangeInput(
            Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag,
            {
                .min = -100,
                .max = 100,
                .value = cfgAdjustmentForAutomatic,
                .step = 10,
            },
            "range-slider");
        sender->tag("div").attr("style", "text-align: center;").body("0");
      });
    });

    sender->toggleBox("brightness_settings_box", !automatic, [&]() {
      sender->formField([&]() {
        sender->labelFor(Supla::ConfigTag::ScreenBrightnessCfgTag,
                         "Screen brightness");
        sender->rangeInput(
            Supla::ConfigTag::ScreenBrightnessCfgTag,
            {
                .min = 1,
                .max = 100,
                .value = cfgBrightness,
                .step = 1,
            },
            "range-slider");
      });
    });

    sender->send("<script>"
         "function showHideBrightnessSettingsToggle() {"
            "var checkBox = document.getElementById(\"auto_bright\");"
            "var text1 = document.getElementById(\"brightness_settings_box\");"
            "var text2 = document.getElementById(\"adjustment_auto_box\");"
            "if (checkBox.checked == true){"
              "text1.style.display = \"none\";"
              "text2.style.display = \"block\";"
            "} else {"
              "text1.style.display = \"block\";"
              "text2.style.display = \"none\";"
            "}"
          "}"
        "</script>");
    // form-field END
  }
}

bool ScreenBrightnessParameters::handleResponse(const char* key,
                                                const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && strcmp(key, "auto_bright") == 0) {
    checkboxFound = true;
    int32_t currentValue = -1;  // default value
    cfg->getInt32(Supla::ConfigTag::ScreenBrightnessCfgTag, &currentValue);

    if (currentValue != -1) {
      cfg->setInt32(Supla::ConfigTag::ScreenBrightnessCfgTag, -1);
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_SCREEN_BRIGHTNESS);
    }
    return true;
  }
  if (cfg && strcmp(key, Supla::ConfigTag::ScreenBrightnessCfgTag) == 0) {
    if (checkboxFound) {
      // if checkbox was found, then automatic brightness is used
      return true;
    }
    int32_t param = stringToInt(value);
    if (param < 1) {
      param = 1;
    }
    if (param > 100) {
      param = 100;
    }

    int32_t currentValue = -1;  // default value
    cfg->getInt32(Supla::ConfigTag::ScreenBrightnessCfgTag, &currentValue);

    if (currentValue != param) {
      cfg->setInt32(Supla::ConfigTag::ScreenBrightnessCfgTag, param);
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_SCREEN_BRIGHTNESS);
    }
    return true;
  }

  if (cfg &&
      strcmp(key, Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag) == 0) {
    if (!checkboxFound) {
      return true;
    }
    int32_t param = stringToInt(value);
    if (param < -100) {
      param = -100;
    } else if (param > 100) {
      param = 100;
    }

    int32_t currentValue = 0;  // default value
    cfg->getInt32(Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag,
                  &currentValue);
    if (currentValue != param) {
      cfg->setInt32(Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag,
                    param);
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_SCREEN_BRIGHTNESS);
    }
    return true;
  }

  return false;
}

void ScreenBrightnessParameters::onProcessingEnd() {
  checkboxFound = false;
}

#endif  // ARDUINO_ARCH_AVR
