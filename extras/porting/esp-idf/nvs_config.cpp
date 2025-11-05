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

#include <supla/crc16.h>
#include <esp_system.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_flash_encrypt.h>
#include <supla/log_wrapper.h>
#include <supla-common/proto.h>
#include <string.h>

namespace Supla {

NvsConfig::NvsConfig() {
}

NvsConfig::~NvsConfig() {}

#define NVS_SUPLA_PARTITION_NAME "suplanvs"
#define NVS_DEFAULT_PARTITION_NAME "nvs"
#define SUPLA_DEVICE_DATA_PARTITION_NAME "supla_dev_data"
#define SUPLA_DEVICE_DATA_PARTITION_TYPE 0x56
#define SUPLA_DEVICE_DATA_PARTITION_AES_KEY_SET_MAGIC "AES_KEY_SET\0\0\0\0"
#define SUPLA_DEVICE_DATA_PARTITION_AES_NOT_SET_MAGIC "AES_NOT_SET\0\0\0\0"
#define SUPLA_DEVICE_DATA_PARTITION_GUID_MAGIC "GUID_MAGIC\0\0\0\0\0"
#define SUPLA_DEVICE_DATA_PARTITION_AUTHKEY_MAGIC "AUTHKEY_MAGIC\0\0"

constexpr uint32_t SUPLA_SECTOR_SIZE = 4096;
constexpr uint32_t DATA_PARTITION_BLOCK_SIZE = 16;

#define SUPLA_DEVICE_DATA_AES_MAGIC_OFFSET 0
#define SUPLA_DEVICE_DATA_AES_KEY_OFFSET \
  (SUPLA_DEVICE_DATA_AES_MAGIC_OFFSET + 16)
#define SUPLA_DEVICE_DATA_GUID_MAGIC_OFFSET \
  (SUPLA_DEVICE_DATA_AES_KEY_OFFSET + 32)
#define SUPLA_DEVICE_DATA_GUID_OFFSET \
  (SUPLA_DEVICE_DATA_GUID_MAGIC_OFFSET + 16)
#define SUPLA_DEVICE_DATA_AUTHKEY_MAGIC_OFFSET \
  (SUPLA_DEVICE_DATA_GUID_OFFSET + 16)
#define SUPLA_DEVICE_DATA_AUTHKEY_OFFSET \
  (SUPLA_DEVICE_DATA_AUTHKEY_MAGIC_OFFSET + 16)
#define SUPLA_DEVICE_DATA_CHECKSUM_OFFSET \
  (SUPLA_DEVICE_DATA_AUTHKEY_OFFSET + 16)

static_assert(sizeof(SUPLA_DEVICE_DATA_PARTITION_AES_KEY_SET_MAGIC) == 16);
static_assert(sizeof(SUPLA_DEVICE_DATA_PARTITION_AES_NOT_SET_MAGIC) == 16);
static_assert(sizeof(SUPLA_DEVICE_DATA_PARTITION_GUID_MAGIC) == 16);
static_assert(sizeof(SUPLA_DEVICE_DATA_PARTITION_AUTHKEY_MAGIC) == 16);
static_assert(sizeof(SUPLA_DEVICE_DATA_PARTITION_NAME) <= 16);
static_assert(sizeof(NVS_SUPLA_PARTITION_NAME) <= 16);
static_assert(sizeof(NVS_DEFAULT_PARTITION_NAME) <= 16);

bool NvsConfig::init() {
  // check nvs_supla partition
  nvsPartitionName = NVS_DEFAULT_PARTITION_NAME;
  esp_err_t err = 0;

  // check if flash encryption is in release mode
  esp_flash_enc_mode_t mode = esp_get_flash_encryption_mode();
  if (mode == ESP_FLASH_ENC_MODE_RELEASE) {
    SUPLA_LOG_INFO("NvsConfig: flash encryption in release mode");
    flashEncryptionReleaseMode = true;
  } else {
    SUPLA_LOG_ERROR("NvsConfig: flash encryption not in release mode (%d)",
                     mode);
    flashEncryptionReleaseMode = false;
  }

  auto nvsKeyPartition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, "nvs_key");

  if (nvsKeyPartition == nullptr) {
    SUPLA_LOG_ERROR("NvsConfig: nvs_key partition not found");
    nvsEncrypted = false;
  } else {
    nvsEncrypted = true;
  }

