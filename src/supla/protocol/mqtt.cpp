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
#include <supla/mutex.h>
#include <supla/auto_lock.h>
#include <supla/time.h>
#include <string.h>

Supla::Protocol::Mqtt::Mqtt(SuplaDeviceClass *sdc) :
  Supla::Protocol::ProtocolLayer(sdc) {
}

bool Supla::Protocol::Mqtt::onLoadConfig() {
  // MQTT protocol specific config
  auto cfg = Supla::Storage::ConfigInstance();

  if (cfg == nullptr) {
    return false;
  }

  bool configComplete = true;

  if (cfg->isMqttCommProtocolEnabled()) {
    enabled = true;
    if (!cfg->getMqttServer(server) || strlen(server) <= 0) {
      SUPLA_LOG_INFO("Config incomplete: missing MQTT server");
      configComplete = false;
    }

    useTls = cfg->isMqttTlsEnabled();
    port = cfg->getMqttServerPort();
    retain = cfg->isMqttRetainEnabled();
    qos = cfg->getMqttQos();
    useAuth = cfg->isMqttAuthEnabled();

    if (useAuth) {
      if (!cfg->getMqttUser(user) || strlen(user) <= 0) {
        SUPLA_LOG_INFO("Config incomplete: missing MQTT username");
        configComplete = false;
      }
      if (!cfg->getMqttPassword(password) || strlen(password) <= 0) {
        SUPLA_LOG_INFO("Config incomplete: missing MQTT password");
        configComplete = false;
      }
    }
  } else {
    enabled = false;
  }

  return configComplete;
}

bool Supla::Protocol::Mqtt::verifyConfig() {
  auto cfg = Supla::Storage::ConfigInstance();

  if (cfg && !cfg->isMqttCommProtocolEnabled()) {
    // skip verification of Mqtt protocol if it is disabled
    return true;
  }

  if (strlen(server) <= 0) {
    sdc->status(STATUS_UNKNOWN_SERVER_ADDRESS, "MQTT: Missing server address");
    if (sdc->getDeviceMode() != Supla::DEVICE_MODE_CONFIG) {
      return false;
    }
  }

  if (useAuth) {
    if (strlen(user) <= 0) {
      sdc->status(STATUS_MISSING_CREDENTIALS, "MQTT: Missing username");
      if (sdc->getDeviceMode() != Supla::DEVICE_MODE_CONFIG) {
        return false;
      }
    }
    if (strlen(password) <= 0) {
      sdc->status(STATUS_MISSING_CREDENTIALS, "MQTT: Missing password");
      if (sdc->getDeviceMode() != Supla::DEVICE_MODE_CONFIG) {
        return false;
      }
    }
  }
  return true;
}


bool Supla::Protocol::Mqtt::isNetworkRestartRequested() {
  return false;
}

uint32_t Supla::Protocol::Mqtt::getConnectionFailTime() {
  return 0;
}

// By default MQTT protocol is enabled.
// It can be disabled if we have config class instance and when it is
// disabled there.
bool Supla::Protocol::Mqtt::isEnabled() {
  return enabled;
}

