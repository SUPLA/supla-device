// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hvac_remote.h"

#include <supla/output/output.h>
#include <supla/parser/parser.h>
#include <supla/time.h>

#include <cmath>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace {

struct ModeMapping {
  int mode;
  const char *name;
};

constexpr ModeMapping modes[] = {
    {SUPLA_HVAC_MODE_OFF, "off"},
    {SUPLA_HVAC_MODE_HEAT, "heat"},
    {SUPLA_HVAC_MODE_COOL, "cool"},
    {SUPLA_HVAC_MODE_HEAT_COOL, "heat_cool"},
};

std::optional<double> readNumber(const Supla::Parser::Parser &parser,
                                 const std::string &key,
                                 double min,
                                 double max) {
  if (key.empty()) {
    return std::nullopt;
  }
  auto raw = parser.getNumberOrText(key);
  if (raw) {
    const auto *number = std::get_if<double>(&*raw);
    if (number && *number >= min && *number <= max) {
      return *number;
    }
  }
  return std::nullopt;
}

}  // namespace

namespace Supla::Control {

HvacRemote::HvacRemote(Parser::Parser &parser,
                       Output::Output &output,
                       Fields fields,
                       bool heatingIsBinary,
                       bool heatCool)
    : Sensor::SensorParsed<HvacBase>(&parser),
      output(output),
      fields(std::move(fields)),
      heatOutput(heatingIsBinary) {
  // A cooling-only thermostat uses the primary output too.
  addPrimaryOutput(this->fields.heating.empty() ? &coolOutput : &heatOutput);
  if (heatCool) {
    addSecondaryOutput(&coolOutput);
  }
  parameterFlags.UsedAlgorithmHidden = 1;
  parameterFlags.UsedAlgorithmReadonly = 1;
  parameterFlags.MinOnTimeSHidden = 1;
  parameterFlags.MinOnTimeSReadonly = 1;
  parameterFlags.MinOffTimeSHidden = 1;
  parameterFlags.MinOffTimeSReadonly = 1;
}

bool HvacRemote::Settings::operator==(const Settings &other) const {
  return mode == other.mode && heat == other.heat && cool == other.cool;
}

HvacRemote::Settings HvacRemote::readSettings() {
  const int mode = getMode();
  return {mode,
          mode == SUPLA_HVAC_MODE_HEAT || mode == SUPLA_HVAC_MODE_HEAT_COOL
              ? getTemperatureSetpointHeat() : INT16_MIN,
          mode == SUPLA_HVAC_MODE_COOL || mode == SUPLA_HVAC_MODE_HEAT_COOL
              ? getTemperatureSetpointCool() : INT16_MIN};
}

std::optional<HvacRemote::Settings> HvacRemote::readDeviceSettings(
    Settings current) const {
  if (fields.mode.empty() && fields.setpointHeat.empty() &&
      fields.setpointCool.empty()) {
    return std::nullopt;
  }
  if (!fields.mode.empty()) {
    const auto raw = parser->getNumberOrText(fields.mode);
    const auto *text = raw ? std::get_if<std::string>(&*raw) : nullptr;
    current.mode = SUPLA_HVAC_MODE_NOT_SET;
    for (const auto &mapping : modes) {
      if (text && *text == mapping.name) {
        current.mode = mapping.mode;
        break;
      }
    }
    if (current.mode == SUPLA_HVAC_MODE_NOT_SET) {
      return std::nullopt;
    }
  }
  if (!fields.setpointHeat.empty()) {
    const auto temperature =
        readNumber(*parser, fields.setpointHeat, -327.67, 327.67);
    if (!temperature) {
      return std::nullopt;
    }
    current.heat = std::lround(*temperature * 100);
  }
  if (!fields.setpointCool.empty()) {
    const auto temperature =
        readNumber(*parser, fields.setpointCool, -327.67, 327.67);
    if (!temperature) {
      return std::nullopt;
    }
    current.cool = std::lround(*temperature * 100);
  }
  return current;
}

void HvacRemote::readOutputs() {
  const auto heat = readNumber(
      *parser, fields.heating, 0, heatOutput.isOnOffOnly() ? 1 : 100);
  if (heat && (!heatOutput.isOnOffOnly() || *heat == std::trunc(*heat))) {
    heatingFeedback = std::lround(*heat);
  }

  const auto cool = readNumber(*parser, fields.cooling, 0, 1);
  if (cool && (*cool == 0 || *cool == 1)) {
    coolOutput.setOutputValueFromRemote(*cool == 1 ? -1 : 0);
  }
  heatOutput.setOutputValueFromRemote(
      coolOutput.getOutputValue() < 0 ? 0 : heatingFeedback);
}

bool HvacRemote::sendSettings(const Settings &settings) {
  for (const auto &mapping : modes) {
    if (settings.mode != mapping.mode) {
      continue;
    }
    nlohmann::json json = {{"mode", mapping.name}};
    if (settings.heat != INT16_MIN) {
      json["setpoint_heat"] = settings.heat / 100.0;
    }
    if (settings.cool != INT16_MIN) {
      json["setpoint_cool"] = settings.cool / 100.0;
    }
    return output.putContent(json.dump());
  }
  return false;
}

void HvacRemote::importSettings(const Settings &settings) {
  if (settings.mode == getMode() &&
      (isWeeklyScheduleEnabled() || isCountdownEnabled())) {
    if (isWeeklyScheduleEnabled()) {
      const auto program = getCurrentProgramId();
      if (program >= 0) {
        setWeeklyScheduleLastProgramManualOverride(program);
        setWeeklyScheduleTemporalOverride(true);
      }
    }
    setTemperatureSetpointHeat(settings.heat);
    setTemperatureSetpointCool(settings.cool);
    return;
  }
  applyNewRuntimeSettings(settings.mode, settings.heat, settings.cool);
}

void HvacRemote::synchronize(bool connected) {
  auto current = readSettings();
  if (!connected) {
    lastReadback.reset();
  }
  const auto readback =
      connected ? readDeviceSettings(current) : std::nullopt;

  if (synced && !(current == *synced)) {
    const auto now = millis();
    if (!lastSendFailure || now - *lastSendFailure >= 1000) {
      if (sendSettings(current)) {
        synced = current;
        lastSendFailure.reset();
        if (readback) {
          lastReadback = readback;
        }
      } else {
        lastSendFailure = now;
      }
    }
    return;
  }
  if (readback && (!lastReadback || !(*readback == *lastReadback))) {
    lastReadback = readback;
    if (!(*readback == current)) {
      importSettings(*readback);
      current = readSettings();
    }
  }
  synced = current;
}

void HvacRemote::iterateAlways() {
  const bool connected = parser->isSourceConnected();
  setChannelStateOnline(connected);
  if (connected) {
    parser->refreshParserSource();
    readOutputs();
  }
  HvacBase::iterateAlways();
  synchronize(connected);
}

}  // namespace Supla::Control