  // In test mode we use default NVS partition without encryption. However
  // test mode is stored on NVS itself, so we will try to open it unencrypted
  // first. Otherwise -> proceed with NVS_ENCRYPTION
  SUPLA_LOG_DEBUG("NvsConfig: trying to open NVS in TEST mode (unencrypted)");
  nvs_sec_scheme_t *schemeCfg = nvs_flash_get_default_security_scheme();
  nvs_sec_cfg_t cfg = {};

  err = nvs_flash_read_security_cfg_v2(schemeCfg, &cfg);
  if (err != ESP_OK) {
    SUPLA_LOG_DEBUG(
        "NvsConfig: NVS read security cfg failed, trying unencrypted NVS");
    err = nvs_flash_init_partition(nvsPartitionName);
    if (err == ESP_ERR_NOT_FOUND) {
      SUPLA_LOG_ERROR("NvsConfig: NVS \"%s\" partition not found",
                      nvsPartitionName);
      initResult = false;
      return false;
    } else if (err == ESP_OK) {
      SUPLA_LOG_DEBUG("NvsConfig: initialized unencrypted NVS");
      printStats(nvsPartitionName);
      err = nvs_open_from_partition(
          nvsPartitionName, "supla", NVS_READONLY, &nvsHandle);
      if (err == ESP_OK) {
        // check if we are in test mode:
        if (getDeviceMode() == DEVICE_MODE_TEST) {
          SUPLA_LOG_INFO("NvsConfig: initialized unencrypted NVS in TEST mode");
          // reopen in READWRITE
          nvs_close(nvsHandle);
          err = nvs_open_from_partition(
              nvsPartitionName, "supla", NVS_READWRITE, &nvsHandle);
          if (err != ESP_OK) {
            SUPLA_LOG_ERROR("NvsConfig: NVS \"%s\" open failed",
                            nvsPartitionName);
          } else {
            initResult = true;
            return true;
          }
        } else {
          SUPLA_LOG_DEBUG("NvsConfig: device mode is not TEST");
          nvs_flash_deinit_partition(nvsPartitionName);
        }
      } else {
        SUPLA_LOG_DEBUG("NvsConfig: missing supla namespace");
        nvs_flash_deinit_partition(nvsPartitionName);
      }
    }
  } else {
    SUPLA_LOG_DEBUG("NvsConfig: NVS keys configured, trying encrypted NVS");
  }

  // First init default nvs. This method will initialize keys partition when
  // NVS_ENCRYPTION is enabled
  SUPLA_LOG_DEBUG("NvsConfig: trying to open NVS in NORMAL mode");
  if (nvsEncrypted && flashEncryptionReleaseMode) {
    err = nvs_flash_init();
  } else {
    err = nvs_flash_init_partition(nvsPartitionName);
  }
  if (err == ESP_ERR_NOT_FOUND) {
    SUPLA_LOG_ERROR("NvsConfig: NVS \"%s\" partition not found",
                    nvsPartitionName);
    initResult = false;
    return false;
  }

  SUPLA_LOG_DEBUG("NvsConfig: initializing nvs config storage on partition %s",
                  nvsPartitionName);

  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    nvs_flash_erase_partition(nvsPartitionName);
    err = nvs_flash_init();
    if (err != ESP_OK) {
      SUPLA_LOG_ERROR("NvsConfig: failed to init NVS storage");
      initResult = false;
      return false;
    }
  }

  printStats(nvsPartitionName);

  // Second, try to initialize nvs_supla partition
  // Check if encryption is enabled
  nvs_sec_cfg_t *usedCfg = &cfg;

  err = nvs_flash_read_security_cfg_v2(schemeCfg, usedCfg);
  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to read NVS security cfg: [0x%02X] (%s)",
                    err,
                    esp_err_to_name(err));
    usedCfg = nullptr;
    nvsEncrypted = false;
  } else {
    SUPLA_LOG_INFO("NvsConfig: NVS encryption enabled");
    nvsEncrypted = true;
  }

  nvsPartitionName = NVS_SUPLA_PARTITION_NAME;
  err = nvs_flash_secure_init_partition(nvsPartitionName, usedCfg);
  if (err == ESP_ERR_NOT_FOUND) {
    SUPLA_LOG_DEBUG("NvsConfig: NVS \"" NVS_SUPLA_PARTITION_NAME
                    "\" partition not found, falling back to "
                    "default partition");
    nvsPartitionName = NVS_DEFAULT_PARTITION_NAME;
  } else {
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
      nvs_flash_erase_partition(nvsPartitionName);
      err = nvs_flash_secure_init_partition(nvsPartitionName, usedCfg);
      if (err != ESP_OK) {
        SUPLA_LOG_ERROR("NvsConfig: failed to init supla NVS partition");
        nvsPartitionName = NVS_DEFAULT_PARTITION_NAME;
        initResult = false;
        return false;
      }
    }

    printStats(nvsPartitionName);

    // we'll use suplanvs, however it may be required to cleanup main nvs
    // partition
    nvs_handle_t suplaNamespace;
    err = nvs_open_from_partition(
        NVS_DEFAULT_PARTITION_NAME, "supla", NVS_READWRITE, &suplaNamespace);
    nvs_erase_all(suplaNamespace);
    nvs_commit(suplaNamespace);
    nvs_close(suplaNamespace);
  }

  SUPLA_LOG_INFO("NvsConfig: initialized NVS storage on partition %s",
                 nvsPartitionName);
  err = nvs_open_from_partition(
      nvsPartitionName, "supla", NVS_READWRITE, &nvsHandle);
  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("NvsConfig: failed to open NVS storage");
    initResult = false;
    return false;
  }
