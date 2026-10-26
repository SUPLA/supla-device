// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "hvac_parameters.h"

#include <stdio.h>
#include <string.h>
#include <supla/network/html/channel_reference_select.h>
#include <supla/channels/channel.h>
#include <supla/control/hvac_base.h>
#include <supla/log_wrapper.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

using Supla::Html::HvacParameters;

namespace {

template <typename Fn>
void emitSelectField(Supla::WebSender* sender,
                     const char* key,
                     const char* label,
                     Fn&& fill,
                     bool disabled = false,
                     bool hidden = false) {
  sender->labeledField(key, label, [&]() {
    auto select = sender->selectTag(key, key);
    select.attrIf("disabled", disabled);
    select.attrIf("hidden", hidden);
    select.body(fill);
  });
}

void emitSwitchField(Supla::WebSender* sender,
                     const char* key,
                     const char* label,
                     bool checked,
                     bool readonly,
                     const char* onchange) {
  sender->formField(
      [&]() {
        sender->labelFor(key, label);
        auto wrapper = sender->tag("label");
        wrapper.body([&]() {
          auto switchTag = sender->tag("span");
          switchTag.attr("class", "switch");
          switchTag.body([&]() {
            auto input = sender->voidTag("input");
            input.attr("type", "checkbox");
            input.attr("value", "on");
            input.attrIf("checked", checked);
            input.attrIf("readonly", readonly);
            input.attr("name", key);
            input.attr("id", key);
            if (onchange != nullptr) {
              input.attr("onchange", onchange);
            }
            input.finish();
            sender->tag("span").attr("class", "slider").body("");
          });
        });
      },
      "form-field right-checkbox");
}

template <typename ValueGetter>
void emitNumberField(Supla::WebSender* sender,
                     const char* key,
                     const char* label,
                     ValueGetter&& valueGetter,
                     bool readonly = false) {
  sender->labeledField(key, label, [&]() {
    auto input = sender->voidTag("input");
    input.attr("type", "number");
    input.attr("step", "0.1");
    input.attrIf("readonly", readonly);
    input.attr("name", key);
    input.attr("id", key);
    auto value = valueGetter();
    if (value != INT16_MIN) {
      char buf[32] = {};
      snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(value) / 100.0);
      input.attr("value", buf);
    }
    input.finish();
  });
}

