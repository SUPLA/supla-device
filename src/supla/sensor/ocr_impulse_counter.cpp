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

#include "ocr_impulse_counter.h"

#include <supla/log_wrapper.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/network/network.h>
#include <supla/clock/clock.h>
#include <supla/device/register_device.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

// Not supported on Arduino Mega
#ifndef ARDUINO_ARCH_AVR

#define OCR_MIN_PHOTO_INTERVAL_SEC (60*60)
#define OCR_DEFAULT_PHOTO_INTERVAL_SEC OCR_MIN_PHOTO_INTERVAL_SEC
#define OCR_DEFAULT_LIGHTING_LEVEL 50
#define OCR_DEFAULT_RESULT_FETCH_INTERVAL_SEC 10

using Supla::Sensor::OcrImpulseCounter;

OcrImpulseCounter::OcrImpulseCounter() {
  channel.setType(SUPLA_CHANNELTYPE_IMPULSE_COUNTER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_OCR);
  addAvailableLightingMode(OCR_LIGHTING_MODE_OFF | OCR_LIGHTING_MODE_ALWAYS_ON |
                           OCR_LIGHTING_MODE_AUTO);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_IC_WATER_METER);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_OCR);
  clearOcrConfig();
}

OcrImpulseCounter::~OcrImpulseCounter() {
}

void OcrImpulseCounter::onInit() {
}

void OcrImpulseCounter::clearOcrConfig() {
  memset(&ocrConfig, 0, sizeof(ocrConfig));
  ocrConfig.AvailableLightingModes = availableLightingModes;
  fixOcrLightingMode();
  ocrConfig.PhotoIntervalSec = OCR_DEFAULT_PHOTO_INTERVAL_SEC;
  ocrConfig.LightingLevel = OCR_DEFAULT_LIGHTING_LEVEL;
}

void OcrImpulseCounter::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  Supla::ElementWithChannelActions::onRegistered(suplaSrpc);
  clearOcrConfig();
}

