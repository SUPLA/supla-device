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

#include <supla/device/device_mode.h>
#include <supla/device/auto_update_policy.h>
#include <stddef.h>
#include <stdint.h>

#define SUPLA_CONFIG_MAX_KEY_SIZE 16

#define MAX_SSID_SIZE          33  // actual SSID should be at most 32 bytes
                                   // but we add here extra byte for null
                                   // termination
#define MAX_WIFI_PASSWORD_SIZE 64
#define MQTT_CLIENTID_MAX_SIZE 23
#define MQTT_USERNAME_MAX_SIZE 256
#define MQTT_PASSWORD_MAX_SIZE 256
#define SUPLA_AES_KEY_SIZE     32
#define SUPLA_CFG_MODE_SALT_SIZE 16
#define SUPLA_CFG_MODE_PASSWORD_SIZE 32

namespace Supla {

#pragma pack(push, 1)
struct SaltPassword {
  uint8_t salt[SUPLA_CFG_MODE_SALT_SIZE] = {};
  uint8_t passwordSha[SUPLA_CFG_MODE_PASSWORD_SIZE] = {};

  void copySalt(const SaltPassword& other);
  bool isSaltEmpty() const { return salt[0] == 0; }
  bool operator==(const SaltPassword& other) const;
  bool isPasswordStrong(const char *password) const;
  void clear();
};
#pragma pack(pop)


class Config {
 public:
  Config();
  virtual ~Config();
  virtual bool init() = 0;
  virtual void removeAll() = 0;
  virtual bool isMinimalConfigReady(bool showLogs = true);
  virtual bool isConfigModeSupported();
  virtual bool isEncryptionEnabled();

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
  virtual bool eraseKey(const char* key) = 0;

  static void generateKey(char *, int, const char *);

  virtual void commit();
  virtual void saveWithDelay(uint16_t delayMs);
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
  virtual bool isSwUpdateSkipCert();
  virtual bool isSwUpdateBeta();
  virtual bool setSwUpdateSkipCert(bool skipCert);
  virtual bool setSwUpdateServer(const char* url);
  virtual bool setSwUpdateBeta(bool enabled);
  virtual bool getCustomCA(char* result, int maxSize);
  virtual int getCustomCASize();
  virtual bool setCustomCA(const char* customCA);
  virtual bool getAESKey(uint8_t* result);

  static void generateSaltPassword(const char* password,
                                   Supla::SaltPassword* result);
  virtual bool setCfgModeSaltPassword(const Supla::SaltPassword& saltPassword);
  virtual bool getCfgModeSaltPassword(Supla::SaltPassword* result);

  /**
   * Returns current automatic firmware update policy
   *
   * @return current automatic firmware update policy
   */
  Supla::AutoUpdatePolicy getAutoUpdatePolicy();

  /**
   * Sets automatic firmware update policy
   *
   * @param policy
   */
  void setAutoUpdatePolicy(Supla::AutoUpdatePolicy policy);

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

  virtual bool setChannelConfigChangeFlag(int channelNo, int configType = 0);
  virtual bool clearChannelConfigChangeFlag(int channelNo, int configType = 0);
  virtual bool isChannelConfigChangeFlagSet(int channelNo, int configType = 0);

  /**
   * Returns channel function stored in config
   *
   * @param channelNo channel number (should be >= 0)
   *
   * @return channel function (as defined in proto.h) or -1 if not found
   */
  int32_t getChannelFunction(int channelNo);

  /**
   * Stores channel function in config
   *
   * @param channelNo channel number (should be >= 0)
   * @param channelFunction channel function (as defined in proto.h)
   *
   * @return true on success
   */
  bool setChannelFunction(int channelNo, int32_t channelFunction);

  bool getInitResult() const;

 protected:
  virtual int getBlobSize(const char* key) = 0;

  uint32_t saveDelayTimestamp = 0;
  uint32_t deviceConfigUpdateDelayTimestamp = 0;
  uint16_t saveDelayMs = 0;
  int8_t deviceConfigChangeFlag = -1;
  bool initResult = false;
};
};  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_CONFIG_H_
