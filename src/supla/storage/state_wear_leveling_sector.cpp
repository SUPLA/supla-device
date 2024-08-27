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

#include "state_wear_leveling_sector.h"

#include <supla/log_wrapper.h>
#include <supla/crc16.h>
#include <string.h>

#include "storage.h"

using Supla::StateWearLevelingSector;

StateWearLevelingSector::StateWearLevelingSector(Storage *storage,
                       uint32_t offset,
                       uint32_t availableSize) :
  StateStorageInterface(storage, STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR),
  sectionOffset(offset),
  availableSize(availableSize) {
  int amountOfSectors = availableSize / getSectorSize();

  if (amountOfSectors >= 4) {
    lastValidAddress =
      getFirstSlotAddress() + (amountOfSectors - 2) * getSectorSize() - 1;
  }
  currentSlotAddress = getFirstSlotAddress();
  lastStoredSlotAddress = currentSlotAddress;
}

StateWearLevelingSector::~StateWearLevelingSector() {
}

void StateWearLevelingSector::initSectionPreamble(SectionPreamble *preamble) {
  if (preamble) {
    if (preamble->crc1 == preamble->crc2) {
      elementStateCrcCValid = true;
    } else {
      elementStateCrcCValid = false;
    }
  }
}

bool StateWearLevelingSector::loadPreambles(uint32_t storageStartingOffset,
    uint16_t size) {
  (void)(size);
  bool preamblesValid = false;
  if (tryLoadPreamblesFrom(storageStartingOffset)) {
    preamblesValid = true;
  } else {
    SUPLA_LOG_DEBUG("Storage: trying read preambles from backup sector");
    if (tryLoadPreamblesFrom(storageStartingOffset + getSectorSize())) {
      preamblesValid = true;
    }
  }

  if (preamblesValid) {
    initFromStorage();
  } else {
    writeSectionPreamble();
  }
  return true;
}

bool StateWearLevelingSector::tryLoadPreamblesFrom(uint32_t offset) {
  uint32_t currentOffset = offset;
  Preamble preamble = {};
  currentOffset += readStorage(currentOffset,
                               reinterpret_cast<unsigned char *>(&preamble),
                               sizeof(preamble));
  const unsigned char suplaTag[] = {'S', 'U', 'P', 'L', 'A'};

  bool suplaTagValid = memcmp(suplaTag, preamble.suplaTag, 5) == 0;

  if (!suplaTagValid) {
    SUPLA_LOG_DEBUG("Storage: missing Supla tag at %d", offset);
    return false;
  }

  if (preamble.sectionsCount == 0) {
    SUPLA_LOG_DEBUG("Storage: sectionsCount == 0 at %d", offset);
    return false;
  }

  if (preamble.version != SUPLA_STORAGE_VERSION) {
    SUPLA_LOG_DEBUG(
        "Storage: storage version [%d] is not supported. Storage not "
        "initialized",
        preamble.version);
    return false;
  }

  SUPLA_LOG_DEBUG("Storage: Number of sections %d", preamble.sectionsCount);

  for (int i = 0; i < preamble.sectionsCount; i++) {
    SUPLA_LOG_DEBUG("Reading section: %d from %d", i, currentOffset);
    SectionPreamble section;
    currentOffset += readStorage(currentOffset,
                                 reinterpret_cast<unsigned char *>(&section),
                                 sizeof(section));

    SUPLA_LOG_DEBUG(
              "Section type: %d; size: %d",
              static_cast<int>(section.type),
              section.size);

    if (section.crc1 != section.crc2) {
      SUPLA_LOG_WARNING(
          "Warning! CRC copies on section doesn't match. Please check your "
          "storage hardware");
    }

    if (sectionType == section.type) {
      initSectionPreamble(&section);

      Supla::StateWlSectorConfig sectorConfig = {};
      currentOffset +=
          readStorage(currentOffset,
              reinterpret_cast<unsigned char *>(&sectorConfig),
              sizeof(sectorConfig));

      uint16_t crc =
          calculateCrc16(reinterpret_cast<const uint8_t *>(&sectorConfig),
                         sizeof(sectorConfig.stateSlotSize));

      if (crc != sectorConfig.crc) {
        SUPLA_LOG_WARNING("Storage: invalid sector config crc");
        return false;
      }

      elementStateSize =
          sectorConfig.stateSlotSize - sizeof(StateWlSectorHeader);

      // read current slot id
      uint8_t bitmap = 0;
      int i = 0;
      for (; i < static_cast<int>(availableSize) - 2 * getSectorSize() &&
             bitmap == 0;
           i++) {
        currentOffset += readStorage(currentOffset, &bitmap, sizeof(bitmap));
      }
      int lastByteValue = 0;
      while (bitmap != 0xFF) {
        lastByteValue++;
        bitmap >>= 1;
        bitmap |= 0b10000000;
      }
      currentSlotAddress =
          getFirstSlotAddress() + (slotSize() * ((i - 1) * 8 + lastByteValue));
      SUPLA_LOG_DEBUG("Storage: initialized current slot address %d",
                      currentSlotAddress);
      lastStoredSlotAddress = currentSlotAddress;

      return true;
    } else {
      SUPLA_LOG_WARNING(
          "Storage: section type mismatch %d != %d", sectionType, section.type);
    }

    currentOffset += section.size;
  }
  return true;
}