//  nvs_iterator_t it = nullptr;
//  esp_err_t res =
//    nvs_entry_find(nvsPartitionName, "supla", NVS_TYPE_ANY, &it);
//  while (res == ESP_OK) {
//    nvs_entry_info_t info;
//    nvs_entry_info(it, &info);
//    SUPLA_LOG_DEBUG("  key '%s', type '%d'", info.key, info.type);
//    res = nvs_entry_next(&it);
//  }
//  nvs_release_iterator(it);
  initResult = true;
  return true;
}

void NvsConfig::printStats(const char* partitionName) const {
  nvs_stats_t nvsStats;
  nvs_get_stats(partitionName, &nvsStats);
  SUPLA_LOG_DEBUG(
      "NVS \"%s\" Count: UsedEntries: %d/%d, FreeEntries: %d, "
      ", namespaces = (%d)",
      nvsPartitionName,
      nvsStats.used_entries,
      nvsStats.total_entries,
      nvsStats.free_entries,
      nvsStats.namespace_count);
}

void NvsConfig::removeAll() {
  nvs_flash_erase_partition(nvsPartitionName);
  init();

  // esp_err_t err = nvs_erase_all(nvsHandle);
  // if (err != ESP_OK) {
  //   SUPLA_LOG_ERROR("Failed to erase NVS storage (%d)", err);
  // }
  // nvs_commit(nvsHandle);
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

bool NvsConfig::getAESKey(uint8_t* result) {
  char buf[16] = {};
  if (!readDataPartition(SUPLA_DEVICE_DATA_AES_MAGIC_OFFSET, buf, 16)) {
    return false;
  }
  if (strncmp(buf, SUPLA_DEVICE_DATA_PARTITION_AES_KEY_SET_MAGIC, 15) != 0) {
    return false;
  }

  return readDataPartition(SUPLA_DEVICE_DATA_AES_KEY_OFFSET,
                           reinterpret_cast<char*>(result),
                           SUPLA_AES_KEY_SIZE);
}

bool NvsConfig::readDataPartition(int offset, char* buffer, int size) {
  if (buffer == nullptr) {
    return false;
  }

  if (!isDeviceDataPartitionAvailable()) {
    return false;
  }

  if (!readDataPartitionImp(offset, buffer, size)) {
    return false;
  }

  return true;
}

bool NvsConfig::readDataPartitionImp(int address, char* buf, int size) {
  if (dataPartition == nullptr || buf == nullptr || size == 0) {
    return false;
  }

  auto result = esp_partition_read(
      dataPartition, dataPartitionOffset + address, buf, size);
  if (result != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to read data partition: addr %d, size %d (%d)",
        address,
        size,
        result);
    return false;
  }

  return true;
}

bool NvsConfig::isDeviceDataPartitionAvailable() {
  if (!dataPartitionInitiazlied) {
    dataPartitionInitiazlied = true;
    dataPartition = esp_partition_find_first(
        static_cast<esp_partition_type_t>(SUPLA_DEVICE_DATA_PARTITION_TYPE),
        static_cast<esp_partition_subtype_t>(0),
        SUPLA_DEVICE_DATA_PARTITION_NAME);

    if (dataPartition == nullptr) {
      SUPLA_LOG_ERROR("Data partition partition not found");
      return false;
    }

    if (dataPartition->size < 8192) {
      SUPLA_LOG_ERROR("Data partition too small");
      dataPartition = nullptr;
      return false;
    }

    char buf[16] = {};
    if (!readDataPartitionImp(0, buf, 16)) {
      dataPartition = nullptr;
      return false;
    }
    if (!initDeviceDataPartitionCopyAndChecksum()) {
      dataPartition = nullptr;
      return false;
    }
  }

  return dataPartition != nullptr;
}

