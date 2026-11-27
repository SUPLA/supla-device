// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "electricity_meter_parsed.h"
#include <supla/log_wrapper.h>

#include <cstring>
#include <map>
#include <string>

namespace {

// Checks that a value fits its SDK field after unit scaling.
bool fitsSdkField(const std::string &key, double value) {
  constexpr double int32Range = 2147483648.0;
  // Halved int64 range, so forward minus reverse power also fits.
  constexpr double powerRange = 4611686018427387904.0;
  static constexpr struct {
    const char *prefix;
    double scale;
    double min;
    double max;  // exclusive
  } limits[] = {
      {"voltage_", 100, 0, 65536},
      {"frequency", 100, 0, 65536},
      {"current_", 1000, 0, 2 * int32Range},
      {"power_factor_", 1000, -int32Range, int32Range},
      {"phase_angle_", 10, -int32Range, int32Range},
      {"power_", 100000, -powerRange, powerRange},
      {"rvr_power_", 100000, -powerRange, powerRange},
      {"", 100000, 0, 4 * powerRange},  // uint64 energy counters
  };
  for (const auto &limit : limits) {
    if (key.compare(0, strlen(limit.prefix), limit.prefix) == 0) {
      const double scaled = value * limit.scale;
      return scaled >= limit.min && scaled < limit.max;
    }
  }
  return false;
}

}  // namespace

Supla::Sensor::ElectricityMeterParsed::ElectricityMeterParsed(
    Supla::Parser::Parser *parser) :
  Supla::Sensor::SensorParsed<Supla::Sensor::ElectricityMeter>(parser) {}

void Supla::Sensor::ElectricityMeterParsed::onInit() {
  readValuesFromDevice();
  updateChannelValues();
}

void Supla::Sensor::ElectricityMeterParsed::setOfflineOnInvalidMeasurements(
    bool enabled) {
  offlineOnInvalidMeasurements = enabled;
}

