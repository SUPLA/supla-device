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

#include <supla/auto_lock.h>
#include <supla/log_wrapper.h>
#include <supla-common/proto.h>
#include <supla/storage/config.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/device/register_device.h>

#define DS_HANDLER_ADDRESS_LENGTH 24
#define DS_NAME "DS18B20"
#define DS_UNUSED_STRING "-"

using Supla::Sensor::MultiDsHandlerBase;

MultiDsHandlerBase::MultiDsHandlerBase(
    SuplaDeviceClass *sdc,
    uint8_t pin): sdc(sdc), pin(pin) {}

MultiDsHandlerBase::~MultiDsHandlerBase() {
  for (int i = 0; i < MULTI_DS_MAX_DEVICES_COUNT; i++) {
    auto sensor = sensors[i];
    if (sensor != nullptr) {
      delete sensor;
      sensors[i] = nullptr;
    }
  }

  for (int i = 0; i < MULTI_DS_MAX_ACTIONS; i++) {
    auto action = actions[i];
    if (action != nullptr) {
      delete action;
      actions[i] = nullptr;
    }
  }
}

void MultiDsHandlerBase::onLoadConfig(SuplaDeviceClass *sdc) {
  auto config = Supla::Storage::ConfigInstance();
  if (!config) {
    return;
  }

  anySensorLoaded = false;
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  for (int i = 0; i < maxDeviceCount; i++) {
    int subDeviceId = i + 1;
    Supla::Config::generateKey(key, subDeviceId, DS_SENSOR_CONFIG_KEY);
    Supla::Sensor::DsSensorConfig sensorConfig;

    SUPLA_LOG_DEBUG("MultiDS: Loading config for key %s", key);
    bool configExists = config->getBlob(key,
        reinterpret_cast<char *>(&sensorConfig), sizeof(sensorConfig));

    if (configExists) {
      anySensorLoaded = true;
      char addressString[DS_HANDLER_ADDRESS_LENGTH] = {};
      addressToString(addressString, DS_HANDLER_ADDRESS_LENGTH,
                      sensorConfig.address);
      SUPLA_LOG_INFO("MultiDS: Adding device with address %s", addressString);

      auto device = addDevice(sensorConfig.address, sensorConfig.channelNumber,
                              subDeviceId);
      if (device == nullptr) {
        SUPLA_LOG_ERROR(
            "MultiDS: Failed to create a new device %d (address: %s)",
            subDeviceId,
            addressString);
      }
    }
  }
}

void MultiDsHandlerBase::onInit() {
  if (searchFirstDevice && !anySensorLoaded) {
    initialSensorSearch();
  }
}

void MultiDsHandlerBase::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  Element::onRegistered(suplaSrpc);

  this->srpc = suplaSrpc;

  for (int i = 0; i < maxDeviceCount; i++) {
    auto sensor = sensors[i];
    if (sensor != nullptr) {
      sensor->setDetailsSend(false);
    }
  }
}

