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

#ifndef EXTRAS_TEST_DOUBLES_CONFIG_MOCK_H_
#define EXTRAS_TEST_DOUBLES_CONFIG_MOCK_H_

#include <gmock/gmock.h>
#include <supla/storage/config.h>

class ConfigMock : public Supla::Config {
 public:
  ConfigMock();
  virtual ~ConfigMock();
  MOCK_METHOD(bool, init, (), (override));
  MOCK_METHOD(void, removeAll, (), (override));
  //  MOCK_METHOD(bool, isMinimalConfigReady, (), (override));
  MOCK_METHOD(bool, isConfigModeSupported, (), (override));
  MOCK_METHOD(bool,
              setString,
              (const char* key, const char* value),
              (override));
  MOCK_METHOD(bool,
              getString,
              (const char* key, char* value, size_t maxSize),
              (override));
  MOCK_METHOD(int, getStringSize, (const char* key), (override));
  MOCK_METHOD(bool,
              setBlob,
              (const char* key, const char* value, size_t blobSize),
              (override));
  MOCK_METHOD(bool,
              getBlob,
              (const char* key, char* value, size_t blobSize),
              (override));
  MOCK_METHOD(bool, eraseKey, (const char* key), (override));
  MOCK_METHOD(int, getBlobSize, (const char* key), (override));
  MOCK_METHOD(bool, getInt8, (const char* key, int8_t* result), (override));
  MOCK_METHOD(bool, getUInt8, (const char* key, uint8_t* result), (override));
  MOCK_METHOD(bool, getInt32, (const char* key, int32_t* result), (override));
  MOCK_METHOD(bool, getUInt32, (const char* key, uint32_t* result), (override));
  MOCK_METHOD(bool, setInt8, (const char* key, const int8_t value), (override));
  MOCK_METHOD(bool,
              setUInt8,
              (const char* key, const uint8_t value),
              (override));
  MOCK_METHOD(bool,
              setInt32,
              (const char* key, const int32_t value),
              (override));
  MOCK_METHOD(bool,
              setUInt32,
              (const char* key, const uint32_t value),
              (override));
  MOCK_METHOD(void, commit, (), (override));
  MOCK_METHOD(void, saveWithDelay, (uint32_t delayMs), (override));
  MOCK_METHOD(void, saveIfNeeded, (), (override));
  MOCK_METHOD(bool, generateGuidAndAuthkey, (), (override));
  MOCK_METHOD(bool, setDeviceName, (const char* name), (override));
  MOCK_METHOD(bool, setDeviceMode, (enum Supla::DeviceMode mode), (override));
  MOCK_METHOD(bool, setGUID, (const char* guid), (override));
  MOCK_METHOD(bool, getDeviceName, (char* result), (override));
  MOCK_METHOD(enum Supla::DeviceMode, getDeviceMode, (), (override));
  MOCK_METHOD(bool, getGUID, (char* result), (override));
  MOCK_METHOD(bool, getSwUpdateServer, (char* url), (override));
  MOCK_METHOD(bool, isSwUpdateBeta, (), (override));
  MOCK_METHOD(bool, setSwUpdateServer, (const char* url), (override));
  MOCK_METHOD(bool, setSwUpdateBeta, (bool enabled), (override));
  MOCK_METHOD(bool, getCustomCA, (char* result, int maxSize), (override));
  MOCK_METHOD(int, getCustomCASize, (), (override));
  MOCK_METHOD(bool, setCustomCA, (const char* customCA), (override));
  MOCK_METHOD(bool, setSuplaCommProtocolEnabled, (bool enabled), (override));
  MOCK_METHOD(bool, setSuplaServer, (const char* server), (override));
  MOCK_METHOD(bool, setSuplaServerPort, (int32_t port), (override));
  MOCK_METHOD(bool, setEmail, (const char* email), (override));
  MOCK_METHOD(bool, setAuthKey, (const char* authkey), (override));
  MOCK_METHOD(bool, isSuplaCommProtocolEnabled, (), (override));
  MOCK_METHOD(bool, getSuplaServer, (char* result), (override));
  MOCK_METHOD(int32_t, getSuplaServerPort, (), (override));
  MOCK_METHOD(bool, getEmail, (char* result), (override));
  MOCK_METHOD(bool, getAuthKey, (char* result), (override));
  MOCK_METHOD(bool, setMqttCommProtocolEnabled, (bool enabled), (override));
  MOCK_METHOD(bool, setMqttServer, (const char* server), (override));
  MOCK_METHOD(bool, setMqttServerPort, (int32_t port), (override));
  MOCK_METHOD(bool, setMqttUser, (const char* user), (override));
  MOCK_METHOD(bool, setMqttPassword, (const char* password), (override));
  MOCK_METHOD(bool, setMqttQos, (int32_t qos), (override));
  MOCK_METHOD(bool, isMqttCommProtocolEnabled, (), (override));
  MOCK_METHOD(bool, setMqttTlsEnabled, (bool enabled), (override));
  MOCK_METHOD(bool, isMqttTlsEnabled, (), (override));
  MOCK_METHOD(bool, setMqttAuthEnabled, (bool enabled), (override));
  MOCK_METHOD(bool, isMqttAuthEnabled, (), (override));
  MOCK_METHOD(bool, setMqttRetainEnabled, (bool enabled), (override));
  MOCK_METHOD(bool, isMqttRetainEnabled, (), (override));
  MOCK_METHOD(bool, getMqttServer, (char* result), (override));
  MOCK_METHOD(int32_t, getMqttServerPort, (), (override));
  MOCK_METHOD(bool, getMqttUser, (char* result), (override));
  MOCK_METHOD(bool, getMqttPassword, (char* result), (override));
  MOCK_METHOD(int32_t, getMqttQos, (), (override));
  MOCK_METHOD(bool, setMqttPrefix, (const char* prefix), (override));
  MOCK_METHOD(bool, getMqttPrefix, (char* result), (override));
  MOCK_METHOD(bool, setWiFiSSID, (const char* ssid), (override));
  MOCK_METHOD(bool, setWiFiPassword, (const char* password), (override));
  MOCK_METHOD(bool, setAltWiFiSSID, (const char* ssid), (override));
  MOCK_METHOD(bool, setAltWiFiPassword, (const char* password), (override));
  MOCK_METHOD(bool, getWiFiSSID, (char* result), (override));
  MOCK_METHOD(bool, getWiFiPassword, (char* result), (override));
  MOCK_METHOD(bool, getAltWiFiSSID, (char* result), (override));
  MOCK_METHOD(bool, getAltWiFiPassword, (char* result), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_CONFIG_MOCK_H_
