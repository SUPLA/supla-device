// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECTOR_WL_STORAGE_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECTOR_WL_STORAGE_H_

#include <supla/storage/storage.h>

#include <esp_partition.h>

namespace Supla {
class EspIdfSectorWlStorage : public Storage {
 public:
  explicit EspIdfSectorWlStorage(uint32_t size = 512);
  EspIdfSectorWlStorage(uint32_t offset, uint32_t size);
  virtual ~EspIdfSectorWlStorage();

  bool init() override;
  void commit() override;

  void eraseSector(unsigned int address, int size) override;

 protected:
  int readStorage(unsigned int address,
                  unsigned char *buf,
                  unsigned int size,
                  bool logs) override;
  int writeStorage(unsigned int address,
                   const unsigned char *buf,
                   unsigned int size) override;

  bool dataChanged = false;
  char *buffer = nullptr;
  uint32_t bufferSize = 0;

  const esp_partition_t *storagePartition = nullptr;
};
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECTOR_WL_STORAGE_H_
