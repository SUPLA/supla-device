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
#include <string.h>

using Supla::Sensor::OcrImpulseCounter;

OcrImpulseCounter::OcrImpulseCounter() {
  channel.setType(SUPLA_CHANNELTYPE_IMPULSE_COUNTER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  clearOcrConfig();
}

bool OcrImpulseCounter::hasOcrConfig() {
  return true;
}

void OcrImpulseCounter::clearOcrConfig() {
  memset(&ocrConfig, 0, sizeof(ocrConfig));
  ocrConfigReceived = false;
  ocrConfig.AvailableLightingModes = 0;
}

uint8_t OcrImpulseCounter::applyChannelConfig(TSD_ChannelConfig *result,
                                              bool) {
  if (!result) {
    return SUPLA_CONFIG_RESULT_FALSE;
  }

  switch (result->ConfigType) {
    case SUPLA_CONFIG_TYPE_OCR: {
      ocrConfigReceived = true;
      if (result->ConfigSize >= sizeof(ocrConfig)) {
        memcpy(&ocrConfig, result->Config, sizeof(ocrConfig));
        ocrConfig.AuthKey[sizeof(ocrConfig.AuthKey) - 1] = '\0';
        ocrConfig.Host[sizeof(ocrConfig.Host) - 1] = '\0';
        ocrConfigReceived = true;
        SUPLA_LOG_DEBUG("OcrImpulseCounter: OCR config:");
        SUPLA_LOG_DEBUG("    AuthKey: %s", ocrConfig.AuthKey);
        SUPLA_LOG_DEBUG("    Host: %s", ocrConfig.Host);
        SUPLA_LOG_DEBUG("    PhotoIntervalSec: %d", ocrConfig.PhotoIntervalSec);
        SUPLA_LOG_DEBUG("    LightingMode: %d", ocrConfig.LightingMode);
        SUPLA_LOG_DEBUG("    LightingLevel: %d", ocrConfig.LightingLevel);
        SUPLA_LOG_DEBUG("    AvailableLightingModes: %d",
                        ocrConfig.AvailableLightingModes);
        return SUPLA_CONFIG_RESULT_TRUE;
      } else {
        SUPLA_LOG_WARNING(
            "OcrImpulseCounter: applyChannelConfig - invalid config size %d",
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

void OcrImpulseCounter::fillChannelConfig(void *, int *) {
  // todo
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