int OcrImpulseCounter::handleCalcfgFromServer(
    TSD_DeviceCalCfgRequest *request) {
  int result = Supla::Sensor::VirtualImpulseCounter::handleCalcfgFromServer(
      request);
  if (result != SUPLA_CALCFG_RESULT_NOT_SUPPORTED) {
    return result;
  }

  if (request) {
    if (request->Command == SUPLA_CALCFG_CMD_TAKE_OCR_PHOTO) {
      if (!request->SuperUserAuthorized) {
        return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
      }
      SUPLA_LOG_INFO("OcrIC[%d] - CALCFG take OCR photo received",
                     channel.getChannelNumber());
      lastPhotoTakeTimestamp = 0;
      return SUPLA_CALCFG_RESULT_DONE;
    }
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

Supla::ApplyConfigResult OcrImpulseCounter::applyChannelConfig(
    TSD_ChannelConfig *result, bool) {
  if (result->ConfigSize == 0) {
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }

  switch (result->ConfigType) {
    case SUPLA_CONFIG_TYPE_OCR: {
      lastPhotoTakeTimestamp = 0;
      if (result->ConfigSize < sizeof(ocrConfig)) {
        return Supla::ApplyConfigResult::DataError;
      }

      memcpy(&ocrConfig, result->Config, sizeof(ocrConfig));
      ocrConfig.AuthKey[sizeof(ocrConfig.AuthKey) - 1] = '\0';
      ocrConfig.Host[sizeof(ocrConfig.Host) - 1] = '\0';
      bool ocrConfigReceived = true;
      SUPLA_LOG_DEBUG("OcrIC: OCR config:");
      SUPLA_LOG_DEBUG("    AuthKey: %s", ocrConfig.AuthKey);
      SUPLA_LOG_DEBUG("    Host: %s", ocrConfig.Host);
      SUPLA_LOG_DEBUG("    PhotoIntervalSec: %d", ocrConfig.PhotoIntervalSec);
      SUPLA_LOG_DEBUG("    LightingMode: %X%08X",
                      PRINTF_UINT64_HEX(ocrConfig.LightingMode));
      SUPLA_LOG_DEBUG("    LightingLevel: %d", ocrConfig.LightingLevel);
      SUPLA_LOG_DEBUG("    MaximumIncrement: %X%08X",
                      PRINTF_UINT64_HEX(ocrConfig.MaximumIncrement));
      SUPLA_LOG_DEBUG("    AvailableLightingModes: %X%08X",
                      PRINTF_UINT64_HEX(ocrConfig.AvailableLightingModes));
      if (ocrConfig.AvailableLightingModes != availableLightingModes) {
        SUPLA_LOG_DEBUG("OcrIC: invalid AvailableLightingModes");
        ocrConfig.AvailableLightingModes = availableLightingModes;
        fixOcrLightingMode();
        // change flag to false, so config will be send to server with
        // correct AvailableLightingModes
        ocrConfigReceived = false;
      }
      if (ocrConfig.PhotoIntervalSec > 0 &&
          ocrConfig.PhotoIntervalSec < OCR_MIN_PHOTO_INTERVAL_SEC) {
        SUPLA_LOG_DEBUG("OcrIC: invalid PhotoIntervalSec");
        ocrConfig.PhotoIntervalSec = OCR_DEFAULT_PHOTO_INTERVAL_SEC;
        ocrConfigReceived = false;
      }
      if (ocrConfig.LightingLevel < 1 || ocrConfig.LightingLevel > 100) {
        SUPLA_LOG_DEBUG("OcrIC: invalid LightingLevel");
        ocrConfig.LightingLevel = OCR_DEFAULT_LIGHTING_LEVEL;
        ocrConfigReceived = false;
      }

      ledTurnOnTimestamp = 0;
      if (ocrConfig.LightingMode == OCR_LIGHTING_MODE_OFF ||
          ocrConfig.LightingMode == OCR_LIGHTING_MODE_AUTO) {
        // turn off LED
        setLedState(0);
      } else if (ocrConfig.LightingMode == OCR_LIGHTING_MODE_ALWAYS_ON) {
        // turn on LED
        setLedState(ocrConfig.LightingLevel);
      }

      return (ocrConfigReceived
                  ? Supla::ApplyConfigResult::Success
                  : Supla::ApplyConfigResult::SetChannelConfigNeeded);
    }
  }

  return Supla::ApplyConfigResult::NotSupported;
}

void OcrImpulseCounter::fixOcrLightingMode() {
  if ((ocrConfig.AvailableLightingModes & ocrConfig.LightingMode) == 0) {
    if (ocrConfig.AvailableLightingModes == 0) {
      ocrConfig.LightingMode = 0;
    } else {
      for (int i = 0; i < 64; i++) {
        // set first available mode
        if (ocrConfig.AvailableLightingModes & (1 << i)) {
          ocrConfig.LightingMode = 1 << i;
          break;
        }
      }
    }
  }
}

void OcrImpulseCounter::fillChannelConfig(void *channelConfig,
                                          int *size,
                                          uint8_t configType) {
  if (size && channelConfig) {
    if (configType == SUPLA_CONFIG_TYPE_DEFAULT) {
      // init default impulse counter config with 1000 impulses per unit
      *size = sizeof(TChannelConfig_ImpulseCounter);
      TChannelConfig_ImpulseCounter *config =
        reinterpret_cast<TChannelConfig_ImpulseCounter *>(channelConfig);
      memset(config, 0, sizeof(TChannelConfig_ImpulseCounter));
      config->ImpulsesPerUnit = 1000;
    } else if (configType == SUPLA_CONFIG_TYPE_OCR) {
      *size = sizeof(ocrConfig);
      memcpy(channelConfig, &ocrConfig, *size);
    }
  }
}

void OcrImpulseCounter::addAvailableLightingMode(uint64_t mode) {
  availableLightingModes |= mode;
  ocrConfig.AvailableLightingModes = availableLightingModes;
}

void OcrImpulseCounter::onSaveState() {
  Supla::Storage::WriteState((unsigned char *)&lastCorrectOcrReading,
                             sizeof(lastCorrectOcrReading));
  Supla::Storage::WriteState((unsigned char *)&lastCorrectOcrReadingTimestamp,
                             sizeof(lastCorrectOcrReadingTimestamp));
}

void OcrImpulseCounter::onLoadState() {
  uint64_t data = {};
  time_t timestamp = {};
  if (Supla::Storage::ReadState((unsigned char *)&data, sizeof(data))) {
    lastCorrectOcrReading = data;
  }
  if (Supla::Storage::ReadState((unsigned char *)&timestamp,
                                sizeof(timestamp))) {
    lastCorrectOcrReadingTimestamp = timestamp;
  }
  SUPLA_LOG_DEBUG(
      "OcrIC: lastCorrectOcrReading: %d, lastCorrectOcrReadingTimestamp: %d",
      lastCorrectOcrReading,
      lastCorrectOcrReadingTimestamp);
  resetCounter();
}

bool OcrImpulseCounter::iterateConnected() {
  auto result = VirtualImpulseCounter::iterateConnected();
  bool photoTaken = false;

  if (receivedConfigTypes.isSet(SUPLA_CONFIG_TYPE_OCR) &&
      ocrConfig.PhotoIntervalSec >= OCR_MIN_PHOTO_INTERVAL_SEC) {
    if (lastPhotoTakeTimestamp == 0 || millis() - lastPhotoTakeTimestamp >=
                                           ocrConfig.PhotoIntervalSec * 1000) {
      if (handleLedStateBeforePhoto()) {
        if (takePhoto()) {
          photosCount++;
          handleLedStateAfterPhoto();
          lastPhotoTakeTimestamp = millis();
          lastOcrInteractionTimestamp = lastPhotoTakeTimestamp;
          char url[500] = {};
          generateUrl(url, 500);
          SUPLA_LOG_DEBUG("Ocr url: %s", url);
          char reply[2500] = {};
          char cropSettings[100] = {};
          if (testMode) {
            auto cfg = Supla::Storage::ConfigInstance();
            if (cfg) {
              cfg->getString("ocr_crop", cropSettings, sizeof(cropSettings));
            }
          }
          if (sendPhotoToOcrServer(
                  url, ocrConfig.AuthKey, reply, sizeof(reply), cropSettings)) {
            lastUUIDToCheck[0] = '\0';
            parseStatus(reply, sizeof(reply));
          } else if (testMode) {
            if (factoryTester) {
              testMode = false;
              factoryTester->setTestFailed(102);
            }
          }
          releasePhotoResource();
          photoTaken = true;
        } else {
          handleLedStateAfterPhoto();
          SUPLA_LOG_WARNING("OcrIC: takePhoto failed");
          lastPhotoTakeTimestamp += 15 * 1000;
          if (factoryTester) {
            testMode = false;
            factoryTester->setTestFailed(101);
          }
        }
      }
    }
  }

  if (!photoTaken && lastPhotoTakeTimestamp != 0 &&
      millis() - lastOcrInteractionTimestamp >
          OCR_DEFAULT_RESULT_FETCH_INTERVAL_SEC * 1000 &&
      lastUUIDToCheck[0] != '\0' && lastUUIDToCheck[36] == '\0') {
    lastOcrInteractionTimestamp = millis();
    char url[500] = {};
    generateUrl(url, 500, lastUUIDToCheck);
    SUPLA_LOG_DEBUG("Ocr url: %s", url);
    char reply[2500] = {};
    if (getStatusFromOcrServer(url, ocrConfig.AuthKey, reply, sizeof(reply))) {
      parseStatus(reply, sizeof(reply));
    } else {
      SUPLA_LOG_WARNING("OcrIC: getStatusFromOcrServer failed");
    }
  }

  return result;
}

void OcrImpulseCounter::releasePhotoResource() {
  photoDataBuffer = nullptr;
  photoDataSize = 0;
  releasePhoto();
}


void OcrImpulseCounter::parseStatus(const char *status, int size) {
  if (!status || size == 0) {
    SUPLA_LOG_WARNING("OcrIC: parseStatus failed - empty status");
    return;
  }

// Example response:
// {
//  "id":"86e2e33b-0dda-443f-8c7a-e8cccfd7cf7d",
//  "deviceGuid":"543490A7-21BD-2652-508D-ECEFA1ADC496",
//  "channelNo":0,
//  "createdAt":"2024-08-01T11:34:53+00:00",
//  "processedAt":null,
//  "resultMeasurement":0,
//  "processingTimeMs":null
// }

  // get id:
  const char *idStart = strstr(status, "\"id\":\"");
  if (!idStart) {
    SUPLA_LOG_WARNING("OcrIC: parseStatus failed - missing id");
    return;
  }
  idStart += 6;
  const char *idEnd = strstr(idStart, "\"");
  if (!idEnd) {
    SUPLA_LOG_WARNING("OcrIC: parseStatus failed - missing id end");
    return;
  }
  size_t idSize = idEnd - idStart;
  if (idSize > sizeof(lastUUIDToCheck) - 1) {
    SUPLA_LOG_WARNING(
        "OcrIC: parseStatus failed - id too long, %d > %d",
        idSize,
        sizeof(lastUUIDToCheck) - 1);
    return;
  }
  strncpy(lastUUIDToCheck, idStart, idEnd - idStart);
  SUPLA_LOG_DEBUG("OcrIC: parseStatus - id: %s", lastUUIDToCheck);

  // check if processedAt is not null:
  const char *processedAtStart = strstr(status, "\"processedAt\":");
  if (!processedAtStart) {
    SUPLA_LOG_WARNING(
        "OcrIC: parseStatus failed - missing processedAt");
    return;
  }
  processedAtStart += 14;
  if (strncmp(processedAtStart, "null", 4) == 0) {
    SUPLA_LOG_DEBUG(
        "OcrIC: parseStatus - processedAt is null, result not "
        "ready yet");
    return;
  }

  // When processedAt is not null, we'll parse current result and stop
  // checking processing result from server.
  stopResultCheck();

  // check if measurementValid is "true"
  bool measurementValid = false;
  if (!testMode) {
    const char *measurementValidStart = strstr(status, "\"measurementValid\":");
    if (!measurementValidStart) {
      SUPLA_LOG_WARNING(
          "OcrIC: parseStatus failed - missing measurementValid");
      return;
    }
    measurementValidStart += 19;
    if (strncmp(measurementValidStart, "true", 4) != 0) {
      SUPLA_LOG_WARNING(
          "OcrIC: parseStatus failed - measurementValid is not true");
    } else {
      measurementValid = true;
    }
  }

  // get resultMeasurement as int
  const char *resultMeasurementTagStart =
      strstr(status, "\"resultMeasurement\":");
  if (!resultMeasurementTagStart) {
    SUPLA_LOG_WARNING(
        "OcrIC: parseStatus failed - missing resultMeasurement");
    return;
  }
  const char *resultMeasurementStart = resultMeasurementTagStart + 20;
  const char *resultMeasurementEnd =
      strstr(status + (resultMeasurementTagStart - status), ",\"");
  if (!resultMeasurementEnd) {
    resultMeasurementEnd =
        strstr(status + (resultMeasurementTagStart - status), "}");
  }
  if (!resultMeasurementEnd) {
    SUPLA_LOG_WARNING(
        "OcrIC: parseStatus failed - missing resultMeasurement end");
    return;
  }
  if (resultMeasurementEnd - resultMeasurementStart > 100) {
    SUPLA_LOG_WARNING(
        "OcrIC: parseStatus failed - resultMeasurement too long, %d > 100",
        resultMeasurementEnd - resultMeasurementStart);
    return;
  }
  char buf[100] = {};
  strncpy(buf,
          resultMeasurementStart,
          resultMeasurementEnd - resultMeasurementStart);
  buf[resultMeasurementEnd - resultMeasurementStart] = '\0';
  SUPLA_LOG_DEBUG("OcrIC: parseStatus - resultMeasurement: %s",
                  buf);
  uint64_t resultMeasurement = strtoull(buf, nullptr, 10);
  if (resultMeasurement == 0) {
    SUPLA_LOG_WARNING(
        "OcrIC: parseStatus failed - resultMeasurement = 0");
  }

  if (!measurementValid && !testMode) {
    resultMeasurement = 0;
  }

  bool setNewCounterValue = false;
  time_t now = Supla::Clock::GetTimeStamp();
  // There is no previous counter value available, or user reset the counter,
  // so we just set current reading as a new counter value
  if (lastCorrectOcrReading == resultMeasurement) {
    SUPLA_LOG_DEBUG("OcrIC: new counter value %u == previous value %u",
                    static_cast<uint32_t>(resultMeasurement),
                    static_cast<uint32_t>(lastCorrectOcrReading));
  } else if (lastCorrectOcrReading == 0) {
    SUPLA_LOG_DEBUG("OcrIC: new counter value %u (no previous value)",
                    static_cast<uint32_t>(resultMeasurement));
    setNewCounterValue = true;
  } else if (resultMeasurement == 0) {
    SUPLA_LOG_DEBUG("OcrIC: new counter value %u",
                    static_cast<uint32_t>(resultMeasurement));
    setNewCounterValue = true;

  } else if (resultMeasurement < lastCorrectOcrReading) {
    SUPLA_LOG_DEBUG("OcrIC: new counter value %u < previous value %u",
                    static_cast<uint32_t>(resultMeasurement),
                    static_cast<uint32_t>(lastCorrectOcrReading));
  } else {
    SUPLA_LOG_DEBUG("OcrIC: new counter value %u",
        static_cast<uint32_t>(resultMeasurement));
    setNewCounterValue = true;
  }

  if (setNewCounterValue) {
    lastCorrectOcrReading = resultMeasurement;
    if (lastCorrectOcrReadingTimestamp < now) {
      lastCorrectOcrReadingTimestamp = now;
    }
    setCounter(resultMeasurement);
  }
}

void OcrImpulseCounter::generateUrl(char *url,
    int urlSize,
    const char *photoUuid) const {
  char guidText[37] = {};
  Supla::RegisterDevice::fillGUIDText(guidText);
  if (testMode) {
    snprintf(url,
             urlSize,
             "https://%s/testing/%s/images%s%s",
             ocrConfig.Host,
             guidText,
             photoUuid ? "/" : "",
             photoUuid ? photoUuid : "");
  } else {
    snprintf(url,
             urlSize,
             "https://%s/devices/%s/%d/images%s%s",
             ocrConfig.Host,
             guidText,
             getChannelNumber(),
             photoUuid ? "/" : "",
             photoUuid ? photoUuid : "");
  }
}

void OcrImpulseCounter::resetCounter() {
  lastCorrectOcrReading = 0;
  lastCorrectOcrReadingTimestamp = 0;
  setCounter(0);
}

bool OcrImpulseCounter::handleLedStateBeforePhoto() {
  if (ocrConfig.LightingMode == OCR_LIGHTING_MODE_AUTO &&
      ledTurnOnTimestamp == 0) {
    SUPLA_LOG_DEBUG("OcrIC: handleLedStateBeforePhoto - turn on LED");
    setLedState(ocrConfig.LightingLevel);
    ledTurnOnTimestamp = millis();
    return false;
  }
  if (ledTurnOnTimestamp != 0) {
    if (millis() - ledTurnOnTimestamp > 2000) {
      return true;
    } else {
      return false;
    }
  }
  return true;
}

void OcrImpulseCounter::handleLedStateAfterPhoto() {
  if (ocrConfig.LightingMode != OCR_LIGHTING_MODE_ALWAYS_ON) {
    // turn off LED
    setLedState(0);
    ledTurnOnTimestamp = 0;
  }
}

void OcrImpulseCounter::stopResultCheck() {
  lastUUIDToCheck[0] = '\0';
}


void OcrImpulseCounter::onLoadConfig(SuplaDeviceClass *sdc) {
  this->sdc = sdc;
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t tmp = 0;
    if (cfg->getInt32("ocr_test", &tmp)) {
      SUPLA_LOG_INFO("OcrIc test mode enabled");
      if (!cfg->getString(
              "ocr_host", ocrConfig.Host, sizeof(ocrConfig.Host))) {
        SUPLA_LOG_ERROR("OcrIC: missing ocr_host");
        return;
      }
      SUPLA_LOG_INFO("OcrIC: ocr_host = %s", ocrConfig.Host);
      if (!cfg->getString(
              "ocr_auth", ocrConfig.AuthKey, sizeof(ocrConfig.AuthKey))) {
        SUPLA_LOG_ERROR("OcrIC: missing ocr_auth");
        return;
      }
      SUPLA_LOG_INFO("OcrIC: ocr_auth = %s", ocrConfig.AuthKey);
      char buf[100];
      if (!cfg->getString("ocr_crop", buf, sizeof(buf))) {
        SUPLA_LOG_ERROR("OcrIC: missing ocr_crop");
        return;
      }
      SUPLA_LOG_INFO("OcrIC: ocr_crop = %s", buf);

      if (!cfg->getInt32("ocr_expected", &ocrTestExpectedResult)) {
        SUPLA_LOG_ERROR("OcrIC: missing ocr_expected");
        return;
      }
      SUPLA_LOG_INFO("OcrIC: ocr_expected = %d", ocrTestExpectedResult);

      testMode = true;
    }
  }
}

bool OcrImpulseCounter::cleanupOcrTestModeConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  bool result = false;
  if (cfg) {
    if (cfg->eraseKey("ocr_test")) {
      result = true;
    }
    if (cfg->eraseKey("ocr_host")) {
      result = true;
    }
    if (cfg->eraseKey("ocr_auth")) {
      result = true;
    }
    if (cfg->eraseKey("ocr_crop")) {
      result = true;
    }
    if (cfg->eraseKey("ocr_expected")) {
      result = true;
    }
    if (result) {
      cfg->saveWithDelay(500);
    }
  }
  return result;
}

