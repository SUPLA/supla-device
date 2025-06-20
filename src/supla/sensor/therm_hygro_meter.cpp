/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include "therm_hygro_meter.h"

#include <supla/time.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/sensor/thermometer.h>
#include <supla/log_wrapper.h>

#include <stdio.h>

Supla::Sensor::ThermHygroMeter::ThermHygroMeter() {
  channel.setType(SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

void Supla::Sensor::ThermHygroMeter::onInit() {
  channel.setNewValue(getTemp(), getHumi());
}

double Supla::Sensor::ThermHygroMeter::getTemp() {
  return TEMPERATURE_NOT_AVAILABLE;
}

double Supla::Sensor::ThermHygroMeter::getHumi() {
  return HUMIDITY_NOT_AVAILABLE;
}

int16_t Supla::Sensor::ThermHygroMeter::getHumiInt16() {
  if (getChannelNumber() >= 0) {
    double humi = getChannel()->getValueDoubleSecond();
    if (humi <= HUMIDITY_NOT_AVAILABLE) {
      return INT16_MIN;
    }
    humi *= 100;
    if (humi > INT16_MAX) {
      return INT16_MAX;
    }
    if (humi <= INT16_MIN) {
      return INT16_MIN + 1;
    }
    return humi;
  }
  return INT16_MIN;
}

int16_t Supla::Sensor::ThermHygroMeter::getTempInt16() {
  if (getChannelNumber() >= 0) {
    double temp = getChannel()->getLastTemperature();
    if (temp <= TEMPERATURE_NOT_AVAILABLE) {
      return INT16_MIN;
    }
    temp *= 100;
    if (temp > INT16_MAX) {
      return INT16_MAX;
    }
    if (temp <= INT16_MIN) {
      return INT16_MIN + 1;
    }
    return temp;
  }
  return INT16_MIN;
}

void Supla::Sensor::ThermHygroMeter::iterateAlways() {
  if (millis() - lastReadTime > refreshIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getTemp(), getHumi());
  }
}

void Supla::Sensor::ThermHygroMeter::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }

  // temperature correction
  auto cfgTempCorrection = getConfiguredTemperatureCorrection();
  double correction = 1.0 * cfgTempCorrection / 10.0;
  SUPLA_LOG_DEBUG("Channel[%d] temperature correction %.2f",
                  getChannelNumber(),
                  correction);
  if (applyCorrections) {
    getChannel()->setCorrection(correction);
  }

  // humidity correction
  auto cfgHumCorrection = getConfiguredHumidityCorrection();
  correction = 1.0 * cfgHumCorrection / 10.0;
  SUPLA_LOG_DEBUG(
      "Channel[%d] humidity correction %.2f", getChannelNumber(), correction);

  if (applyCorrections) {
    getChannel()->setCorrection(correction, true);
  }

  // load config changed offline flags
  loadConfigChangeFlag();
  lastReadTime = 0;
}

void Supla::Sensor::ThermHygroMeter::purgeConfig() {
  Supla::ElementWithChannelActions::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }
  char key[16] = {};
  for (uint8_t i = 0; i < 2; i++) {
    snprintf(key,
             sizeof(key),
             "corr_%d_%d",
             static_cast<int16_t>(getChannelNumber()),
             i);
    cfg->eraseKey(key);
  }
}

int16_t Supla::Sensor::ThermHygroMeter::readCorrectionFromIndex(int index) {
  if (index < 0 || index > 1) {
    return 0;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  int32_t correction = 0;
  if (cfg) {
    char key[16] = {};
    snprintf(key, sizeof(key), "corr_%d_%d", getChannelNumber(), index);
    cfg->getInt32(key, &correction);
  }
  if (correction < -500) {
    correction = -500;
  }
  if (correction > 500) {
    correction = 500;
  }
  return correction;
}

int16_t Supla::Sensor::ThermHygroMeter::getConfiguredTemperatureCorrection() {
  return readCorrectionFromIndex(0);
}

int16_t Supla::Sensor::ThermHygroMeter::getConfiguredHumidityCorrection() {
  return readCorrectionFromIndex(1);
}

void Supla::Sensor::ThermHygroMeter::setCorrectionAtIndex(int32_t correction,
                                                          int index) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg || index < 0 || index > 1) {
    return;
  }
  // fix value to -500..500
  if (correction < -500) {
    correction = -500;
  } else if (correction > 500) {
    correction = 500;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  snprintf(key, sizeof(key), "corr_%d_%d", getChannelNumber(), index);
  cfg->setInt32(key, correction);
}

void Supla::Sensor::ThermHygroMeter::setTemperatureCorrection(
    int32_t correction) {
  setCorrectionAtIndex(correction, 0);
}

void Supla::Sensor::ThermHygroMeter::setRefreshIntervalMs(int intervalMs) {
  refreshIntervalMs = intervalMs;
}

