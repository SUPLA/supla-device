// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_NVS_CONFIG_H_
#define EXTRAS_PORTING_ESP_IDF_NVS_CONFIG_H_

#include <nvs.h>
#include <esp_partition.h>
#include <supla/storage/config.h>

namespace Supla {

class NvsConfig : public Config {
 public:
  typedef char DeviceDataBuf[16 * 8];

  NvsConfig();
  virtual ~NvsConfig();

  bool isEncryptionEnabled() override;
  bool isDeviceDataPartitionDeclared() override;
  bool isDeviceDataPartitionAvailable() override;

  bool init() override;
  void removeAll() override;

  bool generateGuidAndAuthkey() override;
  bool getAESKey(uint8_t* result) override;
  bool getGUID(char* result) override;
  bool getAuthKey(char* result) override;
  bool setGUID(const char* key) override;
  bool setAuthKey(const char* key) override;

  // Generic getters and setters
  bool setString(const char* key, const char* value) override;
  bool getString(const char* key, char* value, size_t maxSize) override;
  int getStringSize(const char* key) override;

  bool setBlob(const char* key, const char* value, size_t blobSize) override;
  bool getBlob(const char* key, char* value, size_t blobSize) override;

  bool getInt8(const char* key, int8_t* result) override;
  bool getUInt8(const char* key, uint8_t* result) override;
  bool getInt32(const char* key, int32_t* result) override;
  bool getUInt32(const char* key, uint32_t* result) override;

  bool setInt8(const char* key, const int8_t value) override;
  bool setUInt8(const char* key, const uint8_t value) override;
  bool setInt32(const char* key, const int32_t value) override;
  bool setUInt32(const char* key, const uint32_t value) override;
  bool eraseKey(const char* key) override;

  void commit() override;

 protected:
  int getBlobSize(const char* key) override;
  bool readDataPartition(int offset, char* buffer, int size);
  bool readDataPartitionImp(int address, char* buf, int size);
  bool initDeviceDataPartitionCopyAndChecksum();
  bool isDeviceDataValid(const DeviceDataBuf &buf) const;
  bool isDeviceDataFilled(const DeviceDataBuf &deviceDataBuf) const;
  void printStats(const char* partitionName) const;

  nvs_handle_t nvsHandle = 0;
  const char* nvsPartitionName = nullptr;
  const esp_partition_t *dataPartition = nullptr;
  int dataPartitionOffset = 0;
  bool dataPartitionInitiazlied = false;
  bool dataPartitionValid = false;
  bool dataPartitionValidated = false;
  bool nvsEncrypted = false;
  bool flashEncryptionReleaseMode = false;
};
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_NVS_CONFIG_H_
