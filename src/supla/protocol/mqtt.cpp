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
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <supla/storage/config.h>
#include <supla/log_wrapper.h>
#include <SuplaDevice.h>
#include <supla/mutex.h>
#include <supla/auto_lock.h>
#include <supla/time.h>
#include <supla/channel.h>
#include <supla/network/network.h>

Supla::Protocol::Mqtt::Mqtt(SuplaDeviceClass *sdc) :
  Supla::Protocol::ProtocolLayer(sdc) {
}

Supla::Protocol::Mqtt::~Mqtt() {
  if (prefix) {
    delete[] prefix;
    prefix = nullptr;
  }
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

bool Supla::Protocol::Mqtt::isConnectionError() {
  return error;
}

bool Supla::Protocol::Mqtt::isConnecting() {
  return connecting;
}

void Supla::Protocol::Mqtt::generateClientId(
    char result[MQTT_CLIENTID_MAX_SIZE]) {
  memset(result, 0, MQTT_CLIENTID_MAX_SIZE);

  // GUID is truncated here, because of client_id parameter limitation
  snprintf(
      result, MQTT_CLIENTID_MAX_SIZE,
      "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[0]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[1]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[2]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[3]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[4]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[5]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[6]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[7]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[8]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[9]),
      static_cast<unsigned char>(Supla::Channel::reg_dev.GUID[10]));
}

void Supla::Protocol::Mqtt::onInit() {
  auto cfg = Supla::Storage::ConfigInstance();

  char customPrefix[49] = {};
  if (cfg != nullptr) {
    cfg->getMqttPrefix(customPrefix);
  }
  int customPrefixLength = strnlen(customPrefix, 48);
  if (customPrefixLength > 0) {
    customPrefixLength++;  // add one char for '/'
  }
  char hostname[32] = {};
  sdc->generateHostname(hostname, 3);
  for (int i = 0; i < 32; i++) {
    hostname[i] = static_cast<char>(tolower(hostname[i]));
  }
  int hostnameLength = strlen(hostname);
  char suplaTopic[] = "supla/devices/";
  uint8_t mac[6] = {};
  Supla::Network::GetMacAddr(mac);
  // 1 + 6 + 1: 1 byte for -, 6 for MAC address appendix and 1 bytes for '\0'
  int length = customPrefixLength + hostnameLength
    + strlen(suplaTopic) + 1 + 6 + 1;
  if (prefix) {
    delete[] prefix;
    prefix = nullptr;
  }
  prefix = new char[length];
  if (prefix) {
    snprintf(prefix, length - 1, "%s%s%s%s",
        customPrefix,
        customPrefixLength > 0 ? "/" : "",
        suplaTopic,
        hostname);
    SUPLA_LOG_DEBUG("Mqtt: generated prefix \"%s\"", prefix);
  } else {
    SUPLA_LOG_ERROR("Mqtt: failed to generate prefix");
  }
}
