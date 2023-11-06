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

#include "screen_brightness_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/clock/clock.h>

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
    cfg->getInt32(Supla::Html::ScreenBrightnessCfgTag, &cfgBrightness);
    cfg->getInt32(Supla::Html::ScreenAdjustmentForAutomaticCfgTag,
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

    // form-field BEGIN
    sender->send("<div class=\"form-field right-checkbox\">");
    sender->sendLabelFor("auto_bright", "Automatic screen brightness");
    sender->send("<label>");
    sender->send("<div class=\"switch\">");
    sender->send("<input type=\"checkbox\" value=\"on\" ");
    sender->send(checked(automatic));
    sender->sendNameAndId("auto_bright");
    sender->send(" onclick=\"showHideBrightnessSettingsToggle()\">");
    sender->send("<span class=\"slider\"></span>");
    sender->send("</div>");
    sender->send("</label>");
    sender->send("</div>");
    // form-field END

    // adjustment for auto
    sender->send("<div id=\"adjustment_auto_box\" style=\"display: ");
    if (automatic) {
      sender->send("block\">");
    } else {
      sender->send("none\">");
    }

    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(
        Supla::Html::ScreenAdjustmentForAutomaticCfgTag,
        "Brightness adjustment for automatic mode");
    sender->send(
        "<input type=\"range\" min=\"-100\" max=\"100\" step=\"10\" "
        "class=\"range-slider\"");
    sender->sendNameAndId(Supla::Html::ScreenAdjustmentForAutomaticCfgTag);
    sender->send(" value=\"");
    sender->send(cfgAdjustmentForAutomatic, 0);
    sender->send("\">");
    sender->send(
        "<div style=\"text-align: center;\">0</div>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END


    // form-field BEGIN
    sender->send("<div id=\"brightness_settings_box\" style=\"display: ");
    if (automatic) {
      sender->send("none\">");
    } else {
      sender->send("block\">");
    }

    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(Supla::Html::ScreenBrightnessCfgTag,
                         "Screen brightness");
    sender->send(
        "<input type=\"range\" min=\"1\" max=\"100\" step=\"1\" "
        "class=\"range-slider\" ");
        sender->sendNameAndId(Supla::Html::ScreenBrightnessCfgTag);
    sender->send(" value=\"");
    sender->send(cfgBrightness, 0);
    sender->send("\">");
    sender->send("</div>");
    sender->send("</div>");  // time setting box end

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
    cfg->getInt32(Supla::Html::ScreenBrightnessCfgTag, &currentValue);

    if (currentValue != -1) {
      cfg->setInt32(Supla::Html::ScreenBrightnessCfgTag, -1);
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_SCREEN_BRIGHTNESS);
    }
    return true;
  }
  if (cfg && strcmp(key, Supla::Html::ScreenBrightnessCfgTag) == 0) {
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
    cfg->getInt32(Supla::Html::ScreenBrightnessCfgTag, &currentValue);

    if (currentValue != param) {
      cfg->setInt32(Supla::Html::ScreenBrightnessCfgTag, param);
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_SCREEN_BRIGHTNESS);
    }
    return true;
  }

  if (cfg &&
      strcmp(key, Supla::Html::ScreenAdjustmentForAutomaticCfgTag) == 0) {
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
    cfg->getInt32(Supla::Html::ScreenAdjustmentForAutomaticCfgTag,
                  &currentValue);
    if (currentValue != param) {
      cfg->setInt32(Supla::Html::ScreenAdjustmentForAutomaticCfgTag, param);
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