uint16_t StateWearLevelingSector::slotSize() const {
  return elementStateSize + sizeof(StateWlSectorHeader);
}

uint32_t StateWearLevelingSector::getFirstSlotAddress() const {
  int offsetWithinSector = sectionOffset % getSectorSize();
  int bytesAvailableForConfigAndBitmap = getSectorSize() - offsetWithinSector;

  // first slot address it is at the begining of the sector, that is after
  // current sector and next one (copy)
  return sectionOffset + bytesAvailableForConfigAndBitmap + getSectorSize();
}

uint32_t StateWearLevelingSector::getNextSlotAddress(
    uint32_t slotAddress) const {
  uint32_t nextAddress = slotAddress + slotSize();
  if (lastValidAddress == 0) {
    // invalid storage configuration
    return 0;
  }

  if (nextAddress + slotSize() > lastValidAddress) {
    nextAddress = getFirstSlotAddress();
  }

  return nextAddress;
}

/*
1. check if section preamble is valid
2. check if StateWlSectorConfig is valid (element size + own crc)
3. check if lastStoredSlotAddress > currentSlotAddress
4. update bitmap address
5. update bitmap address at backup copy

if 1, 2, or 3 is false, then:
1. copy sector to memory from address 0 to section preamble
2. erase sector
3. write copy from 0 to section preamble to flash
4. write section preamble to flash
5. write StateWlSectorConfig to flash
6. init bitmap with first 0
7. erase backup copy sector
8. write backup copy sector
   */

