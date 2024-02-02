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

#ifndef SRC_SUPLA_STORAGE_CONFIG_H_
#define SRC_SUPLA_STORAGE_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#include "storage.h"

#define SUPLA_CONFIG_MAX_KEY_SIZE 16

#define MAX_SSID_SIZE          33  // actual SSID should be at most 32 bytes
                                   // but we add here extra byte for null
                                   // termination
#define MAX_WIFI_PASSWORD_SIZE 64
#define MQTT_CLIENTID_MAX_SIZE 23
#define MQTT_USERNAME_MAX_SIZE 256
#define MQTT_PASSWORD_MAX_SIZE 256

namespace Supla {

enum DeviceMode : uint8_t {
  DEVICE_MODE_NOT_SET = 0,
  DEVICE_MODE_TEST = 1,
  DEVICE_MODE_NORMAL = 2,
  DEVICE_MODE_CONFIG = 3,
  DEVICE_MODE_SW_UPDATE = 4
};

class Config {
 public:
  Config();
  virtual ~Config();
  virtual bool init() = 0;
  virtual void removeAll() = 0;
  virtual bool isMinimalConfigReady();
  virtual bool isConfigModeSupported();

  // Override this method and setup all default value if needed
  virtual void initDefaultDeviceConfig();

  // Generic getters and setters
  virtual bool setString(const char* key, const char* value) = 0;
  virtual bool getString(const char* key, char* value, size_t maxSize) = 0;
  virtual int getStringSize(const char* key) = 0;

  virtual bool setBlob(const char* key, const char* value, size_t blobSize) = 0;
  virtual bool getBlob(const char* key, char* value, size_t blobSize) = 0;

  virtual bool getInt8(const char* key, int8_t* result) = 0;
  virtual bool getUInt8(const char* key, uint8_t* result) = 0;
  virtual bool getInt32(const char* key, int32_t* result) = 0;
  virtual bool getUInt32(const char* key, uint32_t* result) = 0;

  virtual bool setInt8(const char* key, const int8_t value) = 0;
  virtual bool setUInt8(const char* key, const uint8_t value) = 0;
  virtual bool setInt32(const char* key, const int32_t value) = 0;
  virtual bool setUInt32(const char* key, const uint32_t value) = 0;

  static void generateKey(char *, int, const char *);

  virtual void commit();
  virtual void saveWithDelay(uint32_t delayMs);
  virtual void saveIfNeeded();

  // Device generic config
  virtual bool generateGuidAndAuthkey();
  virtual bool setDeviceName(const char* name);
  virtual bool setDeviceMode(enum Supla::DeviceMode mode);
  virtual bool setGUID(const char* guid);
  virtual bool getDeviceName(char* result);
  virtual enum Supla::DeviceMode getDeviceMode();
  virtual bool getGUID(char* result);
  virtual bool getSwUpdateServer(char* url);
  virtual bool isSwUpdateBeta();
  virtual bool setSwUpdateServer(const char* url);
  virtual bool setSwUpdateBeta(bool enabled);
  virtual bool getCustomCA(char* result, int maxSize);
  virtual int getCustomCASize();
  virtual bool setCustomCA(const char* customCA);

  // Supla protocol config
  virtual bool setSuplaCommProtocolEnabled(bool enabled);
  virtual bool setSuplaServer(const char* server);
  virtual bool setSuplaServerPort(int32_t port);
  virtual bool setEmail(const char* email);
  virtual bool setAuthKey(const char* authkey);
  virtual bool isSuplaCommProtocolEnabled();
  virtual bool getSuplaServer(char* result);
  virtual int32_t getSuplaServerPort();
  virtual bool getEmail(char* result);
  virtual bool getAuthKey(char* result);

  // MQTT protocol config
  virtual bool setMqttCommProtocolEnabled(bool enabled);
  virtual bool setMqttServer(const char* server);
  virtual bool setMqttServerPort(int32_t port);
  virtual bool setMqttUser(const char* user);
  virtual bool setMqttPassword(const char* password);
  virtual bool setMqttQos(int32_t qos);
  virtual bool isMqttCommProtocolEnabled();
  virtual bool setMqttTlsEnabled(bool enabled);
  virtual bool isMqttTlsEnabled();
  virtual bool setMqttAuthEnabled(bool enabled);
  virtual bool isMqttAuthEnabled();
  virtual bool setMqttRetainEnabled(bool enabled);
  virtual bool isMqttRetainEnabled();
  virtual bool getMqttServer(char* result);
  virtual int32_t getMqttServerPort();
  virtual bool getMqttUser(char* result);
  virtual bool getMqttPassword(char* result);
  virtual int32_t getMqttQos();
  virtual bool setMqttPrefix(const char* prefix);
  virtual bool getMqttPrefix(char* result);

  // WiFi config
  virtual bool setWiFiSSID(const char* ssid);
  virtual bool setWiFiPassword(const char* password);
  virtual bool setAltWiFiSSID(const char* ssid);
  virtual bool setAltWiFiPassword(const char* password);
  virtual bool getWiFiSSID(char* result);
  virtual bool getWiFiPassword(char* result);
  virtual bool getAltWiFiSSID(char* result);
  virtual bool getAltWiFiPassword(char* result);

  virtual bool isDeviceConfigChangeFlagSet();
  virtual bool isDeviceConfigChangeReadyToSend();
  virtual bool setDeviceConfigChangeFlag();
  virtual bool clearDeviceConfigChangeFlag();

 protected:
  virtual int getBlobSize(const char* key) = 0;
  uint32_t saveDelayTimestamp = 0;
  uint32_t deviceConfigUpdateDelayTimestamp = 0;
  uint32_t saveDelayMs = 0;
  int8_t deviceConfigChangeFlag = -1;
};
};  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_CONFIG_H_
