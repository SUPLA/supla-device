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

#include "binary_base.h"

#include <supla/time.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>

using Supla::Sensor::BinaryBase;

const char BinarySensorServerInvertedLogicTag[] = "srv_invrt";

BinaryBase::BinaryBase() {
  channel.setType(SUPLA_CHANNELTYPE_SENSORNO);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
}

BinaryBase::~BinaryBase() {
}

void BinaryBase::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};

    generateKey(key, BinarySensorServerInvertedLogicTag);
    uint8_t storedServerInvertLogic = 0;
    cfg->getUInt8(key, &storedServerInvertLogic);

    setServerInvertLogic(storedServerInvertLogic > 0);

    SUPLA_LOG_INFO("Binary[%d] config serverInvertLogic %d",
                   getChannelNumber(),
                   storedServerInvertLogic);
  }
}

uint8_t BinaryBase::handleChannelConfig(TSD_ChannelConfig *newConfig,
                                      bool local) {
  (void)(local);
  SUPLA_LOG_DEBUG("Binary[%d]: processing channel config", getChannelNumber());

  if (newConfig == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  if (newConfig->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
  }

  if (newConfig->ConfigSize != sizeof(TChannelConfig_BinarySensor)) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  TChannelConfig_BinarySensor *config =
      reinterpret_cast<TChannelConfig_BinarySensor *>(newConfig);

  setServerInvertLogic(config->InvertedLogic > 0);

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, BinarySensorServerInvertedLogicTag);
    uint8_t storedServerInvertLogic = (config->InvertedLogic > 0) ? 1 : 0;
    cfg->setUInt8(key, storedServerInvertLogic);
    cfg->saveWithDelay(2000);
  }

  return SUPLA_CONFIG_RESULT_TRUE;
}


void BinaryBase::iterateAlways() {
  if (millis() - lastReadTime > readIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

Supla::Channel *BinaryBase::getChannel() {
  return &channel;
}

void BinaryBase::setServerInvertLogic(bool invertLogic) {
  channel.setServerInvertLogic(invertLogic);
}

void BinaryBase::setReadIntervalMs(uint32_t intervalMs) {
  if (intervalMs == 0) {
    intervalMs = 100;
  }
  readIntervalMs = intervalMs;
}