template <typename ValueGetter>
void emitIntField(Supla::WebSender* sender,
                  const char* key,
                  const char* label,
                  ValueGetter&& valueGetter,
                  bool readonly = false) {
  sender->labeledField(key, label, [&]() {
    auto input = sender->voidTag("input");
    input.attr("type", "number");
    input.attrIf("readonly", readonly);
    input.attr("name", key);
    input.attr("id", key);
    input.attr("value", valueGetter());
    input.finish();
  });
}

}  // namespace

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

  using Field = Supla::Control::HvacBase::ReferenceField;
  const auto remote = [&](Field field) {
    const auto ref = hvac->channelReference(field);
    return ref.kind == ChannelReferenceKind::SERVER_CHANNEL &&
           localReferenceNumber(ref) < 0;
  };
  char key[16] = {};
  int32_t channelFunc = hvac->getChannel()->getDefaultFunction();

  char tmp[100] = {};
  snprintf(tmp, sizeof(tmp), "Thermostat #%d", hvac->getChannelNumber());
  char auxSettingsKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  hvac->generateKey(auxSettingsKey, "aux_box");
  char antiFreezeSettingsKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  hvac->generateKey(antiFreezeSettingsKey, "af_box");

  sender->send("</div><div class=\"box\">");
  sender->tag("h3").body(tmp);

  hvac->generateKey(key, "fnc");
  emitSelectField(sender, key, "Channel function", [&]() {
    if (hvac->isFunctionSupported(SUPLA_CHANNELFNC_HVAC_THERMOSTAT)) {
      sender->selectOption(SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
                           "Room thermostat",
                           channelFunc == SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
    }
    if (hvac->isFunctionSupported(SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER,
          "Domestic hot water",
          channelFunc == SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER);
    }
    if (hvac->isFunctionSupported(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL,
          "Heat + cool",
          channelFunc == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
    }
    if (hvac->isFunctionSupported(
            SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL)) {
      sender->selectOption(
          SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL,
          "Differential",
          channelFunc == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);
    }
  });

  if (hvac->getChannelFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
      !hvac->parameterFlags.SubfunctionHidden) {
    hvac->generateKey(key, "subfnc");
    emitSelectField(
        sender,
        key,
        "Room thermostat function",
        [&]() {
          sender->selectOption(SUPLA_HVAC_SUBFUNCTION_HEAT,
                               "Heat",
                               hvac->isHeatingSubfunction());
          sender->selectOption(SUPLA_HVAC_SUBFUNCTION_COOL,
                               "Cool",
                               hvac->isCoolingSubfunction());
        },
        hvac->parameterFlags.SubfunctionReadonly);
  }

  int hvacCount = 0;
  int pumpCount = 0;
  int hocsCount = 0;
  for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
       ch = ch->next()) {
    auto channelType = ch->getChannelType();
    if (channelType == SUPLA_CHANNELTYPE_HVAC) {
      hvacCount++;
    } else if (channelType == SUPLA_CHANNELTYPE_RELAY) {
      auto relayFunction = ch->getDefaultFunction();
      if (relayFunction == SUPLA_CHANNELFNC_PUMPSWITCH) {
        pumpCount++;
      } else if (relayFunction == SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH) {
        hocsCount++;
      }
    }
  }

  if ((hvacCount > 1 || remote(Field::MasterThermostat)) &&
      !hvac->parameterFlags.MasterThermostatChannelNoHidden) {
    hvac->generateKey(key, "master");
    emitSelectField(
        sender,
        key,
        "Master thermostat",
        [&]() {
          remoteReferenceOption(sender, hvac->channelReference(
              Field::MasterThermostat));
          sender->selectOption(
              -1, "Not set",
              hvac->channelReference(Field::MasterThermostat).kind ==
                  ChannelReferenceKind::NONE);
          for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
               ch = ch->next()) {
            int channelNumber = ch->getChannelNumber();
            auto channelType = ch->getChannelType();
            if (channelType == SUPLA_CHANNELTYPE_HVAC &&
                channelNumber != hvac->getChannelNumber()) {
              auto el =
                  Supla::Element::getElementByChannelNumber(channelNumber);
              auto otherHvac = el ? el->getHvacBase() : nullptr;
              if (otherHvac && !otherHvac->isMasterThermostatSet()) {
                char buf[100] = {};
                snprintf(buf, sizeof(buf), "Thermostat #%d", channelNumber);
                sender->selectOption(
                    channelNumber,
                    buf,
                    hvac->getMasterThermostatChannelNo() == channelNumber);
              }
            }
          }
        },
        hvac->parameterFlags.MasterThermostatChannelNoReadonly);
  }

  auto hvacMode = hvac->getChannel()->getHvacMode();
  hvac->generateKey(key, "hvac_mode");
  emitSelectField(sender, key, "Mode", [&]() {
    if (hvac->isModeSupported(SUPLA_HVAC_MODE_OFF)) {
      sender->selectOption(SUPLA_HVAC_MODE_OFF,
                           "Off",
                           hvacMode == SUPLA_HVAC_MODE_OFF &&
                               !hvac->getChannel()->isHvacFlagWeeklySchedule());
    }
    if (hvac->isModeSupported(SUPLA_HVAC_MODE_HEAT)) {
      sender->selectOption(SUPLA_HVAC_MODE_HEAT,
                           "Heat",
                           hvacMode == SUPLA_HVAC_MODE_HEAT &&
                               !hvac->getChannel()->isHvacFlagWeeklySchedule());
    }
    if (hvac->isModeSupported(SUPLA_HVAC_MODE_COOL)) {
      sender->selectOption(
          SUPLA_HVAC_MODE_COOL, "Cool", hvacMode == SUPLA_HVAC_MODE_COOL);
    }
    if (hvac->isModeSupported(SUPLA_HVAC_MODE_HEAT_COOL)) {
      sender->selectOption(SUPLA_HVAC_MODE_HEAT_COOL,
                           "Heat + cool",
                           hvacMode == SUPLA_HVAC_MODE_HEAT_COOL &&
                               !hvac->getChannel()->isHvacFlagWeeklySchedule());
    }
    if (hvac->isModeSupported(SUPLA_HVAC_MODE_CMD_TURN_ON)) {
      sender->selectOption(SUPLA_HVAC_MODE_CMD_TURN_ON, "Turn on", false);
    }
    if (hvac->isModeSupported(SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE)) {
      sender->selectOption(SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE,
                           "Weekly schedule",
                           hvac->getChannel()->isHvacFlagWeeklySchedule());
    }
  });

  if (hvac->isModeSupported(SUPLA_HVAC_MODE_HEAT)) {
    hvac->generateKey(key, "t_min");
    emitNumberField(sender, key, "Heating temperature setpoint [°C]", [&]() {
      return hvac->getTemperatureSetpointHeat();
    });
  }

  if (hvac->isModeSupported(SUPLA_HVAC_MODE_COOL)) {
    hvac->generateKey(key, "t_max");
    emitNumberField(sender, key, "Cooling temperature setpoint [°C]", [&]() {
      return hvac->getTemperatureSetpointCool();
    });
  }

  sender->tag("h2").body("Thermometers configuration");

  if (!hvac->parameterFlags.MainThermometerChannelNoHidden) {
    hvac->generateKey(key, "t_main");
    emitSelectField(
        sender,
        key,
        "Main thermometer channel number",
        [&]() {
          remoteReferenceOption(sender, hvac->channelReference(
              Field::MainThermometer));
          sender->selectOption(
              hvac->getChannelNumber(),
              "Not set",
              hvac->channelReference(Field::MainThermometer).kind ==
                  ChannelReferenceKind::NONE);
          for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
               ch = ch->next()) {
            int channelNumber = ch->getChannelNumber();
            auto channelType = ch->getChannelType();
            if (channelType == SUPLA_CHANNELTYPE_THERMOMETER ||
                channelType == SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR) {
              char buf[100] = {};
              snprintf(buf, sizeof(buf), "Thermometer #%d", channelNumber);
              sender->selectOption(
                  channelNumber,
                  buf,
                  hvac->getMainThermometerChannelNo() == channelNumber);
            }
          }
        },
        hvac->parameterFlags.MainThermometerChannelNoReadonly,
        hvac->parameterFlags.MainThermometerChannelNoHidden);
  }

  if (!hvac->parameterFlags.AuxThermometerChannelNoHidden) {
    hvac->generateKey(key, "t_aux");
    emitSelectField(
        sender,
        key,
        "Auxiliary thermometer channel number",
        [&]() {
          remoteReferenceOption(sender, hvac->channelReference(
              Field::AuxThermometer));
          sender->selectOption(
              hvac->getChannelNumber(),
              "Not set",
              hvac->channelReference(Field::AuxThermometer).kind ==
                  ChannelReferenceKind::NONE);
          for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
               ch = ch->next()) {
            int channelNumber = ch->getChannelNumber();
            auto channelType = ch->getChannelType();
            if (channelType == SUPLA_CHANNELTYPE_THERMOMETER ||
                channelType == SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR) {
              char buf[100] = {};
              snprintf(buf, sizeof(buf), "Thermometer #%d", channelNumber);
              sender->selectOption(
                  channelNumber,
                  buf,
                  hvac->getAuxThermometerChannelNo() == channelNumber);
            }
          }
        },
        hvac->parameterFlags.AuxThermometerChannelNoReadonly,
        hvac->parameterFlags.AuxThermometerChannelNoHidden);
  }

  if (!hvac->parameterFlags.AuxThermometerTypeHidden) {
    hvac->generateKey(key, "t_aux_type");
    auto tSecType = hvac->getAuxThermometerType();
    emitSelectField(
        sender,
        key,
        "Auxiliary thermometer type",
        [&]() {
          sender->selectOption(
              SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET,
              "Not set",
              tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET);
          sender->selectOption(
              SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED,
              "Disabled",
              tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED);
          sender->selectOption(
              SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR,
              "Floor",
              tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR);
          sender->selectOption(
              SUPLA_HVAC_AUX_THERMOMETER_TYPE_WATER,
              "Water",
              tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_WATER);
          sender->selectOption(
              SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_COOLER,
              "Generic cooler",
              tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_COOLER);
          sender->selectOption(
              SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER,
              "Generic heater",
              tSecType == SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER);
        },
        hvac->parameterFlags.AuxThermometerTypeReadonly,
        hvac->parameterFlags.AuxThermometerTypeHidden);
  }

  if (!hvac->parameterFlags.AuxMinMaxSetpointEnabledHidden) {
    hvac->generateKey(key, "aux_ctrl");
    char auxChangeFn[48] = {};
    char auxOnChange[52] = {};
    snprintf(auxChangeFn,
             sizeof(auxChangeFn),
             "auxSetpointEnabledChange_%d",
             hvac->getChannelNumber());
    snprintf(auxOnChange, sizeof(auxOnChange), "%s()", auxChangeFn);
    emitSwitchField(sender,
                    key,
                    "Enable auxiliary min. and max. setpoints",
                    hvac->isAuxMinMaxSetpointEnabled(),
                    hvac->parameterFlags.AuxMinMaxSetpointEnabledReadonly,
                    auxOnChange);

    sender->send(
        "<script>"
        "function ");
    sender->send(auxChangeFn);
    sender->send(
        "(){"
        "var e=document.getElementById(\"");
    sender->send(key);
    sender->send(
        "\"),"
        "c=document.getElementById(\"");
    sender->send(auxSettingsKey);
    sender->send(
        "\"),"
        "l=e.checked?\"block\":\"none\";"
        "c.style.display=l;}"
        "</script>");
  }

  if (!hvac->parameterFlags.TemperaturesAuxMinSetpointHidden ||
      !hvac->parameterFlags.TemperaturesAuxMaxSetpointHidden) {
    sender->toggleBox(
        auxSettingsKey, hvac->isAuxMinMaxSetpointEnabled(), [&]() {
          if (!hvac->parameterFlags.TemperaturesAuxMinSetpointHidden) {
            hvac->generateKey(key, "t_aux_min");
            emitNumberField(
                sender,
                key,
                "Aux min. temperature setpoint [°C]",
                [&]() { return hvac->getTemperatureAuxMinSetpoint(); },
                hvac->parameterFlags.TemperaturesAuxMinSetpointReadonly);
          }

          if (!hvac->parameterFlags.TemperaturesAuxMaxSetpointHidden) {
            hvac->generateKey(key, "t_aux_max");
            emitNumberField(
                sender,
                key,
                "Aux max. temperature setpoint [°C]",
                [&]() { return hvac->getTemperatureAuxMaxSetpoint(); },
                hvac->parameterFlags.TemperaturesAuxMaxSetpointReadonly);
          }
        });
  }

  if (!hvac->parameterFlags.AntiFreezeAndOverheatProtectionEnabledHidden) {
    sender->tag("h2").body("Anti freeze and overheat protection");
    hvac->generateKey(key, "anti_freeze");
    char antiFreezeChangeFn[48] = {};
    char antiFreezeOnChange[52] = {};
    snprintf(antiFreezeChangeFn,
             sizeof(antiFreezeChangeFn),
             "antiFreezeAndHeatProtectionChange_%d",
             hvac->getChannelNumber());
    snprintf(antiFreezeOnChange,
             sizeof(antiFreezeOnChange),
             "%s()",
             antiFreezeChangeFn);
    emitSwitchField(
        sender,
        key,
        "Enable anti-freeze and overheat protection",
        hvac->isAntiFreezeAndHeatProtectionEnabled(),
        hvac->parameterFlags.AntiFreezeAndOverheatProtectionEnabledReadonly,
        antiFreezeOnChange);

    sender->send(
        "<script>"
        "function ");
    sender->send(antiFreezeChangeFn);
    sender->send(
        "(){"
        "var e=document.getElementById(\"");
    sender->send(key);
    sender->send(
        "\"),"
        "c=document.getElementById(\"");
    sender->send(antiFreezeSettingsKey);
    sender->send(
        "\"),"
        "l=e.checked?\"block\":\"none\";"
        "c.style.display=l;}"
        "</script>");
  }

  if (!hvac->parameterFlags.TemperaturesFreezeProtectionHidden ||
      !hvac->parameterFlags.TemperaturesHeatProtectionHidden) {
    sender->toggleBox(
        antiFreezeSettingsKey,
        hvac->isAntiFreezeAndHeatProtectionEnabled(),
        [&]() {
          if (!hvac->parameterFlags.TemperaturesFreezeProtectionHidden) {
            hvac->generateKey(key, "t_freeze");
            emitNumberField(
                sender,
                key,
                "Freeze protection [°C]",
                [&]() { return hvac->getTemperatureFreezeProtection(); },
                hvac->parameterFlags.TemperaturesFreezeProtectionReadonly);
          }

          if (!hvac->parameterFlags.TemperaturesHeatProtectionHidden) {
            hvac->generateKey(key, "t_heat");
            emitNumberField(
                sender,
                key,
                "Overheat protection [°C]",
                [&]() { return hvac->getTemperatureHeatProtection(); },
                hvac->parameterFlags.TemperaturesHeatProtectionReadonly);
          }
        });
  }

  sender->tag("h2").body("Behavior settings");

  int countSensors = 0;
  int sensorId = 0;
  for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
       ch = ch->next()) {
    auto channelType = ch->getChannelType();
    if (channelType == SUPLA_CHANNELTYPE_BINARYSENSOR) {
      countSensors++;
      sensorId = ch->getChannelNumber();
    }
  }
  if ((countSensors > 0 || remote(Field::BinarySensor)) &&
      !hvac->parameterFlags.BinarySensorChannelNoHidden) {
    hvac->generateKey(key, "sensor");
    emitSelectField(
        sender,
        key,
        "Turn off based on sensor state",
        [&]() {
          remoteReferenceOption(sender, hvac->channelReference(
              Field::BinarySensor));
          sender->selectOption(
              hvac->getChannelNumber(),
              "Disabled",
              hvac->channelReference(Field::BinarySensor).kind ==
                  ChannelReferenceKind::NONE);
          if (countSensors == 1) {
            sender->selectOption(sensorId,
                                 "Enabled",
                                 hvac->getBinarySensorChannelNo() == sensorId);
          } else {
            for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
                 ch = ch->next()) {
              auto channelType = ch->getChannelType();
              if (channelType == SUPLA_CHANNELTYPE_BINARYSENSOR) {
                char buf[100] = {};
                auto channelNumber = ch->getChannelNumber();
                snprintf(buf, sizeof(buf), "Use sensor #%d", channelNumber);
                sender->selectOption(
                    channelNumber,
                    buf,
                    hvac->getBinarySensorChannelNo() == channelNumber);
              }
            }
          }
        },
        hvac->parameterFlags.BinarySensorChannelNoReadonly);
  }

  if ((pumpCount > 0 || remote(Field::PumpSwitch)) &&
      !hvac->parameterFlags.PumpSwitchHidden) {
    hvac->generateKey(key, "pump");
    emitSelectField(
        sender,
        key,
        "Pump switch",
        [&]() {
          remoteReferenceOption(sender, hvac->channelReference(
              Field::PumpSwitch));
          sender->selectOption(
              -1, "Not set",
              hvac->channelReference(Field::PumpSwitch).kind ==
                  ChannelReferenceKind::NONE);
          for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
               ch = ch->next()) {
            int channelNumber = ch->getChannelNumber();
            auto channelType = ch->getChannelType();
            if (channelType == SUPLA_CHANNELTYPE_RELAY) {
              auto relayFunction = ch->getDefaultFunction();
              if (relayFunction == SUPLA_CHANNELFNC_PUMPSWITCH) {
                char buf[100] = {};
                snprintf(buf, sizeof(buf), "Pump switch #%d", channelNumber);
                sender->selectOption(
                    channelNumber,
                    buf,
                    hvac->getPumpSwitchChannelNo() == channelNumber);
              }
            }
          }
        },
        hvac->parameterFlags.PumpSwitchReadonly);
  }

  if ((hocsCount > 0 || remote(Field::HeatOrColdSourceSwitch)) &&
      !hvac->parameterFlags.HeatOrColdSourceSwitchHidden) {
    hvac->generateKey(key, "hocs");
    emitSelectField(
        sender,
        key,
        "Heat or cold source switch",
        [&]() {
          remoteReferenceOption(sender, hvac->channelReference(
              Field::HeatOrColdSourceSwitch));
          sender->selectOption(
              -1, "Not set",
              hvac->channelReference(Field::HeatOrColdSourceSwitch).kind ==
                  ChannelReferenceKind::NONE);
          for (Supla::Channel* ch = Supla::Channel::Begin(); ch != nullptr;
               ch = ch->next()) {
            int channelNumber = ch->getChannelNumber();
            auto channelType = ch->getChannelType();
            if (channelType == SUPLA_CHANNELTYPE_RELAY) {
              auto relayFunction = ch->getDefaultFunction();
              if (relayFunction == SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH) {
                char buf[100] = {};
                snprintf(buf,
                         sizeof(buf),
                         "Heat or cold source switch #%d",
                         channelNumber);
                sender->selectOption(
                    channelNumber,
                    buf,
                    hvac->getHeatOrColdSourceSwitchChannelNo() ==
                        channelNumber);
              }
            }
          }
        },
        hvac->parameterFlags.HeatOrColdSourceSwitchReadonly);
  }

  if (!hvac->parameterFlags.UsedAlgorithmHidden) {
    hvac->generateKey(key, "algorithm");
    emitSelectField(
        sender,
        key,
        "Algorithm",
        [&]() {
          if (hvac->isAlgorithmValid(
                  SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE)) {
            sender->selectOption(
                SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE,
                "On/Off middle",
                hvac->getUsedAlgorithm() ==
                    SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
          }
          if (hvac->isAlgorithmValid(
                  SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST)) {
            sender->selectOption(
                SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST,
                "On/Off at most",
                hvac->getUsedAlgorithm() ==
                    SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);
          }
          if (hvac->isAlgorithmValid(SUPLA_HVAC_ALGORITHM_PID)) {
            sender->selectOption(
                SUPLA_HVAC_ALGORITHM_PID,
                "PID",
                hvac->getUsedAlgorithm() == SUPLA_HVAC_ALGORITHM_PID);
          }
        },
        hvac->parameterFlags.UsedAlgorithmReadonly);
  }

  if (!hvac->parameterFlags.TemperaturesHisteresisHidden) {
    hvac->generateKey(key, "t_hister");
    emitNumberField(
        sender,
        key,
        "Histeresis [°C]",
        [&]() { return hvac->getTemperatureHisteresis(); },
        hvac->parameterFlags.TemperaturesHisteresisReadonly);
  }

  if (!hvac->parameterFlags.MinOnTimeSHidden) {
    hvac->generateKey(key, "min_on_s");
    emitIntField(
        sender,
        key,
        "Minimum ON time before output can be turned off [s]",
        [&]() { return hvac->getMinOnTimeS(); },
        hvac->parameterFlags.MinOnTimeSReadonly);
  }

  if (!hvac->parameterFlags.MinOffTimeSHidden) {
    hvac->generateKey(key, "min_off_s");
    emitIntField(
        sender,
        key,
        "Minimum OFF time before output can be turned on [s]",
        [&]() { return hvac->getMinOffTimeS(); },
        hvac->parameterFlags.MinOffTimeSReadonly);
  }

  if (!hvac->parameterFlags.OutputValueOnErrorHidden &&
      hvac->isOutputControlledInternally()) {
    hvac->generateKey(key, "error_val");
    emitSelectField(
        sender,
        key,
        "Output value on error",
        [&]() {
          sender->selectOption(0, "Off", hvac->getOutputValueOnError() == 0);
          sender->selectOption(
              100, "Heat", hvac->getOutputValueOnError() == 100);
          sender->selectOption(
              -100, "Cool", hvac->getOutputValueOnError() == -100);
        },
        hvac->parameterFlags.OutputValueOnErrorReadonly);
  }

  if (!hvac->parameterFlags
           .TemperatureSetpointChangeSwitchesToManualModeHidden) {
    hvac->generateKey(key, "t_chng_keeps");
    emitSwitchField(sender,
                    key,
                    "Temperature setpoint change switches to manual mode",
                    hvac->isTemperatureSetpointChangeSwitchesToManualMode(),
                    hvac->parameterFlags
                        .TemperatureSetpointChangeSwitchesToManualModeReadonly,
                    nullptr);
  }

  /*
  sender->tag("h2").body("Temperatures configuration");

  hvac->generateKey(key, "t_eco");
  emitNumberField(sender, key, "Eco [°C]",
                  [&]() { return hvac->getTemperatureEco(); });

  hvac->generateKey(key, "t_comfort");
  emitNumberField(sender, key, "Comfort [°C]",
                  [&]() { return hvac->getTemperatureComfort(); });

  hvac->generateKey(key, "t_boost");
  emitNumberField(sender, key, "Boost [°C]",
                  [&]() { return hvac->getTemperatureBoost(); });

  hvac->generateKey(key, "t_below");
  emitNumberField(sender, key, "Below alarm [°C]",
                  [&]() { return hvac->getTemperatureBelowAlarm(); });

  hvac->generateKey(key, "t_above");
  emitNumberField(sender, key, "Above alarm [°C]",
                  [&]() { return hvac->getTemperatureAboveAlarm(); });
  */
}  // NOLINT(readability/fn_size)