bool NvsConfig::isDeviceDataValid(const DeviceDataBuf &buf) const {
  if (!isDeviceDataFilled(buf)) {
    return false;
  }

  // check checksum
  uint16_t crc16 = calculateCrc16(reinterpret_cast<const uint8_t*>(buf),
                                  sizeof(DeviceDataBuf) - 16);

  if (crc16 !=
      *(reinterpret_cast<const uint16_t*>(buf + sizeof(DeviceDataBuf) - 16))) {
    SUPLA_LOG_INFO(
        "CRC16 mismatch %d != %d",
        crc16,
        *(reinterpret_cast<const uint16_t*>(buf + sizeof(DeviceDataBuf) - 16)));
    return false;
  }

  return true;
}

bool NvsConfig::isDeviceDataFilled(const DeviceDataBuf &deviceDataBuf) const {
  const char* deviceDataBufPtr = deviceDataBuf;

  if (strncmp(deviceDataBufPtr,
              SUPLA_DEVICE_DATA_PARTITION_AES_KEY_SET_MAGIC,
              15) != 0 &&
      strncmp(deviceDataBufPtr,
              SUPLA_DEVICE_DATA_PARTITION_AES_NOT_SET_MAGIC,
              15) != 0) {
    SUPLA_LOG_INFO("AES magic key in data partition not found");
    return false;
  }
  deviceDataBufPtr += 16;
  deviceDataBufPtr += SUPLA_AES_KEY_SIZE;

  if (strncmp(deviceDataBufPtr, SUPLA_DEVICE_DATA_PARTITION_GUID_MAGIC, 15) !=
      0) {
    SUPLA_LOG_INFO("GUID magic key in data partition not found");
    return false;
  }
  deviceDataBufPtr += 16;  // guid magic
  deviceDataBufPtr += 16;  // guid

  if (strncmp(deviceDataBufPtr,
              SUPLA_DEVICE_DATA_PARTITION_AUTHKEY_MAGIC,
              15) != 0) {
    SUPLA_LOG_INFO("Authkey magic key in data partition not found");
    return false;
  }
  deviceDataBufPtr += 16;  // authkey magic
  deviceDataBufPtr += 16;  // authkey

  return true;
}

