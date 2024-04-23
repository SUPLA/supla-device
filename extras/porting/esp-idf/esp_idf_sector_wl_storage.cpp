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

#include "esp_idf_sector_wl_storage.h"

#include <supla/log_wrapper.h>
#include <supla/network/network.h>
#include <esp_partition.h>

#define SUPLA_STORAGE_PARTITION_TYPE 0x55

using Supla::EspIdfSectorWlStorage;

EspIdfSectorWlStorage::EspIdfSectorWlStorage(uint32_t size) : Storage(0, size,
    WearLevelingMode::SECTOR_WRITE_MODE) {
  setStateSavePeriod(5000);
}

EspIdfSectorWlStorage::~EspIdfSectorWlStorage() {
}

bool EspIdfSectorWlStorage::init() {
  storagePartition = esp_partition_find_first(
      static_cast<esp_partition_type_t>(SUPLA_STORAGE_PARTITION_TYPE),
      static_cast<esp_partition_subtype_t>(0),
      "suplastorage");

  if (storagePartition == nullptr) {
    SUPLA_LOG_ERROR("Storage partition not found");
    return false;
  }

  return Storage::init();
}

void EspIdfSectorWlStorage::commit() {
}

int EspIdfSectorWlStorage::readStorage(unsigned int address,
    unsigned char *buf,
    int size,
    bool logs) {
  if (storagePartition == nullptr) {
    return 0;
  }
  auto result = esp_partition_read(storagePartition, address, buf, size);
  if (result != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to read storage: addr %d, size %d (%d)",
        address, size, result);
    return 0;
  }
//  if (logs) {
//    Supla::Network::printData("Storage read:", buf, size);
//  }
  return size;
}

int EspIdfSectorWlStorage::writeStorage(unsigned int address,
    const unsigned char *buf,
    int size) {
  if (storagePartition == nullptr) {
    return 0;
  }
  auto result = esp_partition_write(storagePartition, address, buf, size);
  if (result != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to write storage: addr %d, size %d (%d)",
        address, size, result);
    return 0;
  }

  return size;
}

void EspIdfSectorWlStorage::eraseSector(unsigned int address, int size) {
  if (storagePartition == nullptr) {
    return;
  }
  auto result = esp_partition_erase_range(storagePartition, address, size);
  if (result != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to erase storage sector (%d), addr: %d, size: %d",
        result, address, size);
  }

  return;
}
