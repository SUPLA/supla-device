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

#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/crc16.h>
#include <supla/element.h>

#include "config.h"
#include "storage.h"
#include "state_storage_interface.h"
#include "simple_state.h"
#include "state_wear_leveling_byte.h"
#include "state_wear_leveling_sector.h"

namespace Supla {

class SpecialSectionInfo {
 public:
  int sectionId = -1;
  int offset = -1;
  int size = -1;
  bool addCrc = false;
  bool addBackupCopy = false;
  SpecialSectionInfo *next = nullptr;
};


bool Supla::Storage::storageInitDone = false;
static bool configInitDone = false;
Storage *Storage::instance = nullptr;
Config *Storage::configInstance = nullptr;

Storage *Storage::Instance() {
  return instance;
}

Config *Storage::ConfigInstance() {
  return configInstance;
}

bool Storage::Init() {
  bool result = false;
  if (Instance()) {
    if (!storageInitDone) {
      storageInitDone = true;
      result = Instance()->init();
    }
  } else {
    SUPLA_LOG_DEBUG("Main storage not configured");
  }
  if (ConfigInstance()) {
    if (!configInitDone) {
      configInitDone = true;
      result = ConfigInstance()->init();
    }
  } else {
    SUPLA_LOG_DEBUG("Config storage not configured");
  }
  return result;
}

bool Storage::ReadState(unsigned char *buf, int size) {
  if (Instance() && Instance()->stateStorage) {
    return Instance()->stateStorage->readState(buf, size);
  }
  return false;
}

bool Storage::WriteState(const unsigned char *buf, int size) {
  if (Instance() && Instance()->stateStorage) {
    return Instance()->stateStorage->writeState(buf, size);
  }
  return false;
}

bool Storage::SaveStateAllowed(uint32_t ms) {
  if (Instance()) {
    return Instance()->saveStateAllowed(ms);
  }
  return false;
}

void Storage::ScheduleSave(uint32_t delayMs) {
  if (Instance()) {
    Instance()->scheduleSave(delayMs);
  }
}

void Storage::SetConfigInstance(Config *instance) {
  configInstance = instance;
  configInitDone = false;
}

bool Storage::IsConfigStorageAvailable() {
  return (ConfigInstance() != nullptr);
}

bool Storage::RegisterSection(int sectionId,
                              int offset,
                              int size,
                              bool addCrc,
                              bool addBackupCopy) {
  if (Instance()) {
    return Instance()->registerSection(sectionId, offset, size, addCrc,
        addBackupCopy);
  }
  return false;
}

bool Storage::ReadSection(int sectionId, unsigned char *data, int size) {
  if (Instance()) {
    return Instance()->readSection(sectionId, data, size);
  }
  return false;
}

bool Storage::WriteSection(int sectionId, const unsigned char *data, int size) {
  if (Instance()) {
    return Instance()->writeSection(sectionId, data, size);
  }
  return false;
}

bool Storage::DeleteSection(int sectionId) {
  if (Instance()) {
    return Instance()->deleteSection(sectionId);
  }
  return false;
}

Storage::Storage(unsigned int storageStartingOffset,
                 unsigned int availableSize,
                 enum WearLevelingMode wearLevelingMode)
    : storageStartingOffset(storageStartingOffset),
      availableSize(availableSize),
      wearLevelingMode(wearLevelingMode) {
  instance = this;
}

Storage::~Storage() {
  instance = nullptr;
  storageInitDone = false;
  auto ptr = firstSectionInfo;
  firstSectionInfo = nullptr;
  while (ptr) {
    auto nextPtr = ptr->next;
    delete ptr;
    ptr = nextPtr;
  }
  if (stateStorage != nullptr) {
    delete stateStorage;
    stateStorage = nullptr;
  }
}

bool Storage::init() {
  SUPLA_LOG_DEBUG("Storage initialization");
  if (stateStorage != nullptr) {
    SUPLA_LOG_WARNING("Storage: stateStorage not null");
    delete stateStorage;
    stateStorage = nullptr;
  }

  uint32_t stateSectionPreambleOffset =
      storageStartingOffset + sizeof(Preamble);
  switch (wearLevelingMode) {
    case WearLevelingMode::OFF: {
      stateStorage = new Supla::SimpleState(this, stateSectionPreambleOffset);
      break;
    }
    case WearLevelingMode::BYTE_WRITE_MODE: {
      stateStorage =
        new Supla::StateWearLevelingByte(this, stateSectionPreambleOffset);
      break;
    }
    case WearLevelingMode::SECTOR_WRITE_MODE: {
      stateStorage = new Supla::StateWearLevelingSector(
          this, stateSectionPreambleOffset, availableSize);
      break;
    }
  }

  if (stateStorage == nullptr) {
    SUPLA_LOG_WARNING("Storage: stateStorage is null, abort");
    return false;
  }

  return stateStorage->loadPreambles(storageStartingOffset, availableSize);
}

void Storage::deleteAll() {
  char emptyTag[5] = {};
  writeStorage(
      storageStartingOffset, (unsigned char *)&emptyTag, sizeof(emptyTag));
  if (stateStorage != nullptr) {
    stateStorage->deleteAll();
  }
  commit();
}

int Storage::updateStorage(unsigned int offset,
                           const unsigned char *buf,
                           int size) {
  if (offset < storageStartingOffset) {
    return 0;
  }

  unsigned char *currentData = new unsigned char[size];
  readStorage(offset, currentData, size, false);

  if (memcmp(currentData, buf, size)) {
    delete[] currentData;
    if (stateStorage != nullptr) {
      stateStorage->notifyUpdate();
    }
    return writeStorage(offset, buf, size);
  }
  delete[] currentData;
  return size;
}

void Storage::setStateSavePeriod(uint32_t periodMs) {
  if (periodMs < 500) {
    saveStatePeriod = 500;
  } else {
    saveStatePeriod = periodMs;
  }
}

bool Storage::saveStateAllowed(uint32_t ms) {
  if (ms - lastWriteTimestamp > saveStatePeriod) {
    lastWriteTimestamp = ms;
    return true;
  }
  return false;
}

void Storage::scheduleSave(uint32_t delayMs) {
  uint32_t currentMs = millis();
  uint32_t newTimestamp = currentMs - saveStatePeriod - 1 + delayMs;
  if (currentMs - lastWriteTimestamp < currentMs - newTimestamp) {
    lastWriteTimestamp = newTimestamp;
  }
}

bool Storage::registerSection(int sectionId,
                              int offset,
                              int size,
                              bool addCrc,
                              bool addBackupCopy) {
  if (addBackupCopy && !addCrc) {
    SUPLA_LOG_ERROR(
        "Storage: can't add section with backup copy and without CRC");
    return false;
  }
  auto ptr = firstSectionInfo;
  if (ptr == nullptr) {
    ptr = new SpecialSectionInfo;
    firstSectionInfo = ptr;
  } else {
    do {
      if (ptr->sectionId == sectionId) {
        SUPLA_LOG_ERROR("Storage: section ID %d is already used", sectionId);
        return false;
      }

      if (ptr->next == nullptr) {
        ptr->next = new SpecialSectionInfo;
        ptr = ptr->next;
        break;
      }
      ptr = ptr->next;
    } while (1);
  }

  ptr->sectionId = sectionId;
  ptr->offset = offset;
  ptr->size = size;
  ptr->addCrc = addCrc;
  ptr->addBackupCopy = addBackupCopy;


  SUPLA_LOG_DEBUG(
      "Storage: registered section %d, offset %d, size %d, CRC %d, backup %d,"
      "total size %d",
      sectionId, offset, size, addCrc, addBackupCopy,
      (addBackupCopy ? 2 : 1) * (size + (addCrc ? 2 : 0)));
  return true;
}

bool Storage::readSection(int sectionId, unsigned char *data, int size) {
  auto ptr = firstSectionInfo;
  while (ptr) {
    if (ptr->sectionId != sectionId) {
      ptr = ptr->next;
    } else {
      if (ptr->size != size) {
        SUPLA_LOG_ERROR("Storage: special section size mismatch %d != %d",
            ptr->size, size);
        return false;
      }
      for (int entry = 0; entry < (ptr->addBackupCopy ? 2 : 1); entry++) {
        // offset is set to ptr->offset for first entry;
        // for backup entry we add section size and crc (if used)
        int offset = ptr->offset;
        offset += entry * (ptr->size + (ptr->addCrc ? sizeof(uint16_t) : 0));

        SUPLA_LOG_DEBUG("Storage special section[%d]: reading data from"
            " entry %d at offset %d, size %d",
            sectionId, entry, offset, ptr->size);

        auto readBytes = readStorage(offset, data, size);
        if (readBytes != size) {
          SUPLA_LOG_ERROR("Storage: failed to read special section");
          return false;
        }
        if (ptr->addCrc) {
          uint16_t readCrc = 0;
          readStorage(offset + size,
              reinterpret_cast<unsigned char *>(&readCrc), sizeof(readCrc));
          uint16_t calcCrc = 0xFFFF;
          for (int i = 0; i < size; i++) {
            calcCrc = crc16_update(calcCrc, data[i]);
          }
          if (readCrc != calcCrc) {
            SUPLA_LOG_WARNING(
                "Storage: special section crc check failed %d != %d",
                readCrc, calcCrc);
            // if there is backup copy, we'll try to read it
            continue;
          }
        }
        return true;
      }
      return false;
    }
  }
  SUPLA_LOG_ERROR("Storage: can't find sectionId %d", sectionId);
  return false;
}

bool Storage::writeSection(int sectionId, const unsigned char *data, int size) {
  auto ptr = firstSectionInfo;
  while (ptr) {
    if (ptr->sectionId != sectionId) {
      ptr = ptr->next;
    } else {
      if (ptr->size != size) {
        SUPLA_LOG_ERROR("Storage: special section size mismatch %d != %d",
            ptr->size, size);
        return false;
      }
      for (int entry = 0; entry < (ptr->addBackupCopy ? 2 : 1); entry++) {
        // offset is set to ptr->offset for first entry;
        // for backup entry we add section size and crc (if used)
        int offset = ptr->offset;
        offset += entry * (ptr->size + (ptr->addCrc ? sizeof(uint16_t) : 0));

        // check if stored data is the same as requested to write
        unsigned char *currentData = new unsigned char[size];
        readStorage(offset, currentData, size, false);

        auto isDataDifferent = memcmp(currentData, data, size);
        delete[] currentData;

        if (isDataDifferent) {
          SUPLA_LOG_DEBUG("Storage special section[%d]: writing data to"
              " entry %d at offset %d, size %d",
              sectionId, entry, offset, ptr->size);
          auto wroteBytes = writeStorage(offset, data, size);
          if (wroteBytes != size) {
            SUPLA_LOG_ERROR("Storage: failed to write special section");
            return false;
          }
          if (ptr->addCrc) {
            uint16_t calcCrc = 0xFFFF;
            for (int i = 0; i < size; i++) {
              calcCrc = crc16_update(calcCrc, data[i]);
            }
            writeStorage(offset + size,
                reinterpret_cast<unsigned char *>(&calcCrc), sizeof(calcCrc));
          }
        }
      }
      return true;
    }
  }
  SUPLA_LOG_ERROR("Storage: can't find sectionId %d", sectionId);
  return false;
}

bool Storage::deleteSection(int sectionId) {
  auto ptr = firstSectionInfo;
  while (ptr) {
    if (ptr->sectionId != sectionId) {
      ptr = ptr->next;
    } else {
      for (int entry = 0; entry < (ptr->addBackupCopy ? 2 : 1); entry++) {
        // offset is set to ptr->offset for first entry;
        // for backup entry we add section size and crc (if used)
        int offset = ptr->offset;
        offset += entry * (ptr->size + (ptr->addCrc ? sizeof(uint16_t) : 0));

        SUPLA_LOG_DEBUG("Storage special section[%d]: deleting data from"
            " entry %d at offset %d, size %d",
            sectionId, entry, offset, ptr->size);

        for (int i = 0; i < ptr->size; i++) {
          uint8_t zero = 0;
          writeStorage(offset + i, &zero, sizeof(zero));
        }
        if (ptr->addCrc) {
          uint16_t calcCrc = 0;
          writeStorage(offset + ptr->size,
              reinterpret_cast<unsigned char *>(&calcCrc), sizeof(calcCrc));
        }
      }
      return true;
    }
  }
  SUPLA_LOG_ERROR("Storage: can't find sectionId %d", sectionId);
  return false;
}

bool Storage::IsStateStorageValid() {
  if (!Instance() || Instance()->stateStorage == nullptr) {
    return false;
  }

  if (Instance()->stateStorage->prepareSizeCheck()) {
    SUPLA_LOG_DEBUG(
        "Validating storage state section with current device configuration");
    WriteElementsState();
    // If state storage validation was successful, perform read state
    if (Instance()->stateStorage->finalizeSizeCheck()) {
      SUPLA_LOG_INFO("Storage state section validation successful");
      return true;
    }
  }
  return false;
}

void Storage::LoadStateStorage() {
  if (!Instance() || Instance()->stateStorage == nullptr) {
    return;
  }
  // Iterate all elements and load state
  if (Instance()->stateStorage->prepareLoadState()) {
    if (Instance()->isAddChannelNumbersEnabled()) {
      uint8_t channelsToRead[256] = {};
      int channelsCount = 0;
      for (auto element = Supla::Element::begin(); element != nullptr;
          element = element->next()) {
        auto channelNumber = element->getChannelNumber();
        if (channelNumber >= 0 && channelNumber <= 255) {
          channelsToRead[channelNumber] = 1;
          channelsCount++;
        }
        delay(0);
      }

      int channelsRead = 0;
      while (channelsRead < channelsCount) {
        uint8_t channelNumber = 0;
        Supla::Storage::ReadState(
            reinterpret_cast<unsigned char *>(&channelNumber),
            sizeof(channelNumber));
        if (channelsToRead[channelNumber] == 0) {
          SUPLA_LOG_ERROR("Storage: invalid channel number %d", channelNumber);
          break;
        }

        channelsToRead[channelNumber] = 0;

        auto element = Supla::Element::getElementByChannelNumber(channelNumber);
        if (element) {
          element->onLoadState();
          delay(0);
        } else {
          SUPLA_LOG_ERROR("Storage: element with channel number %d not found",
              channelNumber);
        }
        channelsRead++;
      }
    } else {
      for (auto element = Supla::Element::begin(); element != nullptr;
          element = element->next()) {
        element->onLoadState();
        delay(0);
      }
    }
    Instance()->stateStorage->finalizeLoadState();
  }
}

void Storage::WriteStateStorage() {
  if (!Instance() || Instance()->stateStorage == nullptr) {
    return;
  }
  // Repeat save two times if finalizeSaveState returns false
  // It is used i.e. in WearLevelingSector mode, where first save pefroms
  // check if data changed compared to previously stored data
  for (int i = 0; i < 2; i++) {
    Instance()->stateStorage->prepareSaveState();
    WriteElementsState();
    bool result = Instance()->stateStorage->finalizeSaveState();
    if (result) {
      break;
    }
  }
}

void Storage::WriteElementsState() {
  if (Instance()->isAddChannelNumbersEnabled()) {
    for (auto element = Supla::Element::begin(); element != nullptr;
        element = element->next()) {
      auto channelNumber = element->getChannelNumber();
      if (channelNumber >= 0 && channelNumber <= 255) {
        uint8_t channelNumberByte = static_cast<uint8_t>(channelNumber);
        Supla::Storage::WriteState(
            reinterpret_cast<const unsigned char *>(&channelNumberByte),
            sizeof(channelNumberByte));
        element->onSaveState();
      }
      delay(0);
    }
  } else {
    for (auto element = Supla::Element::begin(); element != nullptr;
        element = element->next()) {
      element->onSaveState();
      delay(0);
    }
  }
}


void Storage::eraseSector(unsigned int address, int size) {
  (void)(address);
  (void)(size);
}

void Storage::enableChannelNumbers() {
  addChannelNumbers = true;
}

bool Storage::isAddChannelNumbersEnabled() const {
  return addChannelNumbers;
}

}  // namespace Supla
