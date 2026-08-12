// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "multi_ds_handler_base.h"

#include <stdio.h>
#include <string.h>

#include <supla/auto_lock.h>
#include <supla/log_wrapper.h>
#include <supla-common/proto.h>
#include <supla/storage/config.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/device/register_device.h>
#include <supla/storage/config_tags.h>
#include <supla/channel.h>
#include <supla/time.h>

#define DS_HANDLER_ADDRESS_LENGTH 24
#define DS_NAME "DS18B20"

using Supla::Sensor::MultiDsHandlerBase;

MultiDsHandlerBase::MultiDsHandlerBase(
    SuplaDeviceClass *sdc,
    uint8_t pin): sdc(sdc), pin(pin) {
}

MultiDsHandlerBase::~MultiDsHandlerBase() {
  for (int i = 0; i < MULTI_DS_MAX_DEVICES_COUNT; i++) {
    auto sensor = sensors[i];
    if (sensor != nullptr) {
      delete sensor;
      sensors[i] = nullptr;
    }
  }
}

void MultiDsHandlerBase::onLoadConfig(SuplaDeviceClass *) {
  auto config = Supla::Storage::ConfigInstance();
  if (!config) {
    return;
  }

  anySensorLoaded = false;
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  // The numeric part of the key is the persisted SubDeviceId. It is not a
  // runtime sensor slot, so scan the complete protocol range here.
  for (int subDeviceId = 1; subDeviceId <= UINT8_MAX; subDeviceId++) {
    Supla::Config::generateKey(key, subDeviceId,
                               Supla::ConfigTag::DsSensorConfig);
    Supla::Sensor::DsSensorConfig sensorConfig = {};

    bool configExists = config->getBlob(key,
        reinterpret_cast<char *>(&sensorConfig), sizeof(sensorConfig));

    if (configExists) {
      SUPLA_LOG_DEBUG("MultiDS: Loading config for key %s", key);
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
  if (sdc) {
    // SuplaDevice may still be in static construction when a handler is
    // created. Register runtime callbacks only during normal element init.
    sdc->setChannelConflictResolver(this);
    sdc->addFlags(SUPLA_DEVICE_FLAG_CALCFG_SUBDEVICE_PAIRING);
    sdc->addFlags(SUPLA_DEVICE_FLAG_BLOCK_ADDING_CHANNELS_AFTER_DELETION);
    sdc->setSubdevicePairingHandler(this);
  }

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
        uint8_t address[8] = {};
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
      subdeviceDetails.SubDeviceId = sensor->getChannel()->getSubDeviceId();
      strncpy(subdeviceDetails.Name, DS_NAME, SUPLA_DEVICE_NAME_MAXSIZE - 1);
      addressToString(subdeviceDetails.SerialNumber,
                      SUPLA_SUBDEVICE_SERIAL_NUMBER_MAXSIZE,
                      sensor->getAddress());

      srpc->sendSubdeviceDetails(&subdeviceDetails);
      sensor->setDetailsSend(true);
      dataSend = true;
      break;
    }  // TODO(klew): move subdevice details sending/update to common class
  }

  return !dataSend;
}

