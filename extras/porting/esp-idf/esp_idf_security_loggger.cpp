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

#include "esp_idf_security_loggger.h"
#include <supla/log_wrapper.h>
#include <esp_partition.h>
#include <stdint.h>
#include <supla/device/security_logger.h>
#include <supla/mutex.h>
#include <supla/auto_lock.h>
#include "supla/network/html/security_log_list.h"

namespace Supla {
constexpr uint8_t SecurityLoggerPartitionh = 0x57;
constexpr char SecurityLoggerHead[] = "SSLOG\0\0";
constexpr uint8_t LastEntryNumber = 62;

#pragma pack(push, 1)
struct SecurityLoggerHeader {
  char head[8];
  uint64_t bitmap = UINT64_MAX;
  bool isFull() const { return bitmap == 0; }
  bool isEmpty() const { return bitmap == UINT64_MAX; }
};
#pragma pack(pop)

}  // namespace Supla

static_assert (sizeof(Supla::SecurityLoggerHead) <= 8);
static_assert (sizeof(Supla::SecurityLoggerHeader) == 16);


ESPIdfSecurityLogger::ESPIdfSecurityLogger() {
  mutex = Supla::Mutex::Create();
}

size_t ESPIdfSecurityLogger::getAddress(uint8_t sector, uint8_t entry) const {
  if (sector > sectorsCount) {
    SUPLA_LOG_ERROR("SSLOG: get address for sector %d is out of range", sector);
  }
  if (entry > Supla::LastEntryNumber) {
    SUPLA_LOG_ERROR("SSLOG: get address for entry %d is out of range", entry);
  }
  return sector * partition->erase_size +
         (entry + 1) * sizeof(Supla::SecurityLogEntry);
}

void ESPIdfSecurityLogger::init() {
  Supla::AutoLock lock(mutex);
  partition = esp_partition_find_first(
      static_cast<esp_partition_type_t>(Supla::SecurityLoggerPartitionh),
      static_cast<esp_partition_subtype_t>(0),
      "supla_sslog");

  if (partition == nullptr) {
    SUPLA_LOG_ERROR("Security logger partition not found");
    return;
  }

  if (partition->size < 8192) {
    SUPLA_LOG_ERROR("Security logger too small");
    partition = nullptr;
    return;
  }
  if (partition->erase_size != 4096) {
    SUPLA_LOG_ERROR("Security logger erase size is not 4096");
    partition = nullptr;
    return;
  }
  sectorsCount = partition->size / partition->erase_size;

  Supla::SecurityLoggerHeader header = {};
  for (currentSector = 0; currentSector < sectorsCount; currentSector++) {
    SUPLA_LOG_DEBUG("SSLOG: check sector %u", currentSector);
    auto result = esp_partition_read(partition,
                                     currentSector * partition->erase_size,
                                     reinterpret_cast<char *>(&header),
                                     sizeof(header));
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to read security logger: sector %u, size %u (%d)",
          currentSector,
          sizeof(header),
          result);
      continue;
    }

    if (memcmp(header.head, Supla::SecurityLoggerHead, sizeof(header.head)) ==
        0) {
      SUPLA_LOG_DEBUG("SSLOG: sector %u is valid (header present)",
                      currentSector);
      if (header.isFull()) {
        SUPLA_LOG_DEBUG("SSLOG: sector %u is full", currentSector);
        continue;
      }
      if (header.isEmpty()) {
        SUPLA_LOG_DEBUG("SSLOG: sector %u is empty", currentSector);
        continue;
      }
      // get last entry bit
      for (nextFreeEntry = 0; nextFreeEntry <= Supla::LastEntryNumber + 1;
           nextFreeEntry++) {
        uint64_t bit = header.bitmap & (1ull << nextFreeEntry);
        if (bit) {
          break;
        }
      }
      if (nextFreeEntry == 0) {
        SUPLA_LOG_ERROR("SSLOG: sector %u has invalid bitmap", currentSector);
        continue;
      }
      // nextFreeEntry points to first empty slot in currentSector
      // Entry reading should be with sizeof(entry) offset, since first
      // 64 B are for header

      Supla::SecurityLogEntry entry = {};
      result = esp_partition_read(partition,
                                  getAddress(currentSector, nextFreeEntry - 1),
                                  reinterpret_cast<char *>(&entry),
                                  sizeof(entry));
      if (result != ESP_OK) {
        SUPLA_LOG_ERROR("Failed to read security logger: sector %u, entry %u, "
                            "size %u (%d)",
            currentSector,
            nextFreeEntry - 1,
            sizeof(entry),
            result);
        return;
      }
      if (!entry.isEmpty()) {
        index = entry.index;
      } else {
        SUPLA_LOG_WARNING("SSLOG: entry %u is empty", nextFreeEntry - 1);
      }
      if (nextFreeEntry > Supla::LastEntryNumber) {
        nextFreeEntry = 0;
        currentSector++;
        if (currentSector == sectorsCount) {
          currentSector = 0;
        }
      }
      SUPLA_LOG_INFO(
          "SSLOG: restored last index %u, current sector %u, next entry %u",
          index,
          currentSector,
          nextFreeEntry);
      break;
    } else {
      SUPLA_LOG_DEBUG("SSLOG: sector %d is not yet initialized", currentSector);
    }
  }
  if (currentSector == sectorsCount) {
    SUPLA_LOG_DEBUG("SSLOG: empty - no entry found, starting from sector 0");
    currentSector = 0;
    nextFreeEntry = 0;
  }

  if (isEnabled()) {
    htmlLog = new Supla::Html::SecurityLogList(this);
  }
}

