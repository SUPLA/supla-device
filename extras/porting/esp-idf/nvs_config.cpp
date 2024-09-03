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

#include "nvs_config.h"

#ifdef SUPLA_DEVICE_ESP32
#include <esp_random.h>
#endif

#include <esp_system.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <supla/log_wrapper.h>
#include <supla-common/proto.h>
#include <cstring>

namespace Supla {

NvsConfig::NvsConfig() {
}

NvsConfig::~NvsConfig() {
}

#define NVS_SUPLA_PARTITION_NAME "suplanvs"
#define NVS_DEFAULT_PARTITION_NAME "nvs"

bool NvsConfig::init() {
  // check nvs_supla partition
  const char* nvsPartitionName = NVS_DEFAULT_PARTITION_NAME;
  esp_err_t err = 0;
  bool firstStep = true;
  while (true) {
    esp_err_t err = nvs_flash_init_partition(nvsPartitionName);
    if (err == ESP_ERR_NOT_FOUND) {
      if (firstStep) {
        SUPLA_LOG_ERROR("NvsConfig: NVS \"%s\" partition not found",
                        nvsPartitionName);
        return false;
      }
      nvsPartitionName = NVS_DEFAULT_PARTITION_NAME;
      break;
    }
    SUPLA_LOG_DEBUG(
        "NvsConfig: initializing nvs config storage on partition %s",
        nvsPartitionName);

    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
      nvs_flash_erase_partition(nvsPartitionName);
      err = nvs_flash_init_partition(nvsPartitionName);
      if (err != ESP_OK) {
        SUPLA_LOG_ERROR("NvsConfig: failed to init NVS storage");
        return false;
      }
    }
    nvs_stats_t nvs_stats;
    nvs_get_stats(nvsPartitionName, &nvs_stats);
    SUPLA_LOG_DEBUG(
        "NVS \"%s\" Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries "
        "= (%d)",
        nvsPartitionName,
        nvs_stats.used_entries,
        nvs_stats.free_entries,
        nvs_stats.total_entries);
    if (firstStep) {
      firstStep = false;
      nvsPartitionName = NVS_SUPLA_PARTITION_NAME;
    } else {
      break;
    }
  }
  SUPLA_LOG_INFO("NvsConfig: initialized NVS storage on partition %s",
                 nvsPartitionName);
  err = nvs_open_from_partition(
      nvsPartitionName, "supla", NVS_READWRITE, &nvsHandle);
  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("NvsConfig: failed to open NVS storage");
    return false;
  }
  return true;
}

void NvsConfig::removeAll() {
  esp_err_t err = nvs_erase_all(nvsHandle);
  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to erase NVS storage (%d)", err);
  }
  nvs_commit(nvsHandle);
}

bool NvsConfig::eraseKey(const char* key) {
  SUPLA_LOG_DEBUG("NvsConfig: erase key %s", key);
  esp_err_t err = nvs_erase_key(nvsHandle, key);
  return err == ESP_OK;
}

// Generic getters and setters
bool NvsConfig::setString(const char* key, const char* value) {
  esp_err_t err = nvs_set_str(nvsHandle, key, value);
  return err == ESP_OK;
}

bool NvsConfig::getString(const char* key, char* value, size_t maxSize) {
  esp_err_t err = nvs_get_str(nvsHandle, key, value, &maxSize);
  return err == ESP_OK;
}

int NvsConfig::getStringSize(const char* key) {
  auto buf = new char[4000];
  if (getString(key, buf, 4000)) {
    int len = strnlen(buf, 4000);
    delete [] buf;
    return len;
  }
  return -1;
}

bool NvsConfig::setBlob(const char* key, const char* value, size_t blobSize) {
  esp_err_t err = nvs_set_blob(nvsHandle, key, value, blobSize);
  return err == ESP_OK;
}

bool NvsConfig::getBlob(const char* key, char* value, size_t blobSize) {
  esp_err_t err = nvs_get_blob(nvsHandle, key, value, &blobSize);
  return err == ESP_OK;
}

int NvsConfig::getBlobSize(const char* key) {
  return -1;
}

bool NvsConfig::getInt8(const char* key, int8_t* result) {
  esp_err_t err = nvs_get_i8(nvsHandle, key, result);
  return err == ESP_OK;
}

bool NvsConfig::getUInt8(const char* key, uint8_t* result) {
  esp_err_t err = nvs_get_u8(nvsHandle, key, result);
  return err == ESP_OK;
}

bool NvsConfig::getInt32(const char* key, int32_t* result) {
  esp_err_t err = nvs_get_i32(nvsHandle, key, result);
  return err == ESP_OK;
}

bool NvsConfig::getUInt32(const char* key, uint32_t* result) {
  esp_err_t err = nvs_get_u32(nvsHandle, key, result);
  return err == ESP_OK;
}

bool NvsConfig::setInt8(const char* key, const int8_t value) {
  esp_err_t err = nvs_set_i8(nvsHandle, key, value);
  return err == ESP_OK;
}

bool NvsConfig::setUInt8(const char* key, const uint8_t value) {
  esp_err_t err = nvs_set_u8(nvsHandle, key, value);
  return err == ESP_OK;
}

bool NvsConfig::setInt32(const char* key, const int32_t value) {
  esp_err_t err = nvs_set_i32(nvsHandle, key, value);
  return err == ESP_OK;
}

bool NvsConfig::setUInt32(const char* key, const uint32_t value) {
  esp_err_t err = nvs_set_u32(nvsHandle, key, value);
  return err == ESP_OK;
}

void NvsConfig::commit() {
  SUPLA_LOG_DEBUG("NvsConfig: commit");
  nvs_commit(nvsHandle);
}

bool NvsConfig::generateGuidAndAuthkey() {
  char guid[SUPLA_GUID_SIZE];
  char authkey[SUPLA_AUTHKEY_SIZE];

  esp_fill_random(guid, SUPLA_GUID_SIZE);
  esp_fill_random(authkey, SUPLA_AUTHKEY_SIZE);

  setGUID(guid);
  setAuthKey(authkey);
  commit();
  return true;
}

}  // namespace Supla
