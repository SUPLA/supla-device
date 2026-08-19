// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
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
constexpr char BinaryLocalAlarmIndicationKey[] = "bs_local_alarm";
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
    sender->tag("h3").body(tmp);

    Supla::Config::generateKey(
        key,
        binary->getChannelNumber(),
        Supla::ConfigTag::BinarySensorServerInvertedLogicTag);

    sender->labeledField(
        key,
        "Inverted logic",
        [&]() {
          sender->tag("label").body([&]() {
            sender->tag("span").attr("class", "switch").body([&]() {
              auto input = sender->voidTag("input");
              input.attr("type", "checkbox");
              input.attr("value", "on");
              input.attrIf("checked", binary->isServerInvertLogic());
              input.attr("name", key);
              input.attr("id", key);
              input.finish();
              sender->tag("span").attr("class", "slider").body([]() {});
            });
          });
        },
        "form-field right-checkbox");

    if (binary->getFilteringTimeMs() > 0) {
      Supla::Config::generateKey(key,
                                 binary->getChannelNumber(),
                                 BinaryFilterKey);

      sender->labeledField(
          key,
          "Filtering time [s]",
          [&]() {
            auto input = sender->voidTag("input");
            input.attr("type", "number");
            input.attr("min", "0.03");
            input.attr("max", "3.0");
            input.attr("step", "0.01");
            input.attr("name", key);
            input.attr("id", key);
            input.attr("value", binary->getFilteringTimeMs(), 3);
            input.finish();
          });
    }

    if (binary->getTimeoutDs() > 0) {
      Supla::Config::generateKey(key,
                                 binary->getChannelNumber(),
                                 BinaryTimeoutKey);

      sender->labeledField(
          key,
          "Sensor timeout [s]",
          [&]() {
            auto input = sender->voidTag("input");
            input.attr("type", "number");
            input.attr("min", "0.1");
            input.attr("max", "3600");
            input.attr("step", "0.1");
            input.attr("name", key);
            input.attr("id", key);
            input.attr("value", binary->getTimeoutDs(), 1);
            input.finish();
          });
    }

    if (binary->getSensitivity() > 0) {
      Supla::Config::generateKey(key,
                                 binary->getChannelNumber(),
                                 BinarySensitivityKey);

      sender->labeledField(
          key,
          "Sensor sensitivity [%]",
          [&]() {
            auto input = sender->voidTag("input");
            input.attr("type", "number");
            input.attr("min", 0);
            input.attr("max", 100);
            input.attr("step", 1);
            input.attr("name", key);
            input.attr("id", key);
            input.attr("value", binary->getSensitivity() - 1, 0);
            input.finish();
          });
    }

    if (binary->getLocalAlarmIndication() > 0) {
      Supla::Config::generateKey(key,
                                 binary->getChannelNumber(),
                                 BinaryLocalAlarmIndicationKey);

      sender->labeledField(
          key,
          "Local alarm indication",
          [&]() {
            sender->selectInput(key, key, [&]() {
              sender->selectOption(
                  1, "Disabled", binary->getLocalAlarmIndication() == 1);
              sender->selectOption(
                  2, "Enabled", binary->getLocalAlarmIndication() == 2);
            });
          });
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

  Supla::Config::generateKey(
      expectedKey,
      binary->getChannelNumber(),
      BinaryLocalAlarmIndicationKey);
  if (strcmp(key, expectedKey) == 0) {
    uint32_t param = stringToInt(value);
    if (binary->getLocalAlarmIndication() > 0 && param >= 1 && param <= 2) {
      if (binary->setLocalAlarmIndication(static_cast<uint8_t>(param))) {
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

#endif  // ARDUINO_ARCH_AVR