bool StateWearLevelingSector::writeSectionPreamble() {
  if (state == State::ERROR) {
    return false;
  }
  bool isEraseNeeded = false;

  if (!initDone) {
    // if init wasn't done, it means that this is first initialization
    // of storage
    isEraseNeeded = true;
  }

  // last stored address is greater than current address when we reached
  // the end of the storage and we need to start from the beginning
  if (lastStoredSlotAddress > currentSlotAddress) {
    isEraseNeeded = true;
  }
  lastStoredSlotAddress = currentSlotAddress;
  for (int backup = 0; backup < 2; backup++) {
    uint32_t offset = sectionOffset + backup * getSectorSize();

    // Prepare SectionPreamble data
    Supla::SectionPreamble sectionPreamble = {
        STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR, 0, 0, 0};
    sectionPreamble.size = availableSize;

    // Write SectionPreamble
    writeStorage(offset,
                 reinterpret_cast<unsigned char *>(&sectionPreamble),
                 sizeof(sectionPreamble));

    // Check is stored section preamble is valid
    if (isDataDifferent(offset,
                        reinterpret_cast<uint8_t *>(&sectionPreamble),
                        sizeof(sectionPreamble))) {
      isEraseNeeded = true;
    }

    int sectorConfigOffset = offset + sizeof(Supla::SectionPreamble);

    // Prepare sector config data
    Supla::StateWlSectorConfig sectorConfig = {};
    if (initDone) {
      sectorConfig.stateSlotSize = slotSize();
      sectorConfig.crc =
          calculateCrc16(reinterpret_cast<const uint8_t *>(&sectorConfig),
                         sizeof(sectorConfig.stateSlotSize));
    } else {
      // value 0xFFFF means that there is no size set. We use FFFF here
      // because of NAND flash and FFFF value can be changed later without
      // sector erase
      sectorConfig.stateSlotSize = 0xFFFF;
      sectorConfig.crc = 0xFFFF;
    }

    // Write sector config
    writeStorage(sectorConfigOffset,
                 reinterpret_cast<unsigned char *>(&sectorConfig),
                 sizeof(sectorConfig));

    // Check is stored sector config is valid
    if (isDataDifferent(sectorConfigOffset,
                        reinterpret_cast<uint8_t *>(&sectorConfig),
                        sizeof(sectorConfig))) {
      isEraseNeeded = true;
    }

    uint32_t sectionOffsetForSector = sectionOffset % getSectorSize();
    uint8_t *dataFromSector = nullptr;
    uint32_t firstSectorAddress = offset - sectionOffsetForSector;

    if (isEraseNeeded) {
      dataFromSector = new uint8_t[sectionOffsetForSector];
      if (dataFromSector == nullptr) {
        SUPLA_LOG_ERROR("Failed to allocate memory (sectionOffsetForSector %d)",
                        sectionOffsetForSector);
        return false;
      }
      memset(dataFromSector, 0, sectionOffsetForSector);
      if (initDone) {
        readStorage(
            firstSectorAddress, dataFromSector, sectionOffsetForSector, false);
      }
      eraseSector(firstSectorAddress, getSectorSize());

      writeStorage(offset,
                   reinterpret_cast<unsigned char *>(&sectionPreamble),
                   sizeof(sectionPreamble));
      writeStorage(sectorConfigOffset,
                   reinterpret_cast<unsigned char *>(&sectorConfig),
                   sizeof(sectorConfig));
    }

    // calculate number of current state slot
    uint32_t currentSlotId =
        (currentSlotAddress - getFirstSlotAddress()) / slotSize();

    if (initDone) {
      uint8_t addressBitmapLastByte = 0xFF;
      for (uint8_t i = currentSlotId % 8; i > 0; i--) {
        addressBitmapLastByte <<= 1;
        addressBitmapLastByte &= 0b11111110;
      }

      // write address bitmap
      uint32_t addressBitmapOffset =
          sectorConfigOffset + sizeof(Supla::StateWlSectorConfig);
      writeStorage(addressBitmapOffset + currentSlotId / 8,
                   &addressBitmapLastByte,
                   sizeof(addressBitmapLastByte));

      if (currentSlotId > 7) {
        // previous bitmap byte should be 0x00
        uint8_t previousBitmapByte = 0;
        readStorage(addressBitmapOffset + currentSlotId / 8 - 1,
                    &previousBitmapByte,
                    sizeof(previousBitmapByte));

        if (previousBitmapByte != 0) {
          // make sure that previous bitmap values are 0
          while (currentSlotId > 7) {
            currentSlotId -= 8;
            addressBitmapLastByte = 0x00;
            writeStorage(addressBitmapOffset + currentSlotId / 8,
                         &addressBitmapLastByte,
                         sizeof(addressBitmapLastByte));
          }
        }
      }
      if (dataFromSector) {
        // here we write Preamble that was erased during sector erase
        // It is written as last operation, because we want to make sure that
        // all other data was successfully written earlier and thus it will
        // be properly read during initialization of storage at startup
        writeStorage(
            firstSectorAddress, dataFromSector, sectionOffsetForSector);
      }
    } else {
      // initialize main Preamble
      const unsigned char suplaTag[] = {'S', 'U', 'P', 'L', 'A'};
      Preamble preamble = {};
      memcpy(preamble.suplaTag, suplaTag, 5);
      preamble.version = SUPLA_STORAGE_VERSION;
      SectionPreamble section = {};
      section.type = sectionType;
      section.size = getSizeValue(availableSize);

      initSectionPreamble(&section);

      preamble.sectionsCount = 1;

      uint32_t storageStartingOffset = sectionOffset - sizeof(Preamble);
      writeStorage(storageStartingOffset,
                   reinterpret_cast<unsigned char *>(&preamble),
                   sizeof(preamble));
    }
    if (dataFromSector) {
      delete[] dataFromSector;
      dataFromSector = nullptr;
    }
  }
  if (!initDone) {
    // erase first data sector, so it'll be ready for write
    uint32_t firstSectorAddress = getFirstSlotAddress();
    eraseSector(firstSectorAddress, getSectorSize());
  }

  initDone = true;
  return true;
}

