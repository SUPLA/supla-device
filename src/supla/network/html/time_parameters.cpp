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

#include "time_parameters.h"

// Exclude Arduino AVR
#ifndef ARDUINO_ARCH_AVR

#include <SuplaDevice.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/clock/clock.h>

using Supla::Html::TimeParameters;

TimeParameters::TimeParameters(SuplaDeviceClass* sdc)
    : HtmlElement(HTML_SECTION_FORM), sdc(sdc) {
}

TimeParameters::~TimeParameters() {
}

void TimeParameters::send(Supla::WebSender* sender) {
  if (!sdc || !sdc->getClock()) {
    return;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint8_t value = 1;  // default value
    cfg->getUInt8(Supla::AutomaticTimeSyncCfgTag, &value);

    // form-field BEGIN
    sender->send("<div class=\"form-field right-checkbox\">");
    sender->sendLabelFor(Supla::AutomaticTimeSyncCfgTag, "Automatic time sync");
    sender->send("<label>");
    sender->send("<div class=\"switch\">");
    sender->send("<input type=\"checkbox\" value=\"on\" ");
    sender->send(checked(value == 1));
    sender->sendNameAndId(Supla::AutomaticTimeSyncCfgTag);
    sender->send(" onclick=\"showHideTimeSettingsToggle()\">");
    sender->send("<span class=\"slider\"></span>");
    sender->send("</div>");
    sender->send("</label>");
    sender->send("</div>");
    // form-field BEGIN
    sender->send("<div id=\"time_settings_box\" style=\"display: ");

    if (value == 1) {
      sender->send("none\">");
    } else {
      sender->send("block\">");
    }

    sender->send("<div class=\"form-field right-checkbox\">");
    sender->sendLabelFor("set_time_toggle", "Set time?");
    sender->send("<label>");
    sender->send("<div class=\"switch\">");
    sender->send("<input type=\"checkbox\" value=\"on\" ");
    sender->sendNameAndId("set_time_toggle");
    sender->send(" onclick=\"showHideTimeSettings()\">");
    sender->send("<span class=\"slider\"></span>");
    sender->send("</div>");
    sender->send("</label>");
    sender->send("</div>");

    sender->send(
        "<div class=\"form-field\" id=\"time_settings_inner_box\" "
        "style=\"display: none\">");
    sender->sendLabelFor("date_time_value", "Date and time");
    sender->send(
        "<input type=\"datetime-local\" id=\"date_time_value\" "
        "name=\"date_time_value\">");

    sender->send("</div>");  // time setting inner box end
    sender->send("</div>");  // time setting box end
    sender->send("<script>"
         "function showHideTimeSettingsToggle() {"
            "var checkBox = document.getElementById(\"");
    sender->send(Supla::AutomaticTimeSyncCfgTag);
    sender->send("\");"
            "var text = document.getElementById(\"time_settings_box\");"
            "if (checkBox.checked == true){"
              "text.style.display = \"none\";"
            "} else {"
              "text.style.display = \"block\";"
            "}"
          "}"
         "function showHideTimeSettings() {"
            "var checkBox = document.getElementById(\"");
    sender->send("set_time_toggle");
    sender->send("\");"
            "var text = document.getElementById(\"time_settings_inner_box\");"
            "if (checkBox.checked == true){"
              "text.style.display = \"block\";"
            "} else {"
              "text.style.display = \"none\";"
            "}"
          "}"
        "</script>");
    // form-field END
  }
}

bool TimeParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && strcmp(key, Supla::AutomaticTimeSyncCfgTag) == 0) {
    checkboxFound = true;
    uint8_t currentValue = 1;  // default value
    cfg->getUInt8(Supla::AutomaticTimeSyncCfgTag, &currentValue);

    uint8_t automaticTimeSync = (strcmp(value, "on") == 0 ? 1 : 0);

    if (automaticTimeSync != currentValue) {
      cfg->setUInt8(Supla::AutomaticTimeSyncCfgTag, automaticTimeSync);
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_AUTOMATIC_TIME_SYNC);
    }
    return true;
  }
  if (cfg && strcmp(key, "date_time_value") == 0) {
    auto clock = Supla::Clock::GetInstance();
    if (value && clock) {
      struct tm userTime = {};
      if (strptime(value, "%Y-%m-%dT%H:%M:%S", &userTime) == NULL) {
        if (strptime(value, "%Y-%m-%dT%H:%M", &userTime) == NULL) {
          SUPLA_LOG_DEBUG("Parsing date time failed");
          return true;
        }
      }
      SUPLA_LOG_DEBUG("year =%d, month=%d, day=%d, hour=%d, min=%d, sec=%d",
                      userTime.tm_year, userTime.tm_mon,
                      userTime.tm_mday, userTime.tm_hour, userTime.tm_min,
                      userTime.tm_sec);
      if (userTime.tm_year + 1900 < 2023 || userTime.tm_year + 1900 > 2099) {
        SUPLA_LOG_DEBUG("Invalid year");
        return true;
      }
      if (userTime.tm_mon < 0 || userTime.tm_mon > 11) {
        SUPLA_LOG_DEBUG("Invalid month");
        return true;
      }
      if (userTime.tm_mday < 1 || userTime.tm_mday > 31) {
        SUPLA_LOG_DEBUG("Invalid day");
        return true;
      }
      if (userTime.tm_hour < 0 || userTime.tm_hour > 23) {
        SUPLA_LOG_DEBUG("Invalid hour");
        return true;
      }
      if (userTime.tm_min < 0 || userTime.tm_min > 59) {
        SUPLA_LOG_DEBUG("Invalid min");
        return true;
      }
      if (userTime.tm_sec < 0 || userTime.tm_sec > 59) {
        SUPLA_LOG_DEBUG("Invalid sec");
        return true;
      }

      // reuse proto structure for time setting
      TSDC_UserLocalTimeResult userLocalTime = {};
      userLocalTime.year = userTime.tm_year + 1900;
      userLocalTime.month = userTime.tm_mon + 1;
      userLocalTime.day = userTime.tm_mday;
      userLocalTime.hour = userTime.tm_hour;
      userLocalTime.min = userTime.tm_min;
      userLocalTime.sec = userTime.tm_sec;

      clock->parseLocaltimeFromServer(&userLocalTime);
      return true;
    }
  }
  return false;
}

void TimeParameters::onProcessingEnd() {
  if (!checkboxFound) {
    // checkbox doesn't send value when it is not checked, so on processing end
    // we check if it was found earlier, and if not, then we process it as "off"
    handleResponse(AutomaticTimeSyncCfgTag, "off");
  }
  checkboxFound = false;
}

#endif