void Supla::Sensor::ElectricityMeterParsed::readValuesFromDevice() {
  bool measurementsValid = refreshParserSource(!offlineOnInvalidMeasurements);
  if (offlineOnInvalidMeasurements && measurementsValid &&
      parser->lastReadUsedFallback()) {
    // A cached read during the source grace period is not new data.
    return;
  }
  std::map<std::string, double> measurements;
  if (measurementsValid) {
    for (const auto& mapping : parameterToKey) {
      if (mapping.first == Supla::Sensor::BatteryState ||
          mapping.first == Supla::Sensor::BatteryLevel ||
          mapping.first == Supla::Sensor::BatteryPowered) {
        continue;
      }
      if (!offlineOnInvalidMeasurements) {
        getParameterValue(mapping.first);
        if (!parser->isValid()) {
          measurementsValid = false;
          break;
        }
        continue;
      }
      const auto raw = parser->readMeasurement(mapping.second);
      if (!raw) {
        measurementsValid = false;
        break;
      }
      const auto multiplier = parameterMultiplier.find(mapping.first);
      const double value = *raw * (multiplier == parameterMultiplier.end()
                                       ? 1.0 : multiplier->second);
      if (!fitsSdkField(mapping.first, value)) {
        measurementsValid = false;
        break;
      }
      measurements.emplace(mapping.first, value);
    }
  }
  if (offlineOnInvalidMeasurements) {
    measurementsValid = measurementsValid && !measurements.empty();
  }
  auto measurement = [&](const char *key) {
    return offlineOnInvalidMeasurements ? measurements.at(key)
                                        : getParameterValue(key);
  };
  if (measurementsValid) {
    if (isParameterConfigured(Supla::Parser::FwdActEnergy1)) {
      setFwdActEnergy(0,
          measurement(Supla::Parser::FwdActEnergy1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::FwdActEnergy2)) {
      setFwdActEnergy(1,
          measurement(Supla::Parser::FwdActEnergy2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::FwdActEnergy3)) {
      setFwdActEnergy(2,
          measurement(Supla::Parser::FwdActEnergy3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::RvrActEnergy1)) {
      setRvrActEnergy(0,
          measurement(Supla::Parser::RvrActEnergy1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::RvrActEnergy2)) {
      setRvrActEnergy(1,
          measurement(Supla::Parser::RvrActEnergy2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::RvrActEnergy3)) {
      setRvrActEnergy(2,
          measurement(Supla::Parser::RvrActEnergy3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::FwdReactEnergy1)) {
      setFwdReactEnergy(
          0, measurement(Supla::Parser::FwdReactEnergy1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::FwdReactEnergy2)) {
      setFwdReactEnergy(
          1, measurement(Supla::Parser::FwdReactEnergy2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::FwdReactEnergy3)) {
      setFwdReactEnergy(
          2, measurement(Supla::Parser::FwdReactEnergy3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::RvrReactEnergy1)) {
      setRvrReactEnergy(
          0, measurement(Supla::Parser::RvrReactEnergy1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::RvrReactEnergy2)) {
      setRvrReactEnergy(
          1, measurement(Supla::Parser::RvrReactEnergy2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::RvrReactEnergy3)) {
      setRvrReactEnergy(
          2, measurement(Supla::Parser::RvrReactEnergy3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::Current1)) {
      setCurrent(0, measurement(Supla::Parser::Current1) * 1000);
    }
    if (isParameterConfigured(Supla::Parser::Current2)) {
      setCurrent(1, measurement(Supla::Parser::Current2) * 1000);
    }
    if (isParameterConfigured(Supla::Parser::Current3)) {
      setCurrent(2, measurement(Supla::Parser::Current3) * 1000);
    }

    // Supla shows signed power (negative value means power returned to grid)
    if (isParameterConfigured(Supla::Parser::PowerActive1)) {
      if (isParameterConfigured(Supla::Parser::RvrPowerActive1)) {
        setPowerActive(0,
            (measurement(Supla::Parser::PowerActive1) -
             measurement(Supla::Parser::RvrPowerActive1)) *
            100000);
      } else {
        setPowerActive(0,
            measurement(Supla::Parser::PowerActive1) * 100000);
      }
    } else if (isParameterConfigured(Supla::Parser::RvrPowerActive1)) {
      setPowerActive(
          0, -measurement(Supla::Parser::RvrPowerActive1) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerActive2)) {
      if (isParameterConfigured(Supla::Parser::RvrPowerActive2)) {
        setPowerActive(1,
            (measurement(Supla::Parser::PowerActive2) -
             measurement(Supla::Parser::RvrPowerActive2)) *
            100000);
      } else {
        setPowerActive(1,
            measurement(Supla::Parser::PowerActive2) * 100000);
      }
    } else if (isParameterConfigured(Supla::Parser::RvrPowerActive2)) {
      setPowerActive(
          1, -measurement(Supla::Parser::RvrPowerActive2) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerActive3)) {
      if (isParameterConfigured(Supla::Parser::RvrPowerActive3)) {
        setPowerActive(2,
            (measurement(Supla::Parser::PowerActive3) -
             measurement(Supla::Parser::RvrPowerActive3)) *
            100000);
      } else {
        setPowerActive(2,
            measurement(Supla::Parser::PowerActive3) * 100000);
      }
    } else if (isParameterConfigured(Supla::Parser::RvrPowerActive3)) {
      setPowerActive(
          2, -measurement(Supla::Parser::RvrPowerActive3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerReactive1)) {
      setPowerReactive(
          0, measurement(Supla::Parser::PowerReactive1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::PowerReactive2)) {
      setPowerReactive(
          1, measurement(Supla::Parser::PowerReactive2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::PowerReactive3)) {
      setPowerReactive(
          2, measurement(Supla::Parser::PowerReactive3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerApparent1)) {
      setPowerApparent(
          0, measurement(Supla::Parser::PowerApparent1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::PowerApparent2)) {
      setPowerApparent(
          1, measurement(Supla::Parser::PowerApparent2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::PowerApparent3)) {
      setPowerApparent(
          2, measurement(Supla::Parser::PowerApparent3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerFactor1)) {
      setPowerFactor(0, measurement(Supla::Parser::PowerFactor1) * 1000);
    }
    if (isParameterConfigured(Supla::Parser::PowerFactor2)) {
      setPowerFactor(1, measurement(Supla::Parser::PowerFactor2) * 1000);
    }
    if (isParameterConfigured(Supla::Parser::PowerFactor3)) {
      setPowerFactor(2, measurement(Supla::Parser::PowerFactor3) * 1000);
    }

    if (isParameterConfigured(Supla::Parser::PhaseAngle1)) {
      setPhaseAngle(0, measurement(Supla::Parser::PhaseAngle1) * 10);
    }
    if (isParameterConfigured(Supla::Parser::PhaseAngle2)) {
      setPhaseAngle(1, measurement(Supla::Parser::PhaseAngle2) * 10);
    }
    if (isParameterConfigured(Supla::Parser::PhaseAngle3)) {
      setPhaseAngle(2, measurement(Supla::Parser::PhaseAngle3) * 10);
    }

    if (isParameterConfigured(Supla::Parser::Voltage1)) {
      setVoltage(0, measurement(Supla::Parser::Voltage1) * 100);
    }
    if (isParameterConfigured(Supla::Parser::Voltage2)) {
      setVoltage(1, measurement(Supla::Parser::Voltage2) * 100);
    }
    if (isParameterConfigured(Supla::Parser::Voltage3)) {
      setVoltage(2, measurement(Supla::Parser::Voltage3) * 100);
    }

    if (isParameterConfigured(Supla::Parser::Frequency)) {
      setFreq(measurement(Supla::Parser::Frequency) * 100);
    }
    isDataErrorLogged = false;

  } else {
    if (!isDataErrorLogged) {
      isDataErrorLogged = true;
      SUPLA_LOG_WARNING("ElectricityMeterParsed: data source error");
    }
    resetReadParameters();
  }
  if (offlineOnInvalidMeasurements) {
    setChannelStateOnline(measurementsValid);
  }
}
