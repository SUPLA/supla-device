// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_STORAGE_LITTLEFS_CONFIG_H_
#define SRC_SUPLA_STORAGE_LITTLEFS_CONFIG_H_

#ifndef SUPLA_EXCLUDE_LITTLEFS_CONFIG

#define SUPLA_LITTLEFS_CONFIG_BUF_SIZE 1024

#include "key_value.h"

namespace Supla {

class LittleFsConfig : public KeyValue {
 public:
  explicit LittleFsConfig(int configMaxSize = SUPLA_LITTLEFS_CONFIG_BUF_SIZE);
  virtual ~LittleFsConfig();
  bool init() override;
  void commit() override;
  void removeAll() override;

  bool getCustomCA(char* result, int maxSize) override;
  int getCustomCASize() override;
  bool setCustomCA(const char* customCA) override;
  bool getMqttCA(char* result, int maxSize) override;
  int getMqttCASize() override;
  bool setMqttCA(const char* mqttCA) override;

  // override blob storage to use separate file for each value
  bool setBlob(const char* key, const char* value, size_t blobSize) override;
  bool getBlob(const char* key, char* value, size_t blobSize) override;
  bool eraseKey(const char* key) override;

 protected:
  int getBlobSize(const char* key) override;
  bool initLittleFs();
  int configMaxSize = SUPLA_LITTLEFS_CONFIG_BUF_SIZE;
};
};  // namespace Supla

#endif  // SUPLA_EXCLUDE_LITTLEFS_CONFIG
#endif  // SRC_SUPLA_STORAGE_LITTLEFS_CONFIG_H_