void OcrImpulseCounter::iterateAlways() {
  Supla::Sensor::VirtualImpulseCounter::iterateAlways();
  if (testMode && factoryTester) {
    if (Supla::Network::IsReady()) {
      if (testModeDelay == 0) {
        testModeDelay = millis();
        ocrConfig.LightingMode = OCR_LIGHTING_MODE_OFF;
        ocrConfig.LightingLevel = 1;
        return;
      }

      if (millis() - testModeDelay > 2000) {
        receivedConfigTypes.set(SUPLA_CONFIG_TYPE_OCR);
        if (photosCount == 0) {
          resetCounter();
        }
        iterateConnected();
        // Step 1 - do photo in dark
        if (photosCount == 1 && ocrConfig.LightingLevel == 1) {
          if (channel.getValueInt64() != 0) {
            // If value changed after photo, fail test.  It should be 0, becuase
            // photo in dark doesn't contain any numbers.
            testMode = false;
            factoryTester->setTestFailed(100);
          } else {
            // Step 1 successful. Enable LED auto mode.
            ocrConfig.LightingMode = OCR_LIGHTING_MODE_AUTO;
            ocrConfig.LightingLevel = 100;
            lastPhotoTakeTimestamp = 0;
          }
        } else if (photosCount == 2) {
          // Step 2 - do photo in light
          if (channel.getValueInt64() !=
              static_cast<uint64_t>(ocrTestExpectedResult)) {
            // Step 2 failed. Fail test.
            SUPLA_LOG_ERROR("OcrIC: OCR test failed: expected %d, got %d",
                            ocrTestExpectedResult,
                            static_cast<int32_t>(channel.getValueInt64()));
            ocrConfig.LightingLevel = OCR_DEFAULT_LIGHTING_LEVEL;
            testMode = false;
            factoryTester->setTestFailed(101);
          } else {
            factoryTester->waitForConfigButtonPress();
          }
          testMode = false;
        }
      }
    }
  }
}

void OcrImpulseCounter::setFactoryTester(Supla::Device::FactoryTest *tester) {
  factoryTester = tester;
}

#endif  // ARDUINO_ARCH_AVR