bool StateWearLevelingSector::initFromStorage() {
  initDone = true;
  return true;
}

void StateWearLevelingSector::deleteAll() {
}

bool StateWearLevelingSector::prepareSaveState() {
  if (state != State::NONE) {
    SUPLA_LOG_WARNING(
        "StateWearLevelingSector::prepareSaveState invalid state");
    return false;
  }

  if (dataBuffer != nullptr) {
    SUPLA_LOG_WARNING("Data buffer is not null");
    delete[] dataBuffer;
    dataBuffer = nullptr;
  }
  dataBuffer = new uint8_t[slotSize()];
  if (dataBuffer == nullptr) {
    SUPLA_LOG_ERROR("Can't allocate data buffer of size %d", slotSize());
    state = State::ERROR;
    return false;
  }
  memset(dataBuffer, 0, slotSize());
  state = State::WRITE;
  currentStateBufferOffset = 0;

  stateSlotNewSize = 0;
  currentStateBufferOffset += sizeof(StateWlSectorHeader);
  crc = 0xFFFF;
  return true;
}

bool StateWearLevelingSector::prepareSizeCheck() {
  if (state == State::ERROR) {
    return false;
  }
  state = State::SIZE_CHECK;
  stateSlotNewSize = 0;

  currentStateBufferOffset = 0;
  currentStateBufferOffset += sizeof(StateWlSectorHeader);
  crc = 0xFFFF;

  return true;
}

bool StateWearLevelingSector::prepareLoadState() {
  if (state == State::ERROR || !elementStateCrcCValid) {
    return false;
  }
  state = State::NONE;
  if (dataBuffer != nullptr) {
    SUPLA_LOG_WARNING("Storage: data buffer is not null");
    delete []dataBuffer;
    dataBuffer = nullptr;
  }
  dataBuffer = new uint8_t[slotSize()];
  memset(dataBuffer, 0, slotSize());
  readStorage(currentSlotAddress, dataBuffer, slotSize(), false);
  stateSlotNewSize = 0;
  currentStateBufferOffset = 0;
  currentStateBufferOffset += sizeof(StateWlSectorHeader);
  return true;
}

bool StateWearLevelingSector::readState(unsigned char *buf, int size) {
  stateSlotNewSize += size;
  if (state == State::ERROR || !elementStateCrcCValid ||
      dataBuffer == nullptr || elementStateSize == 0xFFFF) {
    return false;
  }

  if (elementStateSize < stateSlotNewSize) {
    SUPLA_LOG_DEBUG("Warning! Attempt to read state outside of section size");
    return false;
  }
  memcpy(buf, dataBuffer + currentStateBufferOffset, size);
  currentStateBufferOffset += size;
  return true;
}

bool StateWearLevelingSector::writeState(const unsigned char *buf, int size) {
  if (state == State::ERROR) {
    return false;
  }

  if (size == 0) {
    return true;
  }

  stateSlotNewSize += size;

  if (state == State::SIZE_CHECK) {
    currentStateBufferOffset += size;
    return true;
  }

  if (dataBuffer == nullptr || state != State::WRITE) {
    return false;
  }

  for (int i = 0; i < size; i++) {
    crc = crc16_update(crc, buf[i]);
  }

  if (stateSlotNewSize <= elementStateSize && elementStateSize != 0xFFFF) {
    memcpy(dataBuffer + currentStateBufferOffset, buf, size);
    currentStateBufferOffset += size;
  }
  return true;
}

