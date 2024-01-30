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

#include "state_wear_leveling_byte.h"

#include <supla/log_wrapper.h>
#include <supla/crc16.h>

#include "storage.h"

using Supla::StateWearLevelingByte;

StateWearLevelingByte::StateWearLevelingByte(Storage *storage,
                       uint32_t offset,
                       SectionPreamble *preamble) :
  StateStorageInterface(storage),
  sectionOffset(offset) {
  if (preamble) {
    reservedSize = preamble->size;
    if (preamble->crc1 == preamble->crc2) {
      elementStateCrcCValid = true;
    } else {
      elementStateCrcCValid = false;
    }
  }
  if (elementStateSize > 0) {
    checkIfIsEnoughSpaceForState();
  }
}

StateWearLevelingByte::~StateWearLevelingByte() {
}

uint32_t StateWearLevelingByte::slotSize() const {
  return elementStateSize + sizeof(StateWlByteHeader);
}

uint32_t StateWearLevelingByte::getFirstSlotAddress() const {
  return sectionOffset + sizeof(Supla::SectionPreamble) +
         2 * sizeof(StateEntryAddress);
}

uint32_t StateWearLevelingByte::getNextSlotAddress(uint32_t slotAddress) const {
  uint32_t nextAddress = slotAddress + slotSize();
  if (nextAddress + slotSize() > sectionOffset + reservedSize) {
    nextAddress = getFirstSlotAddress();
  }

  return nextAddress;
}

uint32_t StateWearLevelingByte::updateStateEntryAddress() {
  StateEntryAddress stateEntryAddress = {};
  stateEntryAddress.address = currentSlotAddress;
  stateEntryAddress.elementStateSize = elementStateSize;
  stateEntryAddress.crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddress),
                     sizeof(stateEntryAddress.address) +
                         sizeof(stateEntryAddress.elementStateSize));

  uint32_t writeBytesCount = 0;

  uint32_t stateEntryAddressOffset =
      sectionOffset + sizeof(Supla::SectionPreamble);

  // First write backup copy
  writeBytesCount +=
      updateStorage(stateEntryAddressOffset + sizeof(StateEntryAddress),
                    reinterpret_cast<unsigned char *>(&stateEntryAddress),
                    sizeof(stateEntryAddress));
  // Second write main entry
  writeBytesCount += updateStorage(stateEntryAddressOffset,
                reinterpret_cast<unsigned char *>(&stateEntryAddress),
                sizeof(stateEntryAddress));

  return writeBytesCount;
}

bool StateWearLevelingByte::writeSectionPreamble() {
  checkIfIsEnoughSpaceForState();

  if (!storageStateOk) {
    return false;
  }

  Supla::SectionPreamble sectionPreamble = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, 0, 0, 0};
  sectionPreamble.size = reservedSize;

  int currentOffset = sectionOffset;
  currentOffset +=
      updateStorage(currentOffset,
                    reinterpret_cast<unsigned char *>(&sectionPreamble),
                    sizeof(sectionPreamble));

  if (!initDone) {
    // if init wasn't done, it means that this is first initialization
    // of storage
    writeCount = 0;
    currentSlotAddress = getFirstSlotAddress();
  }

  currentOffset += updateStateEntryAddress();

  if (!initDone) {
    // clear initaial memory
    StateWlByteHeader stateWlByteHeader = {};
    stateWlByteHeader.writeCount = writeCount;
    stateWlByteHeader.crc = 0;
    updateStorage(currentOffset,
                  reinterpret_cast<unsigned char *>(&stateWlByteHeader),
                  sizeof(stateWlByteHeader));
  }

  initDone = true;
  return true;
}

