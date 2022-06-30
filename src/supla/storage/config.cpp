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

/*
 * Default Config implementation assumes that values are stored in key-value
 * structures. Device config parameters may be overriden and stored in simpler
 * (and shorter) way, however for Element Config it is still required to
 * provide some key-value based interface.
 */

#include <string.h>
#include <supla-common/proto.h>
#include <supla/device/sw_update.h>

#include "config.h"

namespace Supla {

Config::Config() {
  Storage::SetConfigInstance(this);
}

Config::~Config() {
  Storage::SetConfigInstance(nullptr);
}

bool Config::getWiFiSSID(char* result) {
  return getString("wifissid", result, MAX_SSID_SIZE);
}

bool Config::getWiFiPassword(char* result) {
  return getString("wifipasswd", result, MAX_WIFI_PASSWORD_SIZE);
}

bool Config::getAltWiFiSSID(char* result) {
  return getString("altwifissid", result, MAX_SSID_SIZE);
}

bool Config::getAltWiFiPassword(char* result) {
  return getString("altwifipasswd", result, MAX_WIFI_PASSWORD_SIZE);
}

bool Config::getDeviceName(char* result) {
  return getString("devicename", result, SUPLA_DEVICE_NAME_MAXSIZE);
}

bool Config::isSuplaCommProtocolEnabled() {
  // by default Supla communication protocol is enabled
  int8_t result = 1;
  getInt8("suplacommproto", &result);
  return result == 1;
}

bool Config::isMqttCommProtocolEnabled() {
  // by default MQTT communication protocol is enabled
  int8_t result = 0;
  getInt8("mqttcommproto", &result);
  return result == 1;
}

bool Config::setMqttTlsEnabled(bool enabled) {
  int8_t value = (enabled ? 1 : 0);
  return setInt8("mqtttls", value);
}

bool Config::isMqttTlsEnabled() {
  int8_t result = 0;
  getInt8("mqtttls", &result);
  return result == 1;
}

bool Config::setMqttAuthEnabled(bool enabled) {
  int8_t value = (enabled ? 1 : 0);
  return setInt8("mqttauth", value);
}

bool Config::isMqttAuthEnabled() {
  int8_t result = 1;
  getInt8("mqttauth", &result);
  return result == 1;
}

bool Config::setMqttRetainEnabled(bool enabled) {
  int8_t value = (enabled ? 1 : 0);
  return setInt8("mqttretain", value);
}

bool Config::isMqttRetainEnabled() {
  int8_t result = 1;
  getInt8("mqttretain", &result);
  return result == 1;
}

enum DeviceMode Config::getDeviceMode() {
  int32_t result = 0;
  if (getInt32("devicemode", &result)) {
    switch (result) {
      case 0:
        return DEVICE_MODE_NOT_SET;
      case 1:
        return DEVICE_MODE_TEST;
      case 2:
        return DEVICE_MODE_NORMAL;
      case 3:
        return DEVICE_MODE_CONFIG;
      case 4:
        return DEVICE_MODE_SW_UPDATE;
      default:
        return DEVICE_MODE_NOT_SET;
    }
  } else {
    return DEVICE_MODE_NOT_SET;
  }
}

bool Config::getSuplaServer(char* result) {
  return getString("suplaserver", result, SUPLA_SERVER_NAME_MAXSIZE);
}

int32_t Config::getSuplaServerPort() {
  int32_t result = -1;
  getInt32("suplaport", &result);
  if (result <= 0 || result > 65536) {
    result = 2016;
  }

  return result;
}

bool Config::getEmail(char* result) {
  return getString("email", result, SUPLA_EMAIL_MAXSIZE);
}

bool Config::getGUID(char* result) {
  return getBlob("guid", result, SUPLA_GUID_SIZE);
}

bool Config::getAuthKey(char* result) {
  return getBlob("authkey", result, SUPLA_AUTHKEY_SIZE);
}

bool Config::getMqttServer(char* result) {
  return getString("mqttserver", result, SUPLA_SERVER_NAME_MAXSIZE);
}

int32_t Config::getMqttServerPort() {
  int32_t result = -1;
  getInt32("mqttport", &result);
  if (result <= 0 || result > 65536) {
    result = 1883;
  }
  return result;
}

bool Config::getMqttUser(char* result) {
  return getString("mqttuser", result, MQTT_CLIENTID_MAX_SIZE);
}

bool Config::getMqttPassword(char* result) {
  return getString("mqttpasswd", result, MQTT_PASSWORD_MAX_SIZE);
}

int32_t Config::getMqttQos() {
  int32_t result = -1;
  getInt32("mqttqos", &result);
  if (result < 0) {
    result = 0;
  }
  return result;
}

int32_t Config::getMqttPoolPublicationDelay() {
  int32_t result = -1;
  getInt32("mqttpooldelay", &result);
  if (result < 0) {
    result = 0;
  }
  return result;
}

bool Config::setWiFiSSID(const char* ssid) {
  if (strlen(ssid) > MAX_SSID_SIZE - 1) {
    return false;
  }
  return setString("wifissid", ssid);
}

bool Config::setWiFiPassword(const char* password) {
  if (strlen(password) > MAX_WIFI_PASSWORD_SIZE - 1) {
    return false;
  }
  return setString("wifipasswd", password);
}

bool Config::setAltWiFiSSID(const char* ssid) {
  if (strlen(ssid) > MAX_SSID_SIZE - 1) {
    return false;
  }
  return setString("altwifissid", ssid);
}

bool Config::setAltWiFiPassword(const char* password) {
  if (strlen(password) > MAX_WIFI_PASSWORD_SIZE - 1) {
    return false;
  }
  return setString("altwifipasswd", password);
}

bool Config::setDeviceName(const char* name) {
  if (strlen(name) > SUPLA_DEVICE_NAME_MAXSIZE - 1) {
    return false;
  }
  return setString("devicename", name);
}

bool Config::setSuplaCommProtocolEnabled(bool enabled) {
  int8_t value = (enabled ? 1 : 0);
  return setInt8("suplacommproto", value);
}

bool Config::setMqttCommProtocolEnabled(bool enabled) {
  int8_t value = (enabled ? 1 : 0);
  return setInt8("mqttcommproto", value);
}

bool Config::setDeviceMode(enum DeviceMode mode) {
  int32_t value = 0;
  switch (mode) {
    case DEVICE_MODE_TEST:
      value = 1;
      break;
    case DEVICE_MODE_NORMAL:
      value = 2;
      break;
    case DEVICE_MODE_CONFIG:
      value = 3;
      break;
    case DEVICE_MODE_SW_UPDATE:
      value = 4;
      break;
    default:
      value = 0;
  }
  return setInt32("devicemode", value);
}

bool Config::setSuplaServer(const char* server) {
  if (strlen(server) > SUPLA_SERVER_NAME_MAXSIZE - 1) {
    return false;
  }
  return setString("suplaserver", server);
}

bool Config::setSuplaServerPort(int32_t port) {
  if (port <= 0 || port > 65536) {
    port = 2016;
  }
  return setInt32("suplaport", port);
}

bool Config::setEmail(const char* email) {
  if (strlen(email) > SUPLA_EMAIL_MAXSIZE - 1) {
    return false;
  }
  return setString("email", email);
}

bool Config::setGUID(const char* guid) {
  return setBlob("guid", guid, SUPLA_GUID_SIZE);
}

bool Config::setAuthKey(const char* authkey) {
  return setBlob("authkey", authkey, SUPLA_AUTHKEY_SIZE);
}

bool Config::setMqttServer(const char* server) {
  if (strlen(server) > SUPLA_SERVER_NAME_MAXSIZE - 1) {
    return false;
  }
  return setString("mqttserver", server);
}

bool Config::setMqttServerPort(int32_t port) {
  if (port <= 0 || port > 65536) {
    port = 1883;
  }
  return setInt32("mqttport", port);
}

bool Config::setMqttUser(const char* user) {
  if (strlen(user) > MQTT_CLIENTID_MAX_SIZE - 1) {
    return false;
  }
  return setString("mqttuser", user);
}

bool Config::setMqttPassword(const char* password) {
  if (strlen(password) > MQTT_PASSWORD_MAX_SIZE - 1) {
    return false;
  }
  return setString("mqttpasswd", password);
}

bool Config::setMqttQos(int32_t qos) {
  if (qos < 0) {
    qos = 0;
  } else if (qos > 2) {
    qos = 2;
  }
  return setInt32("mqttqos", qos);
}

bool Config::setMqttPoolPublicationDelay(int32_t poolDelay) {
  if (poolDelay < 0) {
    poolDelay = 0;
  } else if (poolDelay > 3600) {
    poolDelay = 3600;
  }
  return setInt32("mqttpooldelay", poolDelay);
}

bool Config::setMqttPrefix(const char* prefix) {
  if (strlen(prefix) > 49 - 1) {
    return false;
  }
  return setString("mqttprefix", prefix);
}

bool Config::getMqttPrefix(char* result) {
  return getString("mqttprefix", result, 49);
}

void Config::commit() {
  return;
}

bool Config::generateGuidAndAuthkey() {
  return false;
}

bool Config::getSwUpdateServer(char* url) {
  return getString("swupdateurl", url, SUPLA_MAX_URL_LENGTH);
}

bool Config::isSwUpdateBeta() {
  // by default beta sw update is disabled
  int8_t result = 0;
  getInt8("swupdatebeta", &result);
  return result == 1;
}

bool Config::setSwUpdateServer(const char* url) {
  if (strlen(url) > SUPLA_MAX_URL_LENGTH - 1) {
    return false;
  }
  return setString("swupdateurl", url);
}

bool Config::setSwUpdateBeta(bool enabled) {
  int8_t value = (enabled ? 1 : 0);
  return setInt8("swupdatebeta", value);
}

bool Config::getCustomCA(char* result, int size) {
  return getString("custom_ca", result, size);
}

int Config::getCustomCASize() {
  return getStringSize("custom_ca");
}

bool Config::setCustomCA(const char* customCA) {
  return setString("custom_ca", customCA);
}
}  // namespace Supla