bool NvsConfig::initDeviceDataPartitionCopyAndChecksum() {
  // check for AES key signature on main and copy partition. If it is not found
  // then erase all sectors and start initialization.
  char bufMain[DATA_PARTITION_BLOCK_SIZE] = {};
  char bufCopy[DATA_PARTITION_BLOCK_SIZE] = {};

  char aesMain[2 * DATA_PARTITION_BLOCK_SIZE] = {};
  static_assert(sizeof(aesMain) == 2 * DATA_PARTITION_BLOCK_SIZE);

  // 3x magic keys, 2x for AES, 1x for GUID, 1x for authkey, 1x for checksum
  DeviceDataBuf deviceDataBuf = {};

  bool initChecksumAndCopy = false;

  if (!readDataPartitionImp(0, bufMain, 16)) {
    return false;
  }
  if (!readDataPartitionImp(SUPLA_SECTOR_SIZE, bufCopy, 16)) {
    return false;
  }
  if (!readDataPartitionImp(SUPLA_DEVICE_DATA_AES_KEY_OFFSET, aesMain, 32)) {
    return false;
  }

  bool isAesKeyMagicSet =
      !(strncmp(bufMain, SUPLA_DEVICE_DATA_PARTITION_AES_KEY_SET_MAGIC, 15) &&
        strncmp(bufMain, SUPLA_DEVICE_DATA_PARTITION_AES_NOT_SET_MAGIC, 15) &&
        strncmp(bufCopy, SUPLA_DEVICE_DATA_PARTITION_AES_KEY_SET_MAGIC, 15) &&
        strncmp(bufCopy, SUPLA_DEVICE_DATA_PARTITION_AES_NOT_SET_MAGIC, 15));
  bool isAesSet =
      strncmp(bufMain, SUPLA_DEVICE_DATA_PARTITION_AES_KEY_SET_MAGIC, 15) == 0;

  if (!readDataPartitionImp(SUPLA_DEVICE_DATA_GUID_MAGIC_OFFSET, bufMain, 16)) {
    return false;
  }
  if (!readDataPartitionImp(
          SUPLA_SECTOR_SIZE + SUPLA_DEVICE_DATA_GUID_MAGIC_OFFSET,
          bufCopy,
          16)) {
    return false;
  }

  bool isGuidMagicSet =
      !(strncmp(bufMain, SUPLA_DEVICE_DATA_PARTITION_GUID_MAGIC, 15) &&
        strncmp(bufCopy, SUPLA_DEVICE_DATA_PARTITION_GUID_MAGIC, 15));

  if (!isAesKeyMagicSet || !isGuidMagicSet) {
    SUPLA_LOG_INFO(
        "Data partition: initializing...");
    auto result = esp_partition_erase_range(dataPartition, 0, 8192);
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to erase storage sector (%d), addr: %d, size: %d",
          result, 0, 8192);
      return false;
    }
    if (isAesSet) {
      result = esp_partition_write(
          dataPartition,
          SUPLA_DEVICE_DATA_AES_MAGIC_OFFSET,
          SUPLA_DEVICE_DATA_PARTITION_AES_KEY_SET_MAGIC,
          DATA_PARTITION_BLOCK_SIZE);
      if (result != ESP_OK) {
        SUPLA_LOG_ERROR("Failed to write storage: addr %d (%d)",
            SUPLA_DEVICE_DATA_AES_MAGIC_OFFSET,
            result);
        return false;
      }
      result = esp_partition_write(
          dataPartition,
          SUPLA_DEVICE_DATA_AES_KEY_OFFSET,
          aesMain,
          sizeof(aesMain));
      if (result != ESP_OK) {
        SUPLA_LOG_ERROR("Failed to write storage: addr %d, size %d (%d)",
            SUPLA_DEVICE_DATA_AES_KEY_OFFSET,
            sizeof(aesMain),
            result);
        return false;
      }
    } else {
      result = esp_partition_write(
          dataPartition,
          SUPLA_DEVICE_DATA_AES_MAGIC_OFFSET,
          SUPLA_DEVICE_DATA_PARTITION_AES_NOT_SET_MAGIC,
          DATA_PARTITION_BLOCK_SIZE);
      if (result != ESP_OK) {
        SUPLA_LOG_ERROR(
            "Failed to write storage: addr %d (%d)",
            SUPLA_DEVICE_DATA_AES_MAGIC_OFFSET,
            result);
        return false;
      }
    }

    // generate guid + authey and write
    char guid[SUPLA_GUID_SIZE];
    char authkey[SUPLA_AUTHKEY_SIZE];

    esp_fill_random(guid, SUPLA_GUID_SIZE);
    esp_fill_random(authkey, SUPLA_AUTHKEY_SIZE);

    result =
        esp_partition_write(dataPartition,
                            SUPLA_DEVICE_DATA_GUID_MAGIC_OFFSET,
                            SUPLA_DEVICE_DATA_PARTITION_GUID_MAGIC,
                            DATA_PARTITION_BLOCK_SIZE);
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to write storage: addr %d (%d)",
                      SUPLA_DEVICE_DATA_GUID_MAGIC_OFFSET,
                      result);
      return false;
    }

    static_assert(SUPLA_GUID_SIZE == DATA_PARTITION_BLOCK_SIZE);
    result =
        esp_partition_write(dataPartition,
                            SUPLA_DEVICE_DATA_GUID_OFFSET,
                            guid,
                            SUPLA_GUID_SIZE);
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to write storage: addr %d (%d)",
                      SUPLA_DEVICE_DATA_GUID_OFFSET,
                      result);
      return false;
    }

    result = esp_partition_write(
        dataPartition,
        SUPLA_DEVICE_DATA_AUTHKEY_MAGIC_OFFSET,
        SUPLA_DEVICE_DATA_PARTITION_AUTHKEY_MAGIC,
        DATA_PARTITION_BLOCK_SIZE);
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to write storage: addr %d (%d)",
                      SUPLA_DEVICE_DATA_AUTHKEY_MAGIC_OFFSET,
                      result);
      return false;
    }

    static_assert(SUPLA_AUTHKEY_SIZE == DATA_PARTITION_BLOCK_SIZE);
    result =
        esp_partition_write(dataPartition,
                            SUPLA_DEVICE_DATA_AUTHKEY_OFFSET,
                            authkey,
                            SUPLA_AUTHKEY_SIZE);
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to write storage: addr %d (%d)",
                      SUPLA_DEVICE_DATA_AUTHKEY_OFFSET,
                      result);
      return false;
    }

    initChecksumAndCopy = true;
  }


  if (!readDataPartitionImp(SUPLA_DEVICE_DATA_AES_MAGIC_OFFSET,
                            deviceDataBuf,
                            sizeof(deviceDataBuf))) {
    return false;
  }

  if (initChecksumAndCopy) {
    if (!isDeviceDataFilled(deviceDataBuf)) {
      SUPLA_LOG_ERROR("Data partition data not ready yet");
      return false;
    }

    uint16_t crc16 =
        calculateCrc16(reinterpret_cast<const uint8_t*>(deviceDataBuf),
                       sizeof(DeviceDataBuf) - 16);

    // write checksum
    // copy checksum to 16 B buffer before write:
    uint8_t crc16Buf[DATA_PARTITION_BLOCK_SIZE] = {};
    memcpy(crc16Buf, &crc16, sizeof(crc16));
    esp_partition_write(dataPartition,
                        SUPLA_DEVICE_DATA_CHECKSUM_OFFSET,
                        crc16Buf,
                        DATA_PARTITION_BLOCK_SIZE);
    memcpy(deviceDataBuf + sizeof(DeviceDataBuf) - 16, &crc16, sizeof(crc16));

    // write backup
    static_assert(sizeof(deviceDataBuf) % DATA_PARTITION_BLOCK_SIZE == 0);
    esp_partition_write(
        dataPartition, SUPLA_SECTOR_SIZE, deviceDataBuf, sizeof(DeviceDataBuf));
  }

  if (isDeviceDataValid(deviceDataBuf)) {
    SUPLA_LOG_INFO("Data partition valid");
    return true;
  } else {
    SUPLA_LOG_INFO("Checking data partition backup...");
    dataPartitionOffset = SUPLA_SECTOR_SIZE;
    if (!readDataPartitionImp(SUPLA_DEVICE_DATA_AES_MAGIC_OFFSET,
                              deviceDataBuf,
                              sizeof(deviceDataBuf))) {
      dataPartitionOffset = 0;
      return false;
    }
    if (isDeviceDataValid(deviceDataBuf)) {
      SUPLA_LOG_INFO("Data partition backup valid");
      return true;
    }
    dataPartitionOffset = 0;
  }

  SUPLA_LOG_INFO("Data partition backup initialized");
  return true;
}