bool StateWearLevelingByte::initFromStorage() {
  initDone = true;

  // Read state entry addresses
  StateEntryAddress stateEntryAddressMain = {};
  StateEntryAddress stateEntryAddressBackup = {};

  uint32_t currentOffset =
      sectionOffset + sizeof(Supla::SectionPreamble);

  currentOffset +=
      readStorage(currentOffset,
                  reinterpret_cast<unsigned char *>(&stateEntryAddressMain),
                  sizeof(stateEntryAddressMain),
                  false);

  currentOffset +=
      readStorage(currentOffset,
                  reinterpret_cast<unsigned char *>(&stateEntryAddressBackup),
                  sizeof(stateEntryAddressBackup),
                  false);

  // check if crc is valid in both state entries
  uint16_t crcMain =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddressMain),
                     sizeof(stateEntryAddressMain.address) +
                         sizeof(stateEntryAddressMain.elementStateSize));
  uint16_t crcBackup = calculateCrc16(
      reinterpret_cast<const uint8_t *>(&stateEntryAddressBackup),
      sizeof(stateEntryAddressBackup.address) +
          sizeof(stateEntryAddressBackup.elementStateSize));

  if (crcMain != stateEntryAddressMain.crc &&
      crcBackup != stateEntryAddressBackup.crc) {
    SUPLA_LOG_WARNING("Storage: invalid CRC for both state entries");
    return false;
  }

  currentSlotAddress = getFirstSlotAddress();

  if (crcMain == stateEntryAddressMain.crc &&
      crcBackup == stateEntryAddressBackup.crc &&
      stateEntryAddressMain.address == stateEntryAddressBackup.address) {
    // both state address copies are the same - OK
      currentSlotAddress = stateEntryAddressMain.address;
      elementStateSize = stateEntryAddressMain.elementStateSize;
  } else {
    // one of state address copies is different or crc is not valid
    if (crcMain == stateEntryAddressMain.crc) {
      // first init data from main copy (when crc is valid)
      currentSlotAddress = stateEntryAddressMain.address;
      elementStateSize = stateEntryAddressMain.elementStateSize;
    }
    if (crcBackup == stateEntryAddressBackup.crc) {
      if (currentSlotAddress == 0 || getNextSlotAddress(currentSlotAddress) ==
          stateEntryAddressBackup.address) {
        // init data from backup copy (when crc is valid and address points
        // to next slot) - probably there was power failure during state
        // write
        currentSlotAddress = stateEntryAddressBackup.address;
        elementStateSize = stateEntryAddressBackup.elementStateSize;
      }
    }
  }

  // State entry address is read, so we read data from current slot address
  StateWlByteHeader currentStateWlByteHeader = {};
  StateWlByteHeader nextStateWlByteHeader = {};

  currentOffset = currentSlotAddress;

  currentOffset +=
      readStorage(currentOffset,
                  reinterpret_cast<unsigned char *>(&currentStateWlByteHeader),
                  sizeof(currentStateWlByteHeader),
                  false);

  crc = 0xFFFF;
  for (uint32_t i = 0; i < elementStateSize; i++) {
    uint8_t buf = 0;
    readStorage(currentOffset + i, &buf, 1, false);
    crc = crc16_update(crc, buf);
  }
  uint16_t currentSlotCrc = crc;

  SUPLA_LOG_DEBUG("Current entry: header.crc %d, CRC calc %d",
                  currentStateWlByteHeader.crc,
                  currentSlotCrc);

  currentOffset = getNextSlotAddress(currentSlotAddress);

  currentOffset +=
      readStorage(currentOffset,
                  reinterpret_cast<unsigned char *>(&nextStateWlByteHeader),
                  sizeof(nextStateWlByteHeader),
                  false);

  crc = 0xFFFF;
  for (uint32_t i = 0; i < elementStateSize; i++) {
    uint8_t buf = 0;
    readStorage(currentOffset + i, &buf, 1, false);
    crc = crc16_update(crc, buf);
  }
  uint16_t nextSlotCrc = crc;

  SUPLA_LOG_DEBUG("Next entry: header.crc %d, CRC calc %d",
                  nextStateWlByteHeader.crc,
                  nextSlotCrc);

  elementStateCrcCValid = true;

  if (currentSlotCrc == currentStateWlByteHeader.crc &&
      nextSlotCrc == nextStateWlByteHeader.crc) {
    // both crc are valid -> use latest slot
    if (nextStateWlByteHeader.writeCount == 1) {
      writeCount = 1;
    } else if (currentStateWlByteHeader.writeCount >
               nextStateWlByteHeader.writeCount) {
      writeCount = currentStateWlByteHeader.writeCount;
    } else {
      writeCount = nextStateWlByteHeader.writeCount;
    }
  } else if (currentSlotCrc == currentStateWlByteHeader.crc) {
    // only current crc is valid -> use current slot
    writeCount = currentStateWlByteHeader.writeCount;
  } else if (nextSlotCrc == nextStateWlByteHeader.crc) {
    // only next crc is valid -> use next slot
    writeCount = nextStateWlByteHeader.writeCount;
  } else {
    elementStateCrcCValid = false;
  }

  crc = 0;
  checkIfIsEnoughSpaceForState();

  // increment writeCount (it contain writeCount value for next write)
  writeCount++;
  if (writeCount > repeatBeforeSwitchToAnotherSlot) {
    currentSlotAddress = getNextSlotAddress(currentSlotAddress);
    writeCount = 1;
  }
  return true;
}

