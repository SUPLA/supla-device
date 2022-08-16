/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "mqtt.h"
#include <supla/storage/config.h>
#include <supla/log_wrapper.h>
#include <SuplaDevice.h>

Supla::Protocol::Mqtt::Mqtt(SuplaDeviceClass *sdc) :
  Supla::Protocol::ProtocolLayer(sdc) {
}

void Supla::Protocol::Mqtt::onInit() {
}

bool Supla::Protocol::Mqtt::onLoadConfig() {
  bool configComplete = true;
  // MQTT protocol specific config
  auto cfg = Supla::Storage::ConfigInstance();

  if (cfg->isMqttCommProtocolEnabled()) {
    // TODO(klew): add MQTT config
    sdc->addLastStateLog("MQTT protocol is not supported yet");
    configComplete = false;
    SUPLA_LOG_ERROR("MQTT support not implemented yet");
  }

  return configComplete;
}

void Supla::Protocol::Mqtt::disconnect() {
}

void Supla::Protocol::Mqtt::iterate(uint64_t _millis) {
  (void)(_millis);
  return;
}

bool Supla::Protocol::Mqtt::isNetworkRestartRequested() {
  return false;
}

uint32_t Supla::Protocol::Mqtt::getConnectionFailTime() {
  return 0;
}