Supla::ApplyConfigResult Supla::Sensor::ThermHygroMeter::applyChannelConfig(
    TSD_ChannelConfig *result, bool) {
  if (result->ConfigSize == 0) {
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }

  bool applyAsLocalChange = false;
  if (result->Func == SUPLA_CHANNELFNC_THERMOMETER ||
      result->Func == SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE ||
      result->Func == SUPLA_CHANNELFNC_HUMIDITY) {
    if (result->ConfigSize < sizeof(TChannelConfig_TemperatureAndHumidity)) {
      return Supla::ApplyConfigResult::DataError;
    }
    auto configFromServer =
        reinterpret_cast<TChannelConfig_TemperatureAndHumidity *>(
            result->Config);

    if (channelConfigState != Supla::ChannelConfigState::LocalChangePending) {
      if (configFromServer->MinTemperatureAdjustment !=
              -minMaxAllowedTemperatureAdjustment * 10 ||
          configFromServer->MaxTemperatureAdjustment !=
              minMaxAllowedTemperatureAdjustment * 10 ||
          configFromServer->MinHumidityAdjustment !=
              -minMaxAllowedHumidityAdjustment * 10 ||
          configFromServer->MaxHumidityAdjustment !=
              minMaxAllowedHumidityAdjustment * 10) {
        applyAsLocalChange = true;
        SUPLA_LOG_INFO(
            "Channel[%d] min/max temperature/humidity adjustment not set on "
            "server",
            getChannelNumber());
      }
    }

    if (configFromServer->AdjustmentAppliedByDevice == 0) {
      auto cfgTempCorrection = getConfiguredTemperatureCorrection();
      auto cfgHumiCorrection = getConfiguredHumidityCorrection();

      cfgTempCorrection += (configFromServer->TemperatureAdjustment / 10);
      cfgHumiCorrection += (configFromServer->HumidityAdjustment / 10);

      applyCorrectionsAndStoreIt(cfgTempCorrection, cfgHumiCorrection, true);
    } else {
      applyCorrectionsAndStoreIt(
          configFromServer->TemperatureAdjustment / 10,
          configFromServer->HumidityAdjustment / 10,
          applyAsLocalChange);
    }
  }

  return (applyAsLocalChange ? Supla::ApplyConfigResult::SetChannelConfigNeeded
                             : Supla::ApplyConfigResult::Success);
}

void Supla::Sensor::ThermHygroMeter::setHumidityCorrection(int32_t correction) {
  setCorrectionAtIndex(correction, 1);
}

void Supla::Sensor::ThermHygroMeter::applyCorrectionsAndStoreIt(
    int32_t temperatureCorrection, int32_t humidityCorrection, bool local) {
  if (local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
  } else {
    channelConfigState = Supla::ChannelConfigState::None;
  }

  setTemperatureCorrection(temperatureCorrection);
  setHumidityCorrection(humidityCorrection);

  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }

  saveConfigChangeFlag();

  // reload config
  onLoadConfig(nullptr);
}

void Supla::Sensor::ThermHygroMeter::fillChannelConfig(void *channelConfig,
                                                       int *size,
                                                       uint8_t configType) {
  if (size) {
    *size = 0;
  } else {
    return;
  }
  if (channelConfig == nullptr) {
    return;
  }

  if (configType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return;
  }

  auto config = reinterpret_cast<TChannelConfig_TemperatureAndHumidity *>(
      channelConfig);
  *size = sizeof(TChannelConfig_TemperatureAndHumidity);

  int16_t cfgTempCorr = getConfiguredTemperatureCorrection();
  int16_t cfgHumCorr = getConfiguredHumidityCorrection();
  config->TemperatureAdjustment = cfgTempCorr * 10;
  config->HumidityAdjustment = cfgHumCorr * 10;
  config->AdjustmentAppliedByDevice = 1;
  config->MinTemperatureAdjustment = -minMaxAllowedTemperatureAdjustment * 10;
  config->MaxTemperatureAdjustment = minMaxAllowedTemperatureAdjustment * 10;
  config->MinHumidityAdjustment = -minMaxAllowedHumidityAdjustment * 10;
  config->MaxHumidityAdjustment = minMaxAllowedHumidityAdjustment * 10;
}

void Supla::Sensor::ThermHygroMeter::setApplyCorrections(
    bool applyCorrections) {
  this->applyCorrections = applyCorrections;
}

void Supla::Sensor::ThermHygroMeter::setMinMaxAllowedTemperatureAdjustment(
    int32_t minMax) {
  if (minMax < 0) {
    minMax = 0;
  }
  if (minMax > 200) {
    minMax = 200;
  }
  minMaxAllowedTemperatureAdjustment = minMax;
}

void Supla::Sensor::ThermHygroMeter::setMinMaxAllowedHumidityAdjustment(
    int32_t minMax) {
  if (minMax < 0) {
    minMax = 0;
  }
  if (minMax > 500) {
    minMax = 500;
  }
  minMaxAllowedHumidityAdjustment = minMax;
}