void StateWearLevelingByte::deleteAll() {
}

bool StateWearLevelingByte::prepareSaveState() {
  if (!storageStateOk) {
    return false;
  }
  dryRun = false;
  dataChanged = false;
  stateSlotNewSize = 0;
  if (writeCount % 2) {
    currentStateOffset = getNextSlotAddress(currentSlotAddress);
  } else {
    currentStateOffset = currentSlotAddress;
  }
  currentStateOffset += sizeof(StateWlByteHeader);
  crc = 0xFFFF;

  return true;
}

bool StateWearLevelingByte::prepareSizeCheck() {
  if (!storageStateOk) {
    return false;
  }
  dryRun = true;
  stateSlotNewSize = 0;
  if (writeCount % 2) {
    currentStateOffset = getNextSlotAddress(currentSlotAddress);
  } else {
    currentStateOffset = currentSlotAddress;
  }
  currentStateOffset += sizeof(StateWlByteHeader);
  crc = 0xFFFF;

  return true;
}

bool StateWearLevelingByte::prepareLoadState() {
  if (!storageStateOk || !elementStateCrcCValid) {
    return false;
  }
  dryRun = false;

  stateSlotNewSize = 0;
  int writeCountForRead = writeCount;
  if (writeCount > 0) {
    writeCountForRead--;
  }

  if (writeCountForRead % 2) {
    currentStateOffset = getNextSlotAddress(currentSlotAddress);
  } else {
    currentStateOffset = currentSlotAddress;
  }
  currentStateOffset += sizeof(StateWlByteHeader);
  return true;
}

bool StateWearLevelingByte::readState(unsigned char *buf, int size) {
  stateSlotNewSize += size;
  if (!storageStateOk || !elementStateCrcCValid) {
    return false;
  }

  if (elementStateSize < stateSlotNewSize) {
    SUPLA_LOG_DEBUG(
              "Warning! Attempt to read state outside of section size");
    return false;
  }
  currentStateOffset += readStorage(currentStateOffset, buf, size);
  return true;
}

bool StateWearLevelingByte::writeState(const unsigned char *buf, int size) {
  if (!storageStateOk) {
    return false;
  }

  stateSlotNewSize += size;

  if (size == 0) {
    return true;
  }

  if (dryRun) {
    currentStateOffset += size;
    return true;
  }

  for (int i = 0; i < size; i++) {
    crc = crc16_update(crc, buf[i]);
  }

  if (stateSlotNewSize <= elementStateSize) {
    currentStateOffset += updateStorage(currentStateOffset, buf, size);
  }

  return true;
}

