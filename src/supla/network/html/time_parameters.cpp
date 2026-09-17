// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

namespace {

bool parseDigits(const char *value,
                 size_t offset,
                 size_t count,
                 int *result) {
  int parsed = 0;
  for (size_t i = 0; i < count; i++) {
    const char digit = value[offset + i];
    if (digit < '0' || digit > '9') {
      return false;
    }
    parsed = parsed * 10 + (digit - '0');
  }
  *result = parsed;
  return true;
}

bool parseDateTimeLocal(const char *value, struct tm *result) {
  if (value == nullptr || result == nullptr) {
    return false;
  }

  const size_t length = strlen(value);
  const bool hasSeconds = length == 19;
  if ((!hasSeconds && length != 16) || value[4] != '-' ||
      value[7] != '-' || value[10] != 'T' || value[13] != ':' ||
      (hasSeconds && value[16] != ':')) {
    return false;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  if (!parseDigits(value, 0, 4, &year) ||
      !parseDigits(value, 5, 2, &month) ||
      !parseDigits(value, 8, 2, &day) ||
      !parseDigits(value, 11, 2, &hour) ||
      !parseDigits(value, 14, 2, &minute) ||
      (hasSeconds && !parseDigits(value, 17, 2, &second))) {
    return false;
  }

  result->tm_year = year - 1900;
  result->tm_mon = month - 1;
  result->tm_mday = day;
  result->tm_hour = hour;
  result->tm_min = minute;
  result->tm_sec = hasSeconds ? second : 0;
  return true;
}

}  // namespace

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

    sender->formField([&]() {
      sender->labelFor(Supla::AutomaticTimeSyncCfgTag, "Automatic time sync");
      auto label = sender->tag("label");
      label.body([&]() {
        auto switchSpan = sender->tag("span");
        switchSpan.attr("class", "switch").body([&]() {
          auto input = sender->voidTag("input");
          input.attr("type", "checkbox")
              .attr("value", "on")
              .attrIf("checked", value == 1)
              .attr("name", Supla::AutomaticTimeSyncCfgTag)
              .attr("id", Supla::AutomaticTimeSyncCfgTag)
              .attr("onclick", "showHideTimeSettingsToggle()")
              .finish();
          sender->tag("span").attr("class", "slider").body("");
        });
      });
    }, "form-field right-checkbox");

    sender->toggleBox("time_settings_box", value != 1, [&]() {
      sender->formField([&]() {
        sender->labelFor("set_time_toggle", "Set time?");
        auto label = sender->tag("label");
        label.body([&]() {
          auto switchSpan = sender->tag("span");
          switchSpan.attr("class", "switch").body([&]() {
            auto input = sender->voidTag("input");
            input.attr("type", "checkbox")
                .attr("value", "on")
                .attr("name", "set_time_toggle")
                .attr("id", "set_time_toggle")
                .attr("onclick", "showHideTimeSettings()")
                .finish();

            sender->tag("span").attr("class", "slider").body("");
          });
        });
      }, "form-field right-checkbox");

      sender->toggleBox("time_settings_inner_box", false, [&]() {
        sender->formField([&]() {
          sender->labelFor("date_time_value", "Date and time");
          auto input = sender->voidTag("input");
          input.attr("type", "datetime-local")
              .attr("id", "date_time_value")
              .attr("name", "date_time_value")
              .finish();
        });
      }, "form-field");
    });
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
      if (!parseDateTimeLocal(value, &userTime)) {
        SUPLA_LOG_DEBUG("Parsing date time failed");
        return true;
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
