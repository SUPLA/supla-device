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

#include "config.h"
#include "storage.h"

#define SUPLA_STORAGE_VERSION 1

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


static bool storageInitDone = false;
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
  if (Instance()) {
    return Instance()->readState(buf, size);
  }
  return false;
}

bool Storage::WriteState(const unsigned char *buf, int size) {
  if (Instance()) {
    return Instance()->writeState(buf, size);
  }
  return false;
}

bool Storage::PrepareState(bool dryRun) {
  if (Instance()) {
    return Instance()->prepareState(dryRun);
  }
  return false;
}

bool Storage::FinalizeSaveState() {
  if (Instance()) {
    return Instance()->finalizeSaveState();
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

Storage::Storage(unsigned int storageStartingOffset)
    : storageStartingOffset(storageStartingOffset) {
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
}

bool Storage::prepareState(bool performDryRun) {
  dryRun = performDryRun;
  newSectionSize = 0;
  currentStateOffset = elementStateOffset + sizeof(SectionPreamble);
  crc = 0xFFFF;
  return true;
}

bool Storage::readState(unsigned char *buf, int size) {
  if (!elementStateCrcCValid) {
    // don't read state if CRC was invalid
    return false;
  }

  if (elementStateOffset + sizeof(SectionPreamble) + elementStateSize <
      currentStateOffset + size) {
    SUPLA_LOG_DEBUG(
              "Warning! Attempt to read state outside of section size");
    return false;
  }
  currentStateOffset += readStorage(currentStateOffset, buf, size);
  return true;
}

bool Storage::writeState(const unsigned char *buf, int size) {
  newSectionSize += size;

  if (size == 0) {
    return true;
  }

  if (elementStateSize > 0 &&
      elementStateOffset + sizeof(SectionPreamble) + elementStateSize <
          currentStateOffset + size) {
    SUPLA_LOG_DEBUG(
              "Warning! Attempt to write state outside of section size.");
    SUPLA_LOG_DEBUG(
        "Storage: rewriting element state section. All data will be lost.");
    elementStateSize = 0;
    elementStateOffset = 0;
    return false;
  }

  if (dryRun) {
    currentStateOffset += size;
    return true;
  }

  // Calculation of offset for section data - in case sector is missing
  if (elementStateOffset == 0) {
    elementStateOffset = storageStartingOffset + sizeof(Preamble);
    SUPLA_LOG_DEBUG(
              "Initialization of elementStateOffset: %d",
              elementStateOffset);

    currentStateOffset = elementStateOffset + sizeof(SectionPreamble);

    sectionsCount++;

    // Update Storage preamble with new section count
    SUPLA_LOG_DEBUG("Updating Storage preamble");
    unsigned char suplaTag[] = {'S', 'U', 'P', 'L', 'A'};
    Preamble preamble;
    memcpy(preamble.suplaTag, suplaTag, 5);
    preamble.version = SUPLA_STORAGE_VERSION;
    preamble.sectionsCount = sectionsCount;

    updateStorage(
        storageStartingOffset, (unsigned char *)&preamble, sizeof(preamble));
  }

  for (int i = 0; i < size; i++) {
    crc = crc16_update(crc, buf[i]);
  }

  currentStateOffset += updateStorage(currentStateOffset, buf, size);

  return true;
}

bool Storage::finalizeSaveState() {
  if (dryRun) {
    dryRun = false;
    if (elementStateSize != newSectionSize) {
      SUPLA_LOG_DEBUG(
                "Element state section size doesn't match current device "
                "configuration");
      elementStateOffset = 0;
      elementStateSize = 0;
      return false;
    }
    return true;
  }

  SectionPreamble preamble;
  preamble.type = STORAGE_SECTION_TYPE_ELEMENT_STATE;
  preamble.size = newSectionSize;
  preamble.crc1 = crc;
  preamble.crc2 = crc;

  crc = 0xFFFF;

  updateStorage(
      elementStateOffset, (unsigned char *)&preamble, sizeof(preamble));

  commit();

  elementStateCrcCValid = true;

  return true;
}

bool Storage::init() {
  SUPLA_LOG_DEBUG("Storage initialization");
  unsigned int currentOffset = storageStartingOffset;
  Preamble preamble = {};
  currentOffset +=
      readStorage(currentOffset, (unsigned char *)&preamble, sizeof(preamble));

  unsigned char suplaTag[] = {'S', 'U', 'P', 'L', 'A'};

  if (memcmp(suplaTag, preamble.suplaTag, 5)) {
    SUPLA_LOG_DEBUG("Storage: missing Supla tag. Rewriting...");

    memcpy(preamble.suplaTag, suplaTag, 5);
    preamble.version = SUPLA_STORAGE_VERSION;
    preamble.sectionsCount = 0;

    writeStorage(
        storageStartingOffset, (unsigned char *)&preamble, sizeof(preamble));
    commit();

    elementStateSize = 0;
    elementStateOffset = 0;
    elementStateCrcCValid = false;

  } else if (preamble.version != SUPLA_STORAGE_VERSION) {
    SUPLA_LOG_DEBUG(
              "Storage: storage version [%d] is not supported. Storage not "
              "initialized",
              preamble.version);
    return false;
  } else {
    SUPLA_LOG_DEBUG("Storage: Number of sections %d", preamble.sectionsCount);
  }

  if (preamble.sectionsCount == 0) {
    return true;
  }

  for (int i = 0; i < preamble.sectionsCount; i++) {
    SUPLA_LOG_DEBUG("Reading section: %d", i);
    SectionPreamble section;
    unsigned int sectionOffset = currentOffset;
    currentOffset +=
        readStorage(currentOffset, (unsigned char *)&section, sizeof(section));

    SUPLA_LOG_DEBUG(
              "Section type: %d; size: %d",
              static_cast<int>(section.type),
              section.size);

    if (section.crc1 != section.crc2) {
      SUPLA_LOG_DEBUG(
          "Warning! CRC copies on section doesn't match. Please check your "
          "storage hardware");
    }

    // calculate CRC
    crc = 0xFFFF;
    for (int i = 0; i < section.size; i++) {
      uint8_t buf = 0;
      readStorage(sectionOffset + sizeof(SectionPreamble) + i, &buf, 1, false);
      crc = crc16_update(crc, buf);
    }

    SUPLA_LOG_DEBUG("CRC1 %d, CRC2 %d, CRC calc %d", section.crc1, section.crc2,
        crc);

    bool crcValid = true;
    if (crc != section.crc1 && crc != section.crc2
        && (section.crc1 != 0 || section.crc2 != 0)) {
      SUPLA_LOG_ERROR("Storage: invalid CRC for state data");
      crcValid = false;
    }

    switch (section.type) {
      case STORAGE_SECTION_TYPE_ELEMENT_STATE: {
        elementStateOffset = sectionOffset;
        elementStateSize = section.size;
        elementStateCrcCValid = crcValid;
        break;
      }
      default: {
        SUPLA_LOG_DEBUG("Warning! Unknown section type");
        break;
      }
    }
    currentOffset += section.size;
  }

  return true;
}

void Storage::deleteAll() {
  char emptyTag[5] = {};
  writeStorage(
      storageStartingOffset, (unsigned char *)&emptyTag, sizeof(emptyTag));
  commit();
  elementStateSize = 0;
  elementStateOffset = 0;
  elementStateCrcCValid = false;
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
    return writeStorage(offset, buf, size);
  }
  delete[] currentData;
  return size;
}

void Storage::setStateSavePeriod(uint32_t periodMs) {
  if (periodMs < 1000) {
    saveStatePeriod = 1000;
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
  // skip any write during dryRun
  if (dryRun) {
    return true;
  }

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

}  // namespace Supla