Supla::Sensor::MultiDsSensor *MultiDsHandlerBase::addDevice(
    uint8_t *deviceAddress, int channelNumber, int subDeviceId) {

  bool newDevice = (subDeviceId == -1);
  int sensorSlot = findFreeSensorSlot();
  if (sensorSlot == -1) {
    SUPLA_LOG_DEBUG("MultiDS: Cannot add new device - limit exceeded!");
    return nullptr;
  }

  if (subDeviceId == -1) {
    subDeviceId = findNextFreeSubDeviceId();
  }

  if (subDeviceId <= 0 || subDeviceId > UINT8_MAX) {
    SUPLA_LOG_DEBUG("MultiDS: Cannot add new device - no free subdevice ID!");
    return nullptr;
  }

  if (channelNumber == -1) {
    if (channelNumberOffset == -1) {
      channelNumber = Supla::RegisterDevice::getNextFreeChannelNumber();
      SUPLA_LOG_DEBUG("MultiDS: Took next channel number - %d", channelNumber);
    } else {
      channelNumber = findChannelNumber(sensorSlot);
      SUPLA_LOG_DEBUG("MultiDS: Took channel number from offset range - %d",
                      channelNumber);
    }
  }

  if (channelNumber < 0 ||
      !Supla::RegisterDevice::isChannelNumberFree(channelNumber)) {
    SUPLA_LOG_DEBUG("MultiDS: Cannot add new device - no free channel!");
    return nullptr;
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

  if (channelStateDisabled) {
    sensor->disableChannelState();
  }
  if (!sensor->getChannel()->setChannelNumber(channelNumber)) {
    delete sensor;
    return nullptr;
  }
  if (newDevice) {
    sensor->onLoadConfig(sdc);
    sensor->onInit();
    sensor->saveSensorConfig();
    SUPLA_LOG_INFO("MultiDS: Successfully added a new device");
  }
  SUPLA_LOG_DEBUG("MultiDS: Device added (subId: %d, number %d)",
      subDeviceId, sensor->getChannel()->getChannelNumber());

  sensors[sensorSlot] = sensor;
  return sensor;
}

int MultiDsHandlerBase::findFreeSensorSlot() const {
  for (int i = 0; i < maxDeviceCount; i++) {
    if (sensors[i] == nullptr) {
      return i;
    }
  }
  return -1;
}

int MultiDsHandlerBase::findNextFreeSubDeviceId() const {
  for (int candidate = 1; candidate <= UINT8_MAX; candidate++) {
    bool usedByChannel = false;
    for (auto channel = Supla::Channel::Begin(); channel != nullptr;
         channel = channel->next()) {
      if (channel->getSubDeviceId() == candidate) {
        usedByChannel = true;
        break;
      }
    }

    if (!usedByChannel &&
        Supla::Element::getOwnerOfSubDeviceId(candidate) == nullptr) {
      return candidate;
    }
  }
  return -1;
}

int MultiDsHandlerBase::findChannelNumber(int sensorSlot) const {
  if (channelNumberOffset < 0) {
    return -1;
  }

  const int preferred = channelNumberOffset + sensorSlot;
  if (Supla::RegisterDevice::isChannelNumberFree(preferred)) {
    return preferred;
  }

  // Keep the offset range tied to handler capacity, not to SubDeviceId. This
  // also allows a newly paired sensor to reuse a channel freed from a slot.
  for (int offset = 0; offset < maxDeviceCount; offset++) {
    const int candidate = channelNumberOffset + offset;
    if (Supla::RegisterDevice::isChannelNumberFree(candidate)) {
      return candidate;
    }
  }
  return -1;
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
  notifySubdevicePairingStarted(pairingTimeout);
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
    notifyChannelConflictResolution(false);
    return false;
  }
  if (hasConflictInvalidType) {
    SUPLA_LOG_ERROR("MultiDS: Channel conflict - channel type mismatch. "
        "Aborting...");
    notifyChannelConflictResolution(false);
    return false;
  }
  bool handled = false;
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
      if ((channelNumber >= channelReportSize ||
          channelReport[channelNumber] == 0) &&
          !Supla::RegisterDevice::isChannelNumberFree(channelNumber)) {
        Supla::AutoLock lock(sdc->getTimerAccessMutex());

        SUPLA_LOG_DEBUG("MultiDS: Channel removed (subId: %d, number: %d)",
                        sensor->getSubDeviceId(), channelNumber);

        sensor->purgeConfig();
        delete sensor;
        sensor = nullptr;
        sensors[i] = nullptr;
        handled = true;
      }
    }
  }

  notifyChannelConflictResolution(handled);
  return handled;
}

void MultiDsHandlerBase::setMaxDeviceCount(uint8_t count) {
  if (count > MULTI_DS_MAX_DEVICES_COUNT) {
    SUPLA_LOG_WARNING("MultiDS: Setting max count bigger then allowed"
        " - value ignored!");
    return;
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

void MultiDsHandlerBase::notifySrpcAboutParingEnd(
    int pairingResult, const char *name) {
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

  result.MaximumDurationSec = pairingTimeout;
  result.NameSize = len;
  result.PairingResult = pairingResult;

  notifySubdevicePairingFinished(result);

  if (srpc) {
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

  uint8_t address[8] = {};
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