void MultiDsHandlerBase::iterateAlways() {
  if (state == MultiDsState::PARING) {
    bool sensorFound = false;

    if (helperTimeMs == 0 || millis() - helperTimeMs > 1000) {
      // Verify every second
      int deviceCount = refreshSensorsCount();
      SUPLA_LOG_DEBUG("MultiDS: Devices count %d", deviceCount);

      for (int i = 0; i < deviceCount; i++) {
        DeviceAddress address;
        if (getSensorAddress(address, i)) {
          bool found = false;
          for (int j = 0; j < maxDeviceCount; j++) {
            if (sensors[j] != nullptr) {
              if (memcmp(address, sensors[j]->getAddress(), 8) == 0) {
                found = true;
                break;
              }
            }
          }

          if (found) {
            SUPLA_LOG_DEBUG(
                "MultiDS: Device at idx %d already loaded, skipping...", i);
            continue;
          }

          SUPLA_LOG_DEBUG("MultiDS: Adding new device from idx %d", i);
          auto newDevice = addDevice(address);
          if (newDevice == nullptr) {
            SUPLA_LOG_ERROR("MultiDS: Failed to create a new device");
            notifySrpcAboutParingEnd(
                SUPLA_CALCFG_PAIRINGRESULT_RESOURCES_LIMIT_EXCEEDED, nullptr);
          } else {
            char name[SUPLA_DEVICE_NAME_MAXSIZE] = {};
            char addressString[DS_HANDLER_ADDRESS_LENGTH] = {};
            addressToString(addressString, DS_HANDLER_ADDRESS_LENGTH, address);
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
    } else if (millis() - pairingStartTimeMs > pairingTimeout * 1000) {
      SUPLA_LOG_DEBUG("MultiDS: Pairing timeout - no device found!");
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


bool MultiDsHandlerBase::iterateConnected() {
  bool dataSend = false;
  if (srpc == nullptr) {
    return !dataSend;
  }

  if (!useSubDevices) {
    return !dataSend;
  }

  for (int i = 0; i < maxDeviceCount; i++) {
    auto sensor = sensors[i];
    if (sensor && !sensor->getDetailsSend()) {
      SUPLA_LOG_DEBUG("MultiDS: Sending sub device info (idx: %d)", i);

      TDS_SubdeviceDetails subdeviceDetails = {};
      subdeviceDetails.SubDeviceId = i + 1;
      strncpy(subdeviceDetails.Name, DS_NAME, SUPLA_DEVICE_NAME_MAXSIZE - 1);
      addressToString(subdeviceDetails.SerialNumber,
                      SUPLA_SUBDEVICE_SERIAL_NUMBER_MAXSIZE,
                      sensor->getAddress());

      srpc->sendSubdeviceDetails(&subdeviceDetails);
      sensor->setDetailsSend(true);
      dataSend = true;
      break;
    }
  }

  return !dataSend;
}

Supla::Sensor::MultiDsSensor *MultiDsHandlerBase::addDevice(
    DeviceAddress deviceAddress, int channelNumber, int subDeviceId) {

  bool newDevice = (subDeviceId == -1);
  if (subDeviceId == -1) {
    for (int i = 0; i < maxDeviceCount; i++) {
      if (sensors[i] == nullptr) {
        subDeviceId = i + 1;
        break;
      }
    }
  }

  if (subDeviceId == -1 || subDeviceId > maxDeviceCount) {
    SUPLA_LOG_DEBUG("MultiDS: Cannot add new device - limit exceeded!");
    return nullptr;
  }

  if (channelNumber == -1) {
    if (channelNumberOffset == -1) {
      channelNumber = Supla::RegisterDevice::getNextFreeChannelNumber();
      SUPLA_LOG_DEBUG("MultiDS: Took next channel number - %d", channelNumber);
    } else {
      channelNumber = channelNumberOffset + subDeviceId - 1;
      SUPLA_LOG_DEBUG("MultiDS: Channel number calculated - %d", channelNumber);
    }
  }

  SUPLA_LOG_DEBUG("MultiDS: Creating new sub device with id %d", subDeviceId);
  Supla::Sensor::MultiDsSensor *sensor =
      new Supla::Sensor::MultiDsSensor(subDeviceId, deviceAddress,
          useSubDevices, this);

  // Check if allocated
  if (sensor == nullptr) {
    SUPLA_LOG_ERROR("MultiDS: Device add failed!");
    return nullptr;
  }

  // Assign actions if configured
  for (int i = 0; i < MULTI_DS_MAX_ACTIONS; i++) {
    auto action = actions[i];
    if (action == nullptr) {
      break;
    }
    uint8_t idx = subDeviceId - 1;
    if (action->idx == idx) {
      SUPLA_LOG_DEBUG("MultiDS: Assigning action %d for channel on idx %d",
                      action->action, idx);
      sensor->addAction(action->action, action->client, action->condition,
                        action->allwaysEnabled);
    }
  }

  if (channelStateDisabled) {
    sensor->disableChannelState();
  }
  sensor->getChannel()->setChannelNumber(channelNumber);
  if (newDevice) {
    sensor->saveSensorConfig();
    SUPLA_LOG_INFO("MultiDS: Successfully added a new device");
  }
  SUPLA_LOG_DEBUG("MultiDS: Device added (subId: %d, number %d)",
      subDeviceId, sensor->getChannel()->getChannelNumber());

  sensors[subDeviceId - 1] = sensor;
  return sensor;
}

bool MultiDsHandlerBase::startPairing(Supla::Protocol::SuplaSrpc *srpc,
    TCalCfg_SubdevicePairingResult *result) {
  SUPLA_LOG_DEBUG("MultiDS: Start pairing received");
  if (result != nullptr) {
    result->MaximumDurationSec = pairingTimeout;
    if (state == MultiDsState::PARING) {
      SUPLA_LOG_DEBUG("MultiDS: Pairing already in progress");
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

bool MultiDsHandlerBase::onChannelConflictReport(
    uint8_t *channelReport,
    uint8_t channelReportSize,
    bool hasConflictInvalidType,
    bool hasConflictChannelMissingOnServer,
    bool hasConflictChannelMissingOnDevice) {
  if (hasConflictChannelMissingOnDevice) {
    SUPLA_LOG_ERROR("MultiDS: Channel conflict - channel missing on device. "
        "Aborting...");
    return false;
  }
  if (hasConflictInvalidType) {
    SUPLA_LOG_ERROR("MultiDS: Channel conflict - channel type mismatch. "
        "Aborting...");
    return false;
  }
  if (hasConflictChannelMissingOnServer) {
    SUPLA_LOG_INFO(
        "MultiDS: Channel conflict - channel missing on server. "
        "Trying to remove affected devices...");

    for (int i = 0; i < maxDeviceCount; i++) {
      auto sensor = sensors[i];
      if (sensor == nullptr) {
        SUPLA_LOG_DEBUG("MultiDS: No sensor at position: %d", i);
        continue;
      }

      int channelNumber = sensor->getChannel()->getChannelNumber();
      int subDeviceId = sensor->getChannel()->getSubDeviceId();
      if ((channelNumber >= channelReportSize ||
          channelReport[channelNumber] == 0) &&
          !Supla::RegisterDevice::isChannelNumberFree(channelNumber)) {
        Supla::AutoLock lock(sdc->getTimerAccessMutex());

        sensor->purgeConfig();
        delete sensor;
        sensor = nullptr;
        sensors[i] = nullptr;

        if (useSubDevices) {
          SUPLA_LOG_DEBUG("MultiDS: Channel removed (subId: %d, number: %d)",
                         sensor->getChannel()->getSubDeviceId(),
                         channelNumber);
        } else {
          SUPLA_LOG_DEBUG("MultiDS: Channel removed (idx: %d, number: %d)", i,
                         channelNumber);
        }
      }
    }
  }

  return false;
}

void MultiDsHandlerBase::addAction(uint8_t idx, uint16_t action,
    Supla::ActionHandler *client, Supla::Condition *condition,
    bool alwaysEnabled) {
  if (idx < MULTI_DS_MAX_DEVICES_COUNT) {
    if (actionsCount < MULTI_DS_MAX_ACTIONS) {
      auto actionHolder = new Supla::Sensor::MultiDsActionHolder();
      actionHolder->idx = idx;
      actionHolder->action = action;
      actionHolder->client = client;
      actionHolder->condition = condition;
      actionHolder->allwaysEnabled = alwaysEnabled;
      actions[actionsCount++] = actionHolder;
    } else {
      SUPLA_LOG_WARNING("MultiDS: Actions limit reached, action skipped!");
    }

    auto sensor = sensors[idx];
    if (sensor) {
      sensor->addAction(action, client, condition, alwaysEnabled);
    }
  }
}

void MultiDsHandlerBase::setMaxDeviceCount(uint8_t count) {
  if (count > MULTI_DS_MAX_DEVICES_COUNT) {
    SUPLA_LOG_WARNING("MultiDS: Setting max count bigger then allowed"
        " - value ignored!");
  }
  maxDeviceCount = count;
}

void MultiDsHandlerBase::setChannelNumberOffset(uint8_t offset) {
  channelNumberOffset = offset;
}

void MultiDsHandlerBase::setUseSubDevices(bool useSubDevices) {
  this->useSubDevices = useSubDevices;
}

void MultiDsHandlerBase::setPairingTimeout(uint8_t timeout) {
  this->pairingTimeout = timeout;
}

void MultiDsHandlerBase::disableSensorsChannelState() {
  channelStateDisabled = true;
}

void MultiDsHandlerBase::searchForFirstSensorDuringInitialization() {
  searchFirstDevice = true;
}

void MultiDsHandlerBase::MultiDsHandlerBase::notifySrpcAboutParingEnd(
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

void MultiDsHandlerBase::initialSensorSearch() {
  SUPLA_LOG_DEBUG("MultiDS: Trying to find first thermometer sensor");

  int deviceCount = refreshSensorsCount();
  if (deviceCount == 0) {
    SUPLA_LOG_WARNING("MultiDS: Initial search end up with no thermometer");
    return;
  }

  DeviceAddress address;
  if (!getSensorAddress(address, 0)) {
    SUPLA_LOG_ERROR("MultiDS: Initial search found theremometer but could "
                    "not get it address");
    return;
  }

  auto newDevice = addDevice(address);
  if (newDevice == nullptr) {
    SUPLA_LOG_ERROR("MultiDS: Adding initial device failed!");
    return;
  }
}

void MultiDsHandlerBase::addressToString(char *buffor, uint8_t bufforLength,
                                         uint8_t *address) {
  snprintf(
      buffor,
      bufforLength,
      "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
      address[0],
      address[1],
      address[2],
      address[3],
      address[4],
      address[5],
      address[6],
      address[7]);
}