ESPIdfSecurityLogger::~ESPIdfSecurityLogger() {
  if (htmlLog != nullptr) {
    delete htmlLog;
    htmlLog = nullptr;
  }
}

void ESPIdfSecurityLogger::deleteAll() {
  if (!isEnabled()) {
    return;
  }
  Supla::AutoLock lock(mutex);

  for (currentSector = 0; currentSector < sectorsCount; currentSector++) {
    prepareNewSector(currentSector);
  }
  currentSector = 0;
  nextFreeEntry = 0;
}


bool ESPIdfSecurityLogger::prepareGetLog() {
  if (!isEnabled()) {
    return false;
  }
  mutex->lock();
  sectorForOutput = currentSector + 1;
  if (sectorForOutput >= sectorsCount) {
    sectorForOutput = 0;
  }
  entryNumberForOutput = 0;
  return true;
}

Supla::SecurityLogEntry *ESPIdfSecurityLogger::getLog() {
  if (!isEnabled()) {
    return nullptr;
  }
  while (1) {
    if (sectorForOutput == currentSector &&
        entryNumberForOutput == nextFreeEntry) {
      mutex->unlock();
      return nullptr;
    }

    if (entryNumberForOutput == 0) {
      // check sector header
      Supla::SecurityLoggerHeader header = {};
      auto result = esp_partition_read(partition,
                                       sectorForOutput * partition->erase_size,
                                       reinterpret_cast<char *>(&header),
                                       sizeof(header));
      if (result != ESP_OK) {
        SUPLA_LOG_ERROR(
            "Failed to read security logger: sector %d, size %d (%d)",
            sectorForOutput,
            sizeof(header),
            result);
        mutex->unlock();
        return nullptr;
      }
      if (memcmp(header.head, Supla::SecurityLoggerHead, sizeof(header.head)) !=
          0 || header.isEmpty()) {
        SUPLA_LOG_ERROR("SSLOG: sector %d is not initialized or empty",
                        sectorForOutput);
        if (sectorForOutput == currentSector) {
          // it was last sector to check
          mutex->unlock();
          return nullptr;
        }
        sectorForOutput++;
        if (sectorForOutput >= sectorsCount) {
          sectorForOutput = 0;
        }
        entryNumberForOutput = 0;
        continue;
      }
    }

    // header is correct, read entry
    auto result =
        esp_partition_read(partition,
                           getAddress(sectorForOutput, entryNumberForOutput),
                           reinterpret_cast<char *>(&entryForOutput),
                           sizeof(entryForOutput));
    incrementEntry(sectorForOutput, entryNumberForOutput);

    if (result != ESP_OK) {
      SUPLA_LOG_ERROR(
          "Failed to read security logger: sector %d, entry %d, size %d (%d)",
          sectorForOutput,
          entryNumberForOutput,
          sizeof(entryForOutput),
          result);
      mutex->unlock();
      return nullptr;
    }

    return &entryForOutput;
  }

  mutex->unlock();
  return nullptr;
}