bool NvsConfig::getGUID(char* result) {
  if (isDeviceDataPartitionAvailable()) {
    char buf[16] = {};
    if (!readDataPartition(SUPLA_DEVICE_DATA_GUID_MAGIC_OFFSET, buf, 16)) {
     return false;
    }

    if (strncmp(buf, SUPLA_DEVICE_DATA_PARTITION_GUID_MAGIC, 15) != 0) {
      return false;
    }

    return readDataPartition(
        SUPLA_DEVICE_DATA_GUID_OFFSET, result, SUPLA_GUID_SIZE);
  }

  return Supla::Config::getGUID(result);
}

bool NvsConfig::getAuthKey(char* result) {
  if (isDeviceDataPartitionAvailable()) {
    char buf[16] = {};
    if (!readDataPartition(SUPLA_DEVICE_DATA_AUTHKEY_MAGIC_OFFSET, buf, 16)) {
     return false;
    }

    if (strncmp(buf, SUPLA_DEVICE_DATA_PARTITION_AUTHKEY_MAGIC, 15) != 0) {
      return false;
    }

    return readDataPartition(
        SUPLA_DEVICE_DATA_AUTHKEY_OFFSET, result, SUPLA_AUTHKEY_SIZE);
  }

  return Supla::Config::getAuthKey(result);
}

bool NvsConfig::setGUID(const char* guid) {
  if (isDeviceDataPartitionAvailable()) {
    return false;
  }

  return Supla::Config::setGUID(guid);
}

bool NvsConfig::setAuthKey(const char* key) {
  if (isDeviceDataPartitionAvailable()) {
    return false;
  }

  return Supla::Config::setAuthKey(key);
}

bool NvsConfig::isEncryptionEnabled() {
  return nvsEncrypted && flashEncryptionReleaseMode;
}

}  // namespace Supla
