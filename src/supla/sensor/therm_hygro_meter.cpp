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
#include <supla/sensor/thermometer.h>
#include <supla/log_wrapper.h>

#include <stdio.h>

Supla::Sensor::ThermHygroMeter::ThermHygroMeter() {
  channel.setType(SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR);
  channel.setDefault(SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
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

  // set temperature correction
  auto cfgTempCorrection = getConfiguredTemperatureCorrection();
  double correction = 1.0 * cfgTempCorrection / 10.0;
  getChannel()->setCorrection(correction);
  SUPLA_LOG_DEBUG("Channel[%d] temperature correction %f",
      getChannelNumber(), correction);

  // set humidity correction
  auto cfgHumCorrection = getConfiguredHumidityCorrection();
  correction = 1.0 * cfgHumCorrection / 10.0;
  getChannel()->setCorrection(correction, true);
  SUPLA_LOG_DEBUG("Channel[%d] humidity correction %f",
      getChannelNumber(), correction);

  // load config changed offline flags
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "cfg_chng");
  uint8_t flag = 0;
  cfg->getUInt8(key, &flag);
  SUPLA_LOG_INFO(
      "Channel[%d] config changed offline flag %d", getChannelNumber(), flag);
  if (flag) {
    setChannelStateFlag = 1;
  } else {
    setChannelStateFlag = 0;
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

void Supla::Sensor::ThermHygroMeter::onRegistered(
    Supla::Protocol::SuplaSrpc *suplaSrpc) {
  setChannelResult = 0;
  configFinishedReceived = false;
  defaultConfigReceived = false;
  Supla::Element::onRegistered(suplaSrpc);
  if (Supla::Storage::ConfigInstance()) {
    if (setChannelStateFlag) {
      setChannelStateFlag = 1;
      waitForChannelConfigAndIgnoreIt = true;
    }
  }
}

uint8_t Supla::Sensor::ThermHygroMeter::handleChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  (void)(local);
  SUPLA_LOG_DEBUG(
      "handleChannelConfig, func %d, configtype %d, configsize %d",
      result->Func,
      result->ConfigType,
      result->ConfigSize);
  if (waitForChannelConfigAndIgnoreIt && !local) {
    SUPLA_LOG_INFO(
        "Ignoring config for channel %d (local config changed offline)",
        getChannelNumber());
    waitForChannelConfigAndIgnoreIt = false;
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return SUPLA_CONFIG_RESULT_TRUE;
  }
  if (result->ConfigType != 0) {
    SUPLA_LOG_DEBUG("Channel[%d] invalid configtype", getChannelNumber());
    return SUPLA_CONFIG_RESULT_FALSE;
  }
  defaultConfigReceived = true;

  if (result->ConfigSize == 0) {
    // server doesn't have channel configuration, so we'll send it
    // But don't send it if it failed in previous attempt
    if (setChannelResult == 0) {
      setChannelStateFlag = 1;
    }
    return SUPLA_CONFIG_RESULT_TRUE;
  }
  if (result->Func == SUPLA_CHANNELFNC_THERMOMETER ||
      result->Func == SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE ||
      result->Func == SUPLA_CHANNELFNC_HUMIDITY) {
    if (result->ConfigSize < sizeof(TChannelConfig_TemperatureAndHumidity)) {
      return SUPLA_CONFIG_RESULT_DATA_ERROR;
    }
    auto configFromServer =
        reinterpret_cast<TChannelConfig_TemperatureAndHumidity *>(
            result->Config);

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
          false);
    }
  }

  return SUPLA_CONFIG_RESULT_TRUE;
}

void Supla::Sensor::ThermHygroMeter::setHumidityCorrection(int32_t correction) {
  setCorrectionAtIndex(correction, 1);
}

void Supla::Sensor::ThermHygroMeter::applyCorrectionsAndStoreIt(
    int32_t temperatureCorrection, int32_t humidityCorrection, bool local) {
  if (local) {
    setChannelStateFlag = 1;
  } else {
    setChannelStateFlag = 0;
  }

  setTemperatureCorrection(temperatureCorrection);
  setHumidityCorrection(humidityCorrection);

  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "cfg_chng");
  if (setChannelStateFlag) {
    cfg->setUInt8(key, 1);
  } else {
    cfg->setUInt8(key, 0);
  }
  cfg->saveWithDelay(5000);

  // reload config
  onLoadConfig(nullptr);
}

bool Supla::Sensor::ThermHygroMeter::iterateConnected() {
  auto result = Element::iterateConnected();

  if (!result) {
    return result;
  }

  if (configFinishedReceived) {
    if (setChannelStateFlag == 1) {
      for (auto proto = Supla::Protocol::ProtocolLayer::first();
           proto != nullptr;
           proto = proto->next()) {
        TChannelConfig_TemperatureAndHumidity config = {};
        int16_t cfgTempCorr = getConfiguredTemperatureCorrection();
        int16_t cfgHumCorr = getConfiguredHumidityCorrection();
        config.TemperatureAdjustment = cfgTempCorr * 10;
        config.HumidityAdjustment = cfgHumCorr * 10;
        config.AdjustmentAppliedByDevice = 1;
        if (proto->setChannelConfig(
                getChannelNumber(),
                channel.getDefaultFunction(),
                reinterpret_cast<void *>(&config),
                sizeof(TChannelConfig_TemperatureAndHumidity),
                SUPLA_CONFIG_TYPE_DEFAULT)) {
          SUPLA_LOG_INFO("Sending channel config for %d", getChannelNumber());
          setChannelStateFlag = 2;
          return true;
        }
      }
    }
  }

  return result;
}

void Supla::Sensor::ThermHygroMeter::handleSetChannelConfigResult(
    TSDS_SetChannelConfigResult *result) {
  if (result == nullptr) {
    return;
  }

  bool success = (result->Result == SUPLA_CONFIG_RESULT_TRUE);
  (void)(success);
  setChannelResult = result->Result;

  switch (result->ConfigType) {
    case SUPLA_CONFIG_TYPE_DEFAULT: {
      SUPLA_LOG_INFO("set channel config %s (%d)",
                     success ? "succeeded" : "failed",
                     result->Result);
      clearChannelConfigChangedFlag();
      break;
    }
    default:
      break;
  }
}

void Supla::Sensor::ThermHygroMeter::clearChannelConfigChangedFlag() {
  if (setChannelStateFlag) {
    setChannelStateFlag = 0;
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
      generateKey(key, "cfg_chng");
      cfg->setUInt8(key, 0);
      cfg->saveWithDelay(1000);
    }
  }
}

void Supla::Sensor::ThermHygroMeter::handleChannelConfigFinished() {
  configFinishedReceived = true;
  if (!defaultConfigReceived) {
    // trigger sending channel config to server
    setChannelStateFlag = 1;
  }
}