void ESPIdfSecurityLogger::storeLog(const Supla::SecurityLogEntry &entry) {
  if (!isEnabled()) {
    return;
  }
  Supla::AutoLock lock(mutex);

  int attempts = 0;

  while (attempts < 10) {
    bool newSector = false;
    attempts++;
    if (nextFreeEntry == 0) {
      prepareNewSector(currentSector);
      nextFreeEntry = 0;
      newSector = true;
    }

    // check if entry is empty
    Supla::SecurityLogEntry buf = {};
    auto result = esp_partition_read(partition,
                                     getAddress(currentSector, nextFreeEntry),
                                     reinterpret_cast<char *>(&buf),
                                     sizeof(buf));
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("SSLOG flash error (%d)", result);
      return;
    }

    if (!buf.isEmpty()) {
      SUPLA_LOG_WARNING("SSLOG: sector %d, entry %d is not empty",
                        currentSector,
                        nextFreeEntry);
      // update bit in header
      setBitInHeader(currentSector, nextFreeEntry);
      incrementEntry();
      continue;
    }

    result = esp_partition_write(partition,
                                 getAddress(currentSector, nextFreeEntry),
                                 reinterpret_cast<const char *>(&entry),
                                 sizeof(entry));
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("SSLOG flash error (%d)", result);
      return;
    }

    // update bit in header
    setBitInHeader(currentSector, nextFreeEntry);

    // update last bit on previous sector
    if (newSector) {
      if (currentSector > 0) {
        setBitInHeader(currentSector - 1, 63);
      } else {
        setBitInHeader(sectorsCount - 1, 63);
      }
    }
    incrementEntry();
    break;
  }
}

void ESPIdfSecurityLogger::prepareNewSector(uint8_t sector) {
  if (!isEnabled()) {
    return;
  }

  if (sector > sectorsCount) {
    SUPLA_LOG_ERROR("SSLOG: prepeare new sector %d is out of range", sector);
    return;
  }
  esp_partition_erase_range(
      partition, sector * partition->erase_size, partition->erase_size);
  currentSector = sector;

  Supla::SecurityLoggerHeader header = {};
  memcpy(header.head,
         Supla::SecurityLoggerHead,
         sizeof(Supla::SecurityLoggerHead));

  esp_partition_write(partition,
                      sector * partition->erase_size,
                      reinterpret_cast<char *>(&header),
                      sizeof(header));
}

void ESPIdfSecurityLogger::setBitInHeader(uint8_t sector, uint8_t entry) {
  // update bit in header
  Supla::SecurityLoggerHeader header = {};
  auto result = esp_partition_read(partition,
                              sector * partition->erase_size,
                              reinterpret_cast<char *>(&header),
                              sizeof(header));
  if (result != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to read security logger: sector %d, size %d (%d)",
        sector,
        sizeof(header),
        result);
    return;
  }
  header.bitmap &= (~(1ULL << entry));
  esp_partition_write(partition,
                      sector * partition->erase_size,
                      reinterpret_cast<char *>(&header),
                      sizeof(header));
}

void ESPIdfSecurityLogger::incrementEntry() {
  incrementEntry(currentSector, nextFreeEntry);
}

void ESPIdfSecurityLogger::incrementEntry(uint8_t &sectorToIncremenet,
                                          uint8_t &entryToIncremenet) const {
  entryToIncremenet++;
  if (entryToIncremenet > Supla::LastEntryNumber) {
    entryToIncremenet = 0;
    sectorToIncremenet++;
    if (sectorToIncremenet >= sectorsCount) {
      sectorToIncremenet = 0;
    }
  }
}

bool ESPIdfSecurityLogger::isEnabled() const {
  return partition != nullptr;
}
