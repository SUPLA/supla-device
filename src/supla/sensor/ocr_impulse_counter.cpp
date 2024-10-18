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
  addAvailableLightingMode(OCR_LIGHTING_MODE_OFF | OCR_LIGHTING_MODE_ALWAYS_ON |
                           OCR_LIGHTING_MODE_AUTO);
  clearOcrConfig();
}

OcrImpulseCounter::~OcrImpulseCounter() {
}

void OcrImpulseCounter::onInit() {
}

bool OcrImpulseCounter::hasOcrConfig() {
  return true;
}

void OcrImpulseCounter::clearOcrConfig() {
  memset(&ocrConfig, 0, sizeof(ocrConfig));
  ocrConfigReceived = false;
  ocrConfig.AvailableLightingModes = availableLightingModes;
  fixOcrLightingMode();
  ocrConfig.PhotoIntervalSec = OCR_DEFAULT_PHOTO_INTERVAL_SEC;
  ocrConfig.LightingLevel = OCR_DEFAULT_LIGHTING_LEVEL;
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

uint8_t OcrImpulseCounter::applyChannelConfig(TSD_ChannelConfig *result,
                                              bool) {
  if (!result) {
    return SUPLA_CONFIG_RESULT_FALSE;
  }
  if (result->ConfigSize == 0) {
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  switch (result->ConfigType) {
    case SUPLA_CONFIG_TYPE_OCR: {
      lastPhotoTakeTimestamp = 0;
      if (result->ConfigSize >= sizeof(ocrConfig)) {
        memcpy(&ocrConfig, result->Config, sizeof(ocrConfig));
        ocrConfig.AuthKey[sizeof(ocrConfig.AuthKey) - 1] = '\0';
        ocrConfig.Host[sizeof(ocrConfig.Host) - 1] = '\0';
        ocrConfigReceived = true;
        SUPLA_LOG_DEBUG("OcrIC: OCR config:");
        SUPLA_LOG_DEBUG("    AuthKey: %s", ocrConfig.AuthKey);
        SUPLA_LOG_DEBUG("    Host: %s", ocrConfig.Host);
        SUPLA_LOG_DEBUG("    PhotoIntervalSec: %d", ocrConfig.PhotoIntervalSec);
        SUPLA_LOG_DEBUG("    LightingMode: %" PRIu64, ocrConfig.LightingMode);
        SUPLA_LOG_DEBUG("    LightingLevel: %d", ocrConfig.LightingLevel);
        SUPLA_LOG_DEBUG("    MaximumIncrement: %" PRIu64,
                        ocrConfig.MaximumIncrement);
        SUPLA_LOG_DEBUG("    AvailableLightingModes: %" PRIu64,
                        ocrConfig.AvailableLightingModes);
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

        return SUPLA_CONFIG_RESULT_TRUE;
      } else {
        SUPLA_LOG_WARNING(
            "OcrIC: applyChannelConfig - invalid config size %d",
            result->ConfigSize);
        return SUPLA_CONFIG_RESULT_DATA_ERROR;
      }
    }
    default: {
      return SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
    }
  }
  if (result->ConfigType != SUPLA_CONFIG_TYPE_OCR) {
    return SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
  }
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

void OcrImpulseCounter::fillChannelConfig(void *, int *) {
  // TODO(klew): implement
  return;
}

void OcrImpulseCounter::fillChannelOcrConfig(void *channelConfig, int *size) {
  if (channelConfig && size) {
    *size = sizeof(ocrConfig);
    memcpy(channelConfig, &ocrConfig, *size);
  }
}

bool OcrImpulseCounter::isOcrConfigMissing() {
  return !ocrConfigReceived;
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
}

bool OcrImpulseCounter::iterateConnected() {
  auto result = VirtualImpulseCounter::iterateConnected();
  bool photoTaken = false;

  if (!Supla::Clock::IsReady()) {
    return result;
  }
  if (ocrConfigReceived &&
      ocrConfig.PhotoIntervalSec >= OCR_MIN_PHOTO_INTERVAL_SEC) {
    if (lastPhotoTakeTimestamp == 0 || millis() - lastPhotoTakeTimestamp >=
                                           ocrConfig.PhotoIntervalSec * 1000) {
      if (handleLedStateBeforePhoto()) {
        if (takePhoto()) {
          lastPhotoTakeTimestamp = millis();
          lastOcrInteractionTimestamp = lastPhotoTakeTimestamp;
          char url[500] = {};
          generateUrl(url, 500);
          SUPLA_LOG_DEBUG("Ocr url: %s", url);
          char reply[500] = {};
          if (sendPhotoToOcrServer(url, ocrConfig.AuthKey, reply, 500)) {
            lastUUIDToCheck[0] = '\0';
            parseStatus(reply, 500);
          }
          releasePhotoResource();
          photoTaken = true;
        } else {
          SUPLA_LOG_WARNING("OcrIC: takePhoto failed");
          lastPhotoTakeTimestamp += 15 * 1000;
        }
        handleLedStateAfterPhoto();
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
    char reply[500] = {};
    if (getStatusFromOcrServer(url, ocrConfig.AuthKey, reply, 500)) {
      parseStatus(reply, 500);
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
    return;
  }

  lastUUIDToCheck[0] = '\0';
  bool setNewCounterValue = false;
  uint64_t maxIncrementAllowedPerPhoto = ocrConfig.MaximumIncrement;
  time_t now = Supla::Clock::GetTimeStamp();
  // There is no previous counter value available, or user reset the counter,
  // so we just set current reading as a new counter value
  if (lastCorrectOcrReading == resultMeasurement) {
    SUPLA_LOG_DEBUG("OcrIC: new counter value %" PRIu64
                    " == previous value %" PRIu64,
                    resultMeasurement,
                    lastCorrectOcrReading);
  } else if (lastCorrectOcrReading == 0 || maxIncrementAllowedPerPhoto == 0) {
    SUPLA_LOG_DEBUG("OcrIC: new counter value %" PRIu64 " (no previous value)",
                    resultMeasurement);
    setNewCounterValue = true;
  } else if (resultMeasurement < lastCorrectOcrReading) {
    SUPLA_LOG_DEBUG("OcrIC: new counter value %" PRIu64
                    " < previous value %" PRIu64,
                    resultMeasurement,
                    lastCorrectOcrReading);
  } else if (ocrConfig.PhotoIntervalSec >= OCR_MIN_PHOTO_INTERVAL_SEC) {
    time_t diff = now - lastCorrectOcrReadingTimestamp;
    int periods = (diff / ocrConfig.PhotoIntervalSec) + 1;
    uint64_t maxIncrementAllowed = maxIncrementAllowedPerPhoto * periods;
    if (maxIncrementAllowed >= maxIncrementAllowedPerPhoto &&
        resultMeasurement - lastCorrectOcrReading <= maxIncrementAllowed) {
      SUPLA_LOG_DEBUG("OcrIC: new counter value %" PRIu64 " within limits",
                      resultMeasurement);
      setNewCounterValue = true;
    } else {
      SUPLA_LOG_DEBUG("OcrIC: new counter value %" PRIu64
                      " > max increment %" PRIu64,
                      resultMeasurement,
                      maxIncrementAllowed);
    }
  }
  // TODO(klew): should we send 0 to server to indicate invalid reading?

  if (setNewCounterValue) {
    lastCorrectOcrReading = resultMeasurement;
    lastCorrectOcrReadingTimestamp = now;
    setCounter(resultMeasurement);
  }
}

void OcrImpulseCounter::generateUrl(char *url,
    int urlSize,
    const char *photoUuid) const {
  char guidText[37] = {};
  Supla::RegisterDevice::fillGUIDText(guidText);
  snprintf(url,
           urlSize,
           "https://%s/devices/%s/%d/images%s%s",
           ocrConfig.Host,
           guidText,
           getChannelNumber(),
           photoUuid ? "/" : "",
           photoUuid ? photoUuid : "");
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
    if (millis() - ledTurnOnTimestamp > 3000) {
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

#endif  // ARDUINO_ARCH_AVR
