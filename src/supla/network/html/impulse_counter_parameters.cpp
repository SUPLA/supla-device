// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "impulse_counter_parameters.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <supla/actions.h>
#include <supla/channels/channel.h>
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/network/web_sender.h>
#include <supla/sensor/virtual_impulse_counter.h>
#include <supla/storage/config.h>

using Supla::Html::ImpulseCounterParameters;

namespace {

constexpr struct {
  uint32_t function;
  const char *name;
} measurementTypes[] = {
    {SUPLA_CHANNELFNC_IC_EVENTS, "Generic impulse counter"},
    {SUPLA_CHANNELFNC_IC_ELECTRICITY_METER, "Energy"},
    {SUPLA_CHANNELFNC_IC_GAS_METER, "Gas"},
    {SUPLA_CHANNELFNC_IC_WATER_METER, "Water"},
    {SUPLA_CHANNELFNC_IC_HEAT_METER, "Heat"},
    {SUPLA_CHANNELFNC_IC_SECONDS, "Seconds"},
};

}  // namespace

ImpulseCounterParameters::ImpulseCounterParameters(
    Sensor::VirtualImpulseCounter *counter, const char *label)
    : counter(counter), label(label) {
  if (counter) {
    registerCmd("RESET", Supla::ON_EVENT_1);
    addAction(Supla::RESET, *counter, Supla::ON_EVENT_1, true);
  }
}

void ImpulseCounterParameters::send(WebSender *sender) {
  if (!sender || !counter || counter->getChannelNumber() < 0) {
    return;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  char fieldLabel[64] = {};
  const char *prefix = label ? label : "";
  const char *separator = label ? ": " : "";
  counter->generateKey(key, "ic_func");
  snprintf(fieldLabel, sizeof(fieldLabel), "%s%sMeasurement type",
           prefix, separator);
  const auto currentFunction = counter->getChannel()->getDefaultFunction();
  sender->labeledField(key, fieldLabel, [&]() {
    auto select = sender->selectTag(key, key);
    select.body([&]() {
      if (currentFunction == 0) {
        sender->selectOption(0, "Disabled", true);
      }
      for (const auto &type : measurementTypes) {
        sender->selectOption(type.function, type.name,
                             currentFunction == type.function);
      }
    });
  });

  counter->generateKey(key, "ic_rate");
  snprintf(fieldLabel, sizeof(fieldLabel), "%s%sImpulses per unit",
           prefix, separator);
  sender->labeledField(key, fieldLabel, [&]() {
    sender->numberInput(
        key,
        {.min = 0,
         .max = Sensor::VirtualImpulseCounter::MaxImpulsesPerUnit,
         .value = counter->getImpulsesPerUnit(),
         .step = 1});
  });

  counter->generateKey(key, "ic_unit");
  snprintf(fieldLabel, sizeof(fieldLabel), "%s%sCustom unit",
           prefix, separator);
  sender->labeledField(key, fieldLabel, [&]() {
    sender->textInput(key, key, counter->getCustomUnit(),
                      sizeof(TChannelConfig_ImpulseCounter::CustomUnit) - 1);
  });

  counter->generateKey(key, "ic_reset");
  snprintf(fieldLabel, sizeof(fieldLabel),
           "%s%sType \"RESET\" to reset counter to 0", prefix, separator);
  sender->labeledField(key, fieldLabel, [&]() {
    sender->textInput(key, key);
  });
}

bool ImpulseCounterParameters::handleResponse(const char *key,
                                               const char *value) {
  if (!key || !value || !counter || counter->getChannelNumber() < 0) {
    return false;
  }

  char expectedKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  counter->generateKey(expectedKey, "ic_func");
  if (strcmp(key, expectedKey) == 0) {
    char *end = nullptr;
    errno = 0;
    const auto function = strtol(value, &end, 10);
    if (!errno && end != value && *end == '\0' &&
        function >= 0 && function <= UINT16_MAX) {
      for (const auto &type : measurementTypes) {
        if (static_cast<uint16_t>(function) == type.function) {
          counter->setLocalFunction(type.function);
          break;
        }
      }
    }
    return true;
  }

  counter->generateKey(expectedKey, "ic_unit");
  if (strcmp(key, expectedKey) == 0) {
    if (!counter->setCustomUnit(value)) {
      SUPLA_LOG_WARNING("ImpulseCounter[%d]: invalid or unsaved unit",
                        counter->getChannelNumber());
    }
    return true;
  }

  counter->generateKey(expectedKey, "ic_rate");
  if (strcmp(key, expectedKey) == 0) {
    char *end = nullptr;
    errno = 0;
    const auto rate = strtol(value, &end, 10);
    if (errno || end == value || *end || rate < 0 ||
        rate > Sensor::VirtualImpulseCounter::MaxImpulsesPerUnit ||
        !counter->setImpulsesPerUnit(rate)) {
      SUPLA_LOG_WARNING("ImpulseCounter[%d]: invalid or unsaved rate",
                        counter->getChannelNumber());
    }
    return true;
  }

  counter->generateKey(expectedKey, "ic_reset");
  if (strcmp(key, expectedKey) != 0) {
    return false;
  }
  handleCommand(value);
  return true;
}

#endif  // ARDUINO_ARCH_AVR
