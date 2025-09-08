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

#include "binary_sensor_parameters.h"

#include <stdint.h>
#include <supla/network/html_element.h>
#include <supla/storage/config_tags.h>
#include <supla/sensor/binary_base.h>

#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/clock/clock.h>
#include <string.h>
#include <stdio.h>

using Supla::Html::BinarySensorParameters;

namespace {
constexpr char BinaryTimeoutKey[] = "bs_timeout";
constexpr char BinarySensitivityKey[] = "bs_sens";
constexpr char BinaryFilterKey[] = "bs_filter";
}  // namespace

BinarySensorParameters::BinarySensorParameters(
    Supla::Sensor::BinaryBase* binary)
    : HtmlElement(HTML_SECTION_FORM), binary(binary) {
}

BinarySensorParameters::~BinarySensorParameters() {
}

void BinarySensorParameters::send(Supla::WebSender* sender) {
  if (binary && binary->getChannelNumber() >= 0) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};

    char tmp[100] = {};
    snprintf(tmp, sizeof(tmp), "Binary sensor #%d", binary->getChannelNumber());

    sender->send("</div><div class=\"box\">");
    sender->send("<h3>");
    sender->send(tmp);
    sender->send("</h3>");

    Supla::Config::generateKey(
        key,
        binary->getChannelNumber(),
        Supla::ConfigTag::BinarySensorServerInvertedLogicTag);

    // form-field BEGIN
    sender->send("<div class=\"form-field right-checkbox\">");
    sender->sendLabelFor(key, "Inverted logic");
    sender->send("<label>");
    sender->send("<span class=\"switch\">");
    sender->send("<input type=\"checkbox\" value=\"on\" ");
    sender->send(checked(binary->isServerInvertLogic()));
    sender->sendNameAndId(key);
    sender->send(">");
    sender->send("<span class=\"slider\"></span>");
    sender->send("</span>");
    sender->send("</label>");
    sender->send("</div>");
    // form-field END

    // form-field BEGIN
    if (binary->getFilteringTimeMs() > 0) {
      Supla::Config::generateKey(key,
                                 binary->getChannelNumber(),
                                 BinaryFilterKey);

      sender->send("<div class=\"form-field\">");
      sender->sendLabelFor(key, "Filtering time [s]");
      sender->send(
          "<input type=\"number\" min=\"0.03\" max=\"3.0\" step=\"0.01\" ");
      sender->sendNameAndId(key);
      sender->send(" value=\"");
      sender->send(binary->getFilteringTimeMs(), 3);
      sender->send("\">");
      sender->send("</div>");
    }
    // form-field END

    // form-field BEGIN
    if (binary->getTimeoutDs() > 0) {
      Supla::Config::generateKey(key,
                                 binary->getChannelNumber(),
                                 BinaryTimeoutKey);

      sender->send("<div class=\"form-field\">");
      sender->sendLabelFor(key, "Sensor timeout [s]");
      sender->send(
          "<input type=\"number\" min=\"0.1\" max=\"3600\" step=\"0.1\" ");
      sender->sendNameAndId(key);
      sender->send(" value=\"");
      sender->send(binary->getTimeoutDs(), 1);
      sender->send("\">");
      sender->send("</div>");
      // form-field END
    }

    // form-field BEGIN
    if (binary->getSensitivity() > 0) {
      Supla::Config::generateKey(key,
                                 binary->getChannelNumber(),
                                 BinarySensitivityKey);

      sender->send("<div class=\"form-field\">");
      sender->sendLabelFor(key, "Sensor sensitivity [%]");
      sender->send(
          "<input type=\"number\" min=\"0\" max=\"100\" step=\"1\" ");
      sender->sendNameAndId(key);
      sender->send(" value=\"");
      sender->send(binary->getSensitivity() - 1, 0);
      sender->send("\">");
      sender->send("</div>");
      // form-field END
    }
  }
}

bool BinarySensorParameters::handleResponse(const char* key,
                                            const char* value) {
  if (binary == nullptr) {
    return false;
  }
  char expectedKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(
      expectedKey,
      binary->getChannelNumber(),
      Supla::ConfigTag::BinarySensorServerInvertedLogicTag);

  if (strcmp(key, expectedKey) == 0) {
    checkboxFound = true;
    if (binary->setServerInvertLogic(true, true)) {
      configChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(
      expectedKey,
      binary->getChannelNumber(),
      BinaryTimeoutKey);
  if (strcmp(key, expectedKey) == 0) {
    uint32_t param = floatStringToInt(value, 1);
    if (binary->getTimeoutDs() > 0 && param < 36000) {
      if (binary->setTimeoutDs(param)) {
        configChanged = true;
      }
    }
    return true;
  }

  Supla::Config::generateKey(
      expectedKey,
      binary->getChannelNumber(),
      BinarySensitivityKey);
  if (strcmp(key, expectedKey) == 0) {
    uint32_t param = stringToInt(value);
    if (binary->getSensitivity() > 0 && param <= 100) {
      if (binary->setSensitivity(param + 1)) {
        configChanged = true;
      }
    }
    return true;
  }

  Supla::Config::generateKey(
      expectedKey,
      binary->getChannelNumber(),
      BinaryFilterKey);
  if (strcmp(key, expectedKey) == 0) {
    uint32_t param = floatStringToInt(value, 3);
    if (binary->getFilteringTimeMs() > 0 && param < 10000) {
      if (binary->setFilteringTimeMs(param)) {
        configChanged = true;
      }
    }
    return true;
  }

  return false;
}

void BinarySensorParameters::onProcessingEnd() {
  if (!checkboxFound) {
    if (binary->setServerInvertLogic(false, true)) {
      configChanged = true;
    }
  }
  if (configChanged) {
    Supla::Storage::ConfigInstance()->saveWithDelay(1000);
  }
  checkboxFound = false;
  configChanged = false;
}