bool HvacParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (hvac == nullptr || cfg == nullptr) {
    return false;
  }

  if (newValue == nullptr) {
    newValue = new TSD_SuplaChannelNewValue;
    memset(newValue, 0, sizeof(TSD_SuplaChannelNewValue));
    hvacValue = reinterpret_cast<THVACValue*>(newValue->value);
    hvac->getChannel()->fillRawValue(hvacValue);
  }

  if (config == nullptr) {
    config = new TSD_ChannelConfig;
    memset(config, 0, sizeof(TSD_ChannelConfig));
    config->ChannelNumber = hvac->getChannelNumber();
    config->Func = hvac->getChannel()->getDefaultFunction();
    config->ConfigSize = sizeof(TChannelConfig_HVAC);
    config->ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
    hvacConfig = reinterpret_cast<TChannelConfig_HVAC*>(&(config->Config));
    hvac->copyFullChannelConfigTo(hvacConfig);
    // anti free will be enabled when checkbox is checked, otherwise
    // form field won't be send, so we disable it here
    // In case of hidden modifier, we copy value from current config in order
    // to keep it
    if (hvac->parameterFlags.AntiFreezeAndOverheatProtectionEnabledHidden) {
      hvacConfig->AntiFreezeAndOverheatProtectionEnabled =
          hvac->isAntiFreezeAndHeatProtectionEnabled();
    } else {
      hvacConfig->AntiFreezeAndOverheatProtectionEnabled = false;
    }

    if (hvac->parameterFlags
            .TemperatureSetpointChangeSwitchesToManualModeHidden) {
      hvacConfig->TemperatureSetpointChangeSwitchesToManualMode =
          hvac->isTemperatureSetpointChangeSwitchesToManualMode();
    } else {
      hvacConfig->TemperatureSetpointChangeSwitchesToManualMode = false;
    }

    if (hvac->parameterFlags.AuxMinMaxSetpointEnabledHidden) {
      hvacConfig->AuxMinMaxSetpointEnabled = hvac->isAuxMinMaxSetpointEnabled();
    } else {
      hvacConfig->AuxMinMaxSetpointEnabled = false;
    }
  }

  if (config == nullptr || newValue == nullptr) {
    SUPLA_LOG_ERROR("Memory allocation failed");
    return false;
  }

  char keyMatch[16] = {};

  // Only explicit, valid local choices edit references. Omitted, protected,
  // keep and malformed fields retain the current canonical reference.
  const char *referenceKeys[] = {
      "t_main", "t_aux", "sensor", "master", "pump", "hocs"};
  const bool protectedFields[] = {
      hvac->parameterFlags.MainThermometerChannelNoHidden ||
          hvac->parameterFlags.MainThermometerChannelNoReadonly,
      hvac->parameterFlags.AuxThermometerChannelNoHidden ||
          hvac->parameterFlags.AuxThermometerChannelNoReadonly,
      hvac->parameterFlags.BinarySensorChannelNoHidden ||
          hvac->parameterFlags.BinarySensorChannelNoReadonly,
      hvac->parameterFlags.MasterThermostatChannelNoHidden ||
          hvac->parameterFlags.MasterThermostatChannelNoReadonly,
      hvac->parameterFlags.PumpSwitchHidden ||
          hvac->parameterFlags.PumpSwitchReadonly,
      hvac->parameterFlags.HeatOrColdSourceSwitchHidden ||
          hvac->parameterFlags.HeatOrColdSourceSwitchReadonly};
  for (uint8_t i = 0; i < 6; ++i) {
    hvac->generateKey(keyMatch, referenceKeys[i]);
    if (strcmp(key, keyMatch) != 0) continue;
    if (protectedFields[i] || strcmp(value, "keep") == 0) return true;
    int number = -1;
    const bool unset = i < 3
        ? parseLocalChannelNumber(value, &number) &&
              number == hvac->getChannelNumber()
        : strcmp(value, "-1") == 0;
    if (!unset) {
      if (!parseLocalChannelNumber(value, &number)) return true;
      auto ch = Supla::Channel::GetByChannelNumber(number);
      if (!ch || number == hvac->getChannelNumber()) return true;
      const auto type = ch->getChannelType();
      if (i < 2) {
        if (type != SUPLA_CHANNELTYPE_THERMOMETER &&
            type != SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR) return true;
      } else if (i == 2) {
        if (type != SUPLA_CHANNELTYPE_BINARYSENSOR) return true;
      } else if (i == 3) {
        auto el = Supla::Element::getElementByChannelNumber(number);
        auto otherHvac = el ? el->getHvacBase() : nullptr;
        if (type != SUPLA_CHANNELTYPE_HVAC || !otherHvac ||
            otherHvac->isMasterThermostatSet()) return true;
      } else if (type != SUPLA_CHANNELTYPE_RELAY ||
          ch->getDefaultFunction() !=
              (i == 4 ? SUPLA_CHANNELFNC_PUMPSWITCH
                      : SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH)) {
        return true;
      }
    }
    switch (i) {
      case 0: hvacConfig->MainThermometerChannelNo = number; break;
      case 1: hvacConfig->AuxThermometerChannelNo = number; break;
      case 2: hvacConfig->BinarySensorChannelNo = number; break;
      case 3:
        hvacConfig->MasterThermostatChannelNo = unset ? 0 : number;
        hvacConfig->MasterThermostatIsSet = !unset;
        break;
      case 4:
        hvacConfig->PumpSwitchChannelNo = unset ? 0 : number;
        hvacConfig->PumpSwitchIsSet = !unset;
        break;
      case 5:
        hvacConfig->HeatOrColdSourceSwitchChannelNo = unset ? 0 : number;
        hvacConfig->HeatOrColdSourceSwitchIsSet = !unset;
        break;
    }
    referenceChanges |= 1 << i;
    return true;
  }
  hvac->generateKey(keyMatch, "fnc");

  // channel function
  if (strcmp(key, keyMatch) == 0) {
    int32_t channelFunc = stringToUInt(value);
    config->Func = channelFunc;
    return true;
  }

  hvac->generateKey(keyMatch, "subfnc");
  // channel subfunction
  if (strcmp(key, keyMatch) == 0) {
    int32_t subfunction = stringToUInt(value);
    if (subfunction >= 0 && subfunction <= 2) {
      hvacConfig->Subfunction = subfunction;
    }
    return true;
  }

  hvac->generateKey(keyMatch, "hvac_mode");
  // channel mode
  if (strcmp(key, keyMatch) == 0) {
    int32_t mode = stringToUInt(value);
    hvacValue->Mode = mode;
    return true;
  }

  hvac->generateKey(keyMatch, "t_min");
  // setpoint min temperature
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t tHeat = floatStringToInt(value, 1);
      tHeat *= 10;
      Supla::Channel::setHvacSetpointTemperatureHeat(hvacValue, tHeat);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_max");
  // setpoint max temperature
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t tCool = floatStringToInt(value, 1);
      tCool *= 10;
      Supla::Channel::setHvacSetpointTemperatureCool(hvacValue, tCool);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_aux_type");
  // aux thermometer type
  if (strcmp(key, keyMatch) == 0) {
    int32_t tSecType = stringToUInt(value);
    hvacConfig->AuxThermometerType = tSecType;
    return true;
  }

  hvac->generateKey(keyMatch, "anti_freeze");
  // anti freeze temperature
  if (strcmp(key, keyMatch) == 0) {
    bool antiFreeze = (strcmp(value, "on") == 0);
    hvacConfig->AntiFreezeAndOverheatProtectionEnabled = antiFreeze;
    return true;
  }

  hvac->generateKey(keyMatch, "t_chng_keeps");
  // temperature setpoint change switch to manual mode
  if (strcmp(key, keyMatch) == 0) {
    bool tChangeSwitch = (strcmp(value, "on") == 0);
    hvacConfig->TemperatureSetpointChangeSwitchesToManualMode = tChangeSwitch;
    return true;
  }

  hvac->generateKey(keyMatch, "algorithm");
  // algorithm
  if (strcmp(key, keyMatch) == 0) {
    int32_t algorithm = stringToUInt(value);
    hvacConfig->UsedAlgorithm = algorithm;
    return true;
  }

  hvac->generateKey(keyMatch, "min_on_s");
  // min on time
  if (strcmp(key, keyMatch) == 0) {
    int32_t minOnTimeS = stringToInt(value);
    hvacConfig->MinOnTimeS = minOnTimeS;
    return true;
  }

  hvac->generateKey(keyMatch, "min_off_s");
  // min off time
  if (strcmp(key, keyMatch) == 0) {
    int32_t minOffTimeS = stringToInt(value);
    hvacConfig->MinOffTimeS = minOffTimeS;
    return true;
  }

  hvac->generateKey(keyMatch, "t_freeze");
  // temperature freeze
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures,
          TEMPERATURE_FREEZE_PROTECTION,
          temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_eco");
  // temperature eco
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures, TEMPERATURE_ECO, temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_comfort");
  // temperature comfort
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures, TEMPERATURE_COMFORT, temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_boost");
  // temperature boost
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures, TEMPERATURE_BOOST, temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_heat");
  // temperature heat
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures, TEMPERATURE_HEAT_PROTECTION, temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_hister");
  // temperature hister
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures, TEMPERATURE_HISTERESIS, temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_below");
  // temperature below alarm
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures, TEMPERATURE_BELOW_ALARM, temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_above");
  // temperature above alarm
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures, TEMPERATURE_ABOVE_ALARM, temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "aux_ctrl");
  // aux min/max setpoint enabled
  if (strcmp(key, keyMatch) == 0) {
    bool auxMinMaxEnabled = (strcmp(value, "on") == 0);
    hvacConfig->AuxMinMaxSetpointEnabled = auxMinMaxEnabled;
    return true;
  }

  hvac->generateKey(keyMatch, "t_aux_min");
  // temperature aux min setpoint
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures, TEMPERATURE_AUX_MIN_SETPOINT, temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "t_aux_max");
  // temperature aux max setpoint
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      int16_t temperature = floatStringToInt(value, 1);
      temperature *= 10;
      Supla::Control::HvacBase::setTemperatureInStruct(
          &hvacConfig->Temperatures, TEMPERATURE_AUX_MAX_SETPOINT, temperature);
    }
    return true;
  }

  hvac->generateKey(keyMatch, "error_val");
  // output on error value
  if (strcmp(key, keyMatch) == 0) {
    if (strnlen(value, 10) > 0) {
      signed char errorValue = stringToInt(value);
      hvacConfig->OutputValueOnError = errorValue;
    }
    return true;
  }

  return false;
}

void HvacParameters::onProcessingEnd() {
  if (config != nullptr) {
    hvac->handleLocalChannelConfig(config, referenceChanges);
    referenceChanges = 0;

    hvacConfig = nullptr;
    delete config;
    config = nullptr;
  }

  if (newValue != nullptr) {
    hvac->handleNewValueFromServer(newValue);

    delete newValue;
    hvacValue = nullptr;
    newValue = nullptr;
  }
}

#endif  // ARDUINO_ARCH_AVR
