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

#include "multi_ds_handler_base.h"

#include <stdio.h>

#include <supla/storage/config.h>
#include <supla-common/proto.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/supla_srpc.h>

using Supla::Sensor::MultiDsHandlerBase;

MultiDsHandlerBase::MultiDsHandlerBase(
    SuplaDeviceClass *sdc,
    uint8_t pin): sdc(sdc), pin(pin) {
}

void MultiDsHandlerBase::onLoadConfig(SuplaDeviceClass *sdc) {
  auto config = Supla::Storage::ConfigInstance();
  if (!config) {
    return;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  for (int i = 0; i < maxDeviceCount; i++) {
    Supla::Config::generateKey(key, i, DS_SENSOR_CONFIG_KEY);
    Supla::Sensor::DsSensorConfig sensorConfig;

    bool configExists = config->getBlob(key,
        reinterpret_cast<char *>(&sensorConfig), sizeof(sensorConfig));

    if (configExists) {
      char addressString[24] = {};
      addressToString(addressString, sensorConfig.address);
      SUPLA_LOG_INFO("Adding device with address %s", addressString);

      auto device = addDevice(sensorConfig.address, i);
      if (device == nullptr) {
        SUPLA_LOG_ERROR(
            "Failed to create a new device %d (address: %s)", i, addressString);
      }
    }
  }
}

void MultiDsHandlerBase::iterateAlways() {
  if (state == MultiDsState::PARING) {
    bool sensorFound = false;

    if (helperTimeMs == 0 || millis() - helperTimeMs > 1000) {
      // Verify every second
      int deviceCount = refreshSensorsCount();
      SUPLA_LOG_DEBUG("Devices count %d", deviceCount);

      for (int i = 0; i < deviceCount; i++) {
        DeviceAddress address;
        if (getSensorAddress(address, i)) {
          bool found = false;
          for (int j = 0; j < maxDeviceCount; j++) {
            if (devices[j] != nullptr) {
              if (memcmp(address, devices[j]->getAddress(), 8) == 0) {
                found = true;
                break;
              }
            }
          }

          if (found) {
            SUPLA_LOG_DEBUG("Device at idx %d already loaded, skipping...", i);
            continue;
          }

          SUPLA_LOG_DEBUG("Adding new device from idx %d", i);
          auto newDevice = addDevice(address);
          if (newDevice == nullptr) {
            SUPLA_LOG_ERROR("Failed to create a new device");
            notifySrpcAboutParingEnd(
                SUPLA_CALCFG_PAIRINGRESULT_RESOURCES_LIMIT_EXCEEDED, nullptr);
          } else {
            char name[SUPLA_DEVICE_NAME_MAXSIZE] = {};
            char addressString[24] = {};
            addressToString(addressString, address);
            snprintf(name, SUPLA_DEVICE_NAME_MAXSIZE, "DS %s", addressString);
            notifySrpcAboutParingEnd(SUPLA_CALCFG_PAIRINGRESULT_SUCCESS, name);
            sdc->scheduleProtocolsRestart(1500);
          }

          sensorFound = true;
          break;
        }
      }

      if (!sensorFound) {
        helperTimeMs = millis();
      }
    }

    if (sensorFound) {
      state = MultiDsState::READY;
      pairingStartTimeMs = 0;
      helperTimeMs = 0;
    } else if (millis() - pairingStartTimeMs >
        MUTLI_DS_DEFAULT_PAIRING_DURATION_SEC * 1000) {
      SUPLA_LOG_DEBUG("Pairing timeout - no device found!");
      notifySrpcAboutParingEnd(SUPLA_CALCFG_PAIRINGRESULT_NO_NEW_DEVICE_FOUND);
      state = MultiDsState::READY;
      pairingStartTimeMs = 0;
      helperTimeMs = 0;
    }
  }

  if (state == MultiDsState::READY) {
    if (millis() - lastBusReadTime > 10000) {
      requestTemperatures();
      lastBusReadTime = millis();
    }
  }
}

Supla::Sensor::MultiDsSensor *MultiDsHandlerBase::addDevice(
    DeviceAddress deviceAddress, int subDeviceId) {

  bool newDevice = (subDeviceId == -1);
  if (subDeviceId == -1) {
    for (int i = 0; i < maxDeviceCount; i++) {
      if (devices[i] == nullptr) {
        subDeviceId = i + 1;
        break;
      }
    }
  }

  if (subDeviceId == -1 || subDeviceId >= maxDeviceCount) {
    SUPLA_LOG_DEBUG("Cannot add new device - limit exceeded!");
    return nullptr;
  }

  SUPLA_LOG_DEBUG("Creating new sub device with id %d", subDeviceId);
  Supla::Sensor::MultiDsSensor *sensor =
      new Supla::Sensor::MultiDsSensor(
        subDeviceId,
        deviceAddress,
        [this](const uint8_t* a) { return this->getTemperature(a); });

  if (newDevice) {
    sensor->saveSensorConfig();
    SUPLA_LOG_INFO("Successfully added a new device");
  }
  devices[subDeviceId - 1] = sensor;
  return sensor;
}

bool MultiDsHandlerBase::startPairing(Supla::Protocol::SuplaSrpc *srpc,
                                  TCalCfg_SubdevicePairingResult *result) {
  SUPLA_LOG_DEBUG("Start pairing received");
  if (result != nullptr) {
    result->MaximumDurationSec = MUTLI_DS_DEFAULT_PAIRING_DURATION_SEC;
    if (state == MultiDsState::PARING) {
      SUPLA_LOG_DEBUG("Pairing already in progress");
      if (pairingStartTimeMs != 0) {
        result->ElapsedTimeSec = (millis() - pairingStartTimeMs) / 1000;
      }

      result->PairingResult = SUPLA_CALCFG_PAIRINGRESULT_ONGOING;
      return false;
    }

    result->PairingResult = SUPLA_CALCFG_PAIRINGRESULT_PROCEDURE_STARTED;
  }

  state = MultiDsState::PARING;
  pairingStartTimeMs = millis();
  this->srpc = srpc;
  return true;
}

void MultiDsHandlerBase::setMaxDeviceCount(uint8_t count) {
  if (count > MULTI_DS_MAX_DEVICES_COUNT) {
    SUPLA_LOG_WARNING("Setting max count bigger then allowed - value ignored!");
  }
  maxDeviceCount = count;
}

void MultiDsHandlerBase::setChannelNumberOffset(uint8_t offset) {
  channelNumberOffset = offset;
}

void MultiDsHandlerBase::notifySrpcAboutParingEnd(
    int pairingResult, const char *name) {

  if (srpc) {
    TCalCfg_SubdevicePairingResult result = {};
    if (pairingStartTimeMs != 0) {
      result.ElapsedTimeSec = (millis() - pairingStartTimeMs) / 1000;
    }
    int len = 0;
    if (name &&
        pairingResult != SUPLA_CALCFG_PAIRINGRESULT_NO_NEW_DEVICE_FOUND) {
      len = strnlen(name, sizeof(result.Name) - 1);
      strncpy(result.Name, name, len);
      len++;
    }

    result.MaximumDurationSec = MUTLI_DS_DEFAULT_PAIRING_DURATION_SEC;
    result.NameSize = len;
    result.PairingResult = pairingResult;

    srpc->sendPendingCalCfgResult(-1, SUPLA_CALCFG_RESULT_TRUE, -1,
        sizeof(result), &result);
    srpc->clearPendingCalCfgResult(-1);
    pairingStartTimeMs = 0;
  }
}


void MultiDsHandlerBase::addressToString(char *buffor, uint8_t *address) {
  for (uint8_t i = 0; i < 8; i++) {
    snprintf(&buffor[i * 3], 2, "%02X", address[i]);
    if (i < 7) {
      buffor[i * 3 + 2] = ':';
    }
  }
}