bool StateWearLevelingSector::finalizeSaveState() {
  if (state == State::WRITE) {
    state = State::NONE;
    if (elementStateSize != stateSlotNewSize) {
      SUPLA_LOG_ERROR(
          "Element state section size doesn't match current device");
      state = State::ERROR;
      return false;
    }

    // Add crc to state slot
    StateWlSectorHeader stateWlSectorHeader = {};
    stateWlSectorHeader.crc = crc;
    memcpy(dataBuffer, &stateWlSectorHeader, sizeof(stateWlSectorHeader));

    // Check if data changed
    if (!isDataDifferent(currentSlotAddress, dataBuffer, slotSize())) {
//      SUPLA_LOG_DEBUG("WearLevelingSector: state not changed");
      delete[] dataBuffer;
      dataBuffer = nullptr;
      return true;
    }

    bool tryAgain = false;
    bool nextSectorErased = false;
    bool sectorChanged = false;
    bool writeSuccess = true;

    // Data changed, check if next sector has to be erased
    do {
      uint32_t previousSlotAddress = currentSlotAddress;
      currentSlotAddress = getNextSlotAddress(currentSlotAddress);

      int previousSlotEndAtSector =
          (previousSlotAddress + slotSize() - 1) / getSectorSize();
      int currentSlotEndAtSector =
          (currentSlotAddress + slotSize() - 1) / getSectorSize();

      if (previousSlotEndAtSector != currentSlotEndAtSector) {
        eraseSector(currentSlotEndAtSector * getSectorSize(), getSectorSize());
        nextSectorErased = true;
      } else if (nextSectorErased) {
        // next sector was earlier erased, so in this iteration we try to
        // write to next sector (previous attempt may be partially in previous
        // sector)
        sectorChanged = true;
      }

      writeStorage(currentSlotAddress, dataBuffer, slotSize());
      tryAgain = false;
      // make sure that data was properly written
      if (isDataDifferent(currentSlotAddress, dataBuffer, slotSize())) {
        SUPLA_LOG_WARNING(
            "WearLevelingSector: write failed, read data is different");
        if (!sectorChanged) {
          tryAgain = true;
        } else {
          writeSuccess = false;
        }
      }
    } while (tryAgain);
    if (dataBuffer) {
      delete []dataBuffer;
      dataBuffer = nullptr;
    }

    crc = 0xFFFF;

    if (!writeSuccess) {
      state = State::ERROR;
      return false;
    }

    elementStateCrcCValid = true;
    SUPLA_LOG_DEBUG("WearLevelingSector: wrote state, offset = %d",
                    currentSlotAddress);

    // Update section preamble
    writeSectionPreamble();
    return true;
  }
  SUPLA_LOG_WARNING("WearLevelingSector: error invalid state for write");
  return true;
}

bool StateWearLevelingSector::finalizeSizeCheck() {
  if (state != State::SIZE_CHECK) {
    return false;
  }

  state = State::NONE;
  if (elementStateSize != stateSlotNewSize) {
    SUPLA_LOG_DEBUG(
        "Element state section size doesn't match current device "
        "configuration");

    elementStateSize = stateSlotNewSize;
    currentStateBufferOffset = 0;
    elementStateCrcCValid = false;
    currentSlotAddress = getFirstSlotAddress();
    lastStoredSlotAddress = currentSlotAddress;
    eraseSector(getFirstSlotAddress(), getSectorSize());
    return false;
  }
  return true;
}

void StateWearLevelingSector::notifyUpdate() {
}

bool StateWearLevelingSector::isDataDifferent(uint32_t address,
                                              const uint8_t *data,
                                              uint32_t size) {
  if (size == 0 || size > slotSize()) {
    return false;
  }

  uint8_t *dataInStorage = new uint8_t[size];
  if (dataInStorage == nullptr) {
    return true;
  }
  memset(dataInStorage, 0, size);
  readStorage(address, dataInStorage, size);
  if (memcmp(dataInStorage, data, size) != 0) {
    delete[] dataInStorage;
    return true;
  }

  delete[] dataInStorage;
  return false;
}

uint16_t StateWearLevelingSector::getSectorSize() const {
  return 4096;
}

bool StateWearLevelingSector::finalizeLoadState() {
  if (dataBuffer) {
    delete []dataBuffer;
    dataBuffer = nullptr;
  }
  state = State::NONE;
  return true;
}

uint16_t StateWearLevelingSector::getSizeValue(uint16_t availableSize) {
  return availableSize;
}
