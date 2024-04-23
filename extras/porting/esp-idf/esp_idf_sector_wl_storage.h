/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECTOR_WL_STORAGE_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECTOR_WL_STORAGE_H_

#include <supla/storage/storage.h>

#include <esp_partition.h>

namespace Supla {
class EspIdfSectorWlStorage : public Storage {
 public:
  explicit EspIdfSectorWlStorage(uint32_t size = 512);
  virtual ~EspIdfSectorWlStorage();

  bool init() override;
  void commit() override;

 protected:
  int readStorage(unsigned int address,
                  unsigned char *buf,
                  int size,
                  bool logs) override;
  int writeStorage(unsigned int address,
                   const unsigned char *buf,
                   int size) override;
  void eraseSector(unsigned int address, int size) override;

  bool dataChanged = false;
  char *buffer = nullptr;
  uint32_t bufferSize = 0;

  const esp_partition_t *storagePartition = nullptr;
};
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECTOR_WL_STORAGE_H_