bool StateWearLevelingByte::finalizeSaveState() {
  if (!storageStateOk) {
    return false;
  }

  elementStateSize = stateSlotNewSize;

  // Update section preamble
  writeSectionPreamble();
  // Write state slot header
  StateWlByteHeader stateWlByteHeader = {};
  stateWlByteHeader.writeCount = writeCount;
  stateWlByteHeader.crc = crc;
  uint32_t slotAddress = currentSlotAddress;
  uint32_t alternateSlotAddress = getNextSlotAddress(slotAddress);
  if (writeCount % 2) {
    auto copy = slotAddress;
    slotAddress = alternateSlotAddress;
    alternateSlotAddress = copy;
  }

  StateWlByteHeader currentHeader = {};
  readStorage(slotAddress, reinterpret_cast<unsigned char *>(&currentHeader),
      sizeof(currentHeader));

  if (!dataChanged && !isDataDifferent(slotAddress + sizeof(StateWlByteHeader),
      alternateSlotAddress + sizeof(StateWlByteHeader), elementStateSize)) {
    return true;
  }
  SUPLA_LOG_DEBUG("WearLevelingByte: wrote state, writeCount = %d, offset = %d",
                  writeCount,
                  slotAddress);

  updateStorage(slotAddress,
      reinterpret_cast<unsigned char *>(&stateWlByteHeader),
      sizeof(stateWlByteHeader));
  commit();
  crc = 0xFFFF;

  elementStateCrcCValid = true;

  if (writeCount == 1) {
    // When writeCount == 1, it means that we have just moved to next slot,
    // wrote it, so we have to update currentSlotAddress in header
    updateStateEntryAddress();
  }

  writeCount++;
  if (writeCount > repeatBeforeSwitchToAnotherSlot) {
    currentSlotAddress = getNextSlotAddress(currentSlotAddress);
    SUPLA_LOG_DEBUG("WearLevelingByte: Switching to next slot at address %d",
        currentSlotAddress);
    writeCount = 1;
    // update state entry header is done after next state write (after new
    // slot is actually wrote
  }

  return true;
}

bool StateWearLevelingByte::finalizeSizeCheck() {
  if (!storageStateOk) {
    return false;
  }

  dryRun = false;
  if (elementStateSize != stateSlotNewSize) {
    SUPLA_LOG_DEBUG(
        "Element state section size doesn't match current device "
        "configuration");

    elementStateSize = stateSlotNewSize;
    writeCount = 0;
    currentStateOffset = getFirstSlotAddress();
    return false;
  }
  return true;
}

void StateWearLevelingByte::checkIfIsEnoughSpaceForState() {
  int stateSlotSize = sizeof(StateWlByteHeader) + elementStateSize;
  storageStateOk = false;
  int stateSlotsCount = 0;

  if (reservedSize > 2 * sizeof(StateEntryAddress) + stateSlotSize) {
    uint32_t spaceForState = (reservedSize - 2 * sizeof(StateEntryAddress));
    stateSlotsCount = spaceForState / stateSlotSize;
  }

  if (stateSlotsCount > 1) {
//  SUPLA_LOG_DEBUG("StateWearLevelingByte: slots count: %d", stateSlotsCount);
    if (stateSlotsCount < 20) {
      SUPLA_LOG_WARNING(
          "StateWearLevelingByte: low amount of slots available: %d",
          stateSlotsCount);
    }
    repeatBeforeSwitchToAnotherSlot = stateSlotsCount;
    if (stateSlotsCount % 2 == 0) {
      repeatBeforeSwitchToAnotherSlot++;
    }
    storageStateOk = true;
  }
}

void StateWearLevelingByte::notifyUpdate() {
  dataChanged = true;
}

bool StateWearLevelingByte::isDataDifferent(uint32_t firstAddress,
                                            uint32_t secondAddress,
                                            int size) {
  for (int i = 0; i < size; i++) {
    uint8_t data1 = 0;
    uint8_t data2 = 0;
    readStorage(firstAddress + i, &data1, 1);
    readStorage(secondAddress + i, &data2, 1);
    if (data1 != data2) {
      return true;
    }
  }
  return false;
}
