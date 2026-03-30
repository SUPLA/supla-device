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

#include "em_phase_led.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/element.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/storage/config_tags.h>

using Supla::Html::EmPhaseLedParameters;

EmPhaseLedParameters::EmPhaseLedParameters(Supla::Sensor::ElectricityMeter* em)
    : em(em) {
}

EmPhaseLedParameters::~EmPhaseLedParameters() {
}

void EmPhaseLedParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!em || !cfg) {
    return;
  }

  int8_t value = em->getPhaseLedType();
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(
      key, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedTag);

  auto emitFixedField = [&](const char* fieldKey,
                            const char* label,
                            int32_t rawValue) {
    sender->labeledField(fieldKey, label, [&]() {
      sender->tag("div").body([&]() {
        auto input = sender->voidTag("input");
        input.attr("type", "number");
        input.attr("step", "0.1");
        input.attr("name", fieldKey);
        input.attr("id", fieldKey);
        input.attr(
            "value",
            rawValue >= 0 ? (rawValue + 5) / 10 : (rawValue - 5) / 10,
            1);
        input.finish();
      });
    });
  };

  sender->labeledField(key, "Phase indicator LED", [&]() {
    sender->tag("div").body([&]() {
      auto select = sender->selectTag(key, key);
      select.attr("onChange", "emPhaseChange(this.value)");
      select.body([&]() {
        if (em->isPhaseLedTypeSupported(EM_PHASE_LED_TYPE_OFF)) {
          sender->selectOption(0, "OFF", value == 0);
        }
        if (em->isPhaseLedTypeSupported(EM_PHASE_LED_TYPE_VOLTAGE_PRESENCE)) {
          sender->selectOption(1, "Voltage indicator", value == 1);
        }
        if (em->isPhaseLedTypeSupported(
                EM_PHASE_LED_TYPE_VOLTAGE_PRESENCE_INVERTED)) {
          sender->selectOption(2, "No voltage indicator", value == 2);
        }
        if (em->isPhaseLedTypeSupported(EM_PHASE_LED_TYPE_VOLTAGE_LEVEL)) {
          sender->selectOption(3, "Voltage level", value == 3);
        }
        if (em->isPhaseLedTypeSupported(
                EM_PHASE_LED_TYPE_POWER_ACTIVE_DIRECTION)) {
          sender->selectOption(4, "Active power direction", value == 4);
        }
      });
    });
  });

  // js for emPhaseChange
  sender->send(
      "<script>"
      "function emPhaseChange(value) {"
      "  if (value == 3) {"
      "    document.getElementById('em_phase_voltage').style.display = "
      "'block';"
      "  } else {"
      "    document.getElementById('em_phase_voltage').style.display = "
      "'none';"
      "  }"
      "  if (value == 4) {"
      "    document.getElementById('em_phase_power').style.display = 'block';"
      "  } else {"
      "    document.getElementById('em_phase_power').style.display = 'none';"
      "  }"
      "}"
      "</script>");

  sender->toggleBox("em_phase_voltage", value == 3, [&]() {
    Supla::Config::generateKey(
        key, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedVoltageLowTag);
    int32_t voltageLow = em->getLedVoltageLow();
    cfg->getInt32(key, &voltageLow);
    emitFixedField(key, "Low voltage level [V]", voltageLow);

    Supla::Config::generateKey(
        key,
        em->getChannelNumber(),
        Supla::ConfigTag::EmPhaseLedVoltageHighTag);
    int32_t voltageHigh = em->getLedVoltageHigh();
    cfg->getInt32(key, &voltageHigh);
    emitFixedField(key, "High voltage level [V]", voltageHigh);
  });

  sender->toggleBox("em_phase_power", value == 4, [&]() {
    Supla::Config::generateKey(
        key, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedPowerLowTag);
    int32_t powerLow = em->getLedPowerLow();
    cfg->getInt32(key, &powerLow);
    emitFixedField(key, "Low power level [W]", powerLow);

    Supla::Config::generateKey(
        key,
        em->getChannelNumber(),
        Supla::ConfigTag::EmPhaseLedPowerHighTag);
    int32_t powerHigh = em->getLedPowerHigh();
    cfg->getInt32(key, &powerHigh);
    emitFixedField(key, "High power level [W]", powerHigh);
  });
}

bool EmPhaseLedParameters::handleResponse(const char* key, const char* value) {
  if (!em) {
    return false;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  char keyTag[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(
      keyTag, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedTag);
  if (strcmp(keyTag, key) == 0) {
    int ledType = stringToUInt(value);
    int8_t valueInCfg = em->getPhaseLedType();
    cfg->getInt8(keyTag, &valueInCfg);
    if (valueInCfg != ledType) {
      if (ledType > 4) {
        ledType = 1;
      }
      cfg->setInt8(keyTag, ledType);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(keyTag,
                             em->getChannelNumber(),
                             Supla::ConfigTag::EmPhaseLedVoltageLowTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t voltageLow = floatStringToInt(value, 2);
    int32_t valueInCfg = em->getLedVoltageLow();
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != voltageLow) {
      cfg->setInt32(keyTag, voltageLow);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(keyTag,
                             em->getChannelNumber(),
                             Supla::ConfigTag::EmPhaseLedVoltageHighTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t voltageHigh = floatStringToInt(value, 2);
    int32_t valueInCfg = em->getLedVoltageHigh();
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != voltageHigh) {
      cfg->setInt32(keyTag, voltageHigh);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(
      keyTag, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedPowerLowTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t powerLow = floatStringToInt(value, 2);
    int32_t valueInCfg = em->getLedPowerLow();
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != powerLow) {
      cfg->setInt32(keyTag, powerLow);
      channelConfigChanged = true;
    }
    return true;
  }

  Supla::Config::generateKey(
      keyTag, em->getChannelNumber(), Supla::ConfigTag::EmPhaseLedPowerHighTag);
  if (strcmp(keyTag, key) == 0) {
    int32_t powerHigh = floatStringToInt(value, 2);
    int32_t valueInCfg = em->getLedPowerHigh();
    cfg->getInt32(keyTag, &valueInCfg);
    if (valueInCfg != powerHigh) {
      cfg->setInt32(keyTag, powerHigh);
      channelConfigChanged = true;
    }
    return true;
  }

  return false;
}

void EmPhaseLedParameters::onProcessingEnd() {
  if (channelConfigChanged && em) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setChannelConfigChangeFlag(em->getChannelNumber());
    }
    if (em) {
      em->onLoadConfig(nullptr);
    }
  }
  channelConfigChanged = false;
}
