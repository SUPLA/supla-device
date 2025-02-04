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

#include "storage_mock.h"
#include "supla/storage/storage.h"

using Supla::StateWlSectorConfig;

StorageMockSimulator::StorageMockSimulator(
    uint32_t offset, uint32_t size, enum Supla::Storage::WearLevelingMode mode)
    : Supla::Storage(offset, size, mode) {
}

int StorageMockSimulator::readStorage(unsigned int offset,
    unsigned char *data,
    int size,
    bool) {
  if (offset + size >= STORAGE_SIMULATOR_SIZE) {
    assert(false && "StorageMockSimulator out of bounds");
    return 0;
  }
  if (offset < storageStartingOffset) {
    // make sure we don't read from storage before starting offset
    assert(false && "StorageMockSimulator offset before start");
  }
  memcpy(data, &storageSimulatorData[offset], size);
  return size;
}

int StorageMockSimulator::writeStorage(unsigned int offset,
    const unsigned char *data,
    int size) {
  if (noWriteExpected) {
    assert(false && "StorageMockSimulator write not expected");
  }
  if (offset + size >= STORAGE_SIMULATOR_SIZE) {
    assert(false && "StorageMockSimulator out of bounds");
    return 0;
  }
  if (offset < storageStartingOffset) {
    // make sure we don't read from storage before starting offset
    assert(false && "StorageMockSimulator offset before start");
  }
  memcpy(&storageSimulatorData[offset], data, size);
  return size;
}

bool StorageMockSimulator::isEmpty() {
  for (int i = 0; i < STORAGE_SIMULATOR_SIZE; i++) {
    if (storageSimulatorData[i] != 0) {
      return false;
    }
  }
  return true;
}

bool StorageMockSimulator::isPreampleInitialized(int sectionCount) {
  Supla::Preamble preamble = {{'S', 'U', 'P', 'L', 'A'}, 1, 0};
  preamble.sectionsCount = sectionCount;
  return memcmp(&preamble,
                storageSimulatorData + storageStartingOffset,
                sizeof(preamble)) == 0;
}

bool StorageMockSimulator::isBackupPreampleInitialized(int sectionCount) {
  Supla::Preamble preamble = {{'S', 'U', 'P', 'L', 'A'}, 1, 0};
  preamble.sectionsCount = sectionCount;
  return memcmp(&preamble,
                storageSimulatorData + storageStartingOffset + 4096,
                sizeof(preamble)) == 0;
}

bool StorageMockSimulator::isEmptySimpleStatePreamplePresent() {
  if (isPreampleInitialized(1)) {
    Supla::SectionPreamble sectionPreamble = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE, 0, 0, 0};
    return memcmp(&sectionPreamble,
        storageSimulatorData + storageStartingOffset + sizeof(Supla::Preamble),
        sizeof(sectionPreamble)) == 0;
  }
  return false;
}
Supla::SectionPreamble *StorageMockSimulator::getSectionPreamble() {
  return reinterpret_cast<Supla::SectionPreamble *>(
      storageSimulatorData + storageStartingOffset + sizeof(Supla::Preamble));
}

Supla::SectionPreamble *StorageMockSimulator::getBackupSectionPreamble() {
  return reinterpret_cast<Supla::SectionPreamble *>(
      storageSimulatorData + storageStartingOffset + sizeof(Supla::Preamble) +
      4096);
}

Supla::StateEntryAddress *StorageMockSimulator::getStateEntryAddress(
    bool backup) {
  if (!backup) {
    return reinterpret_cast<Supla::StateEntryAddress *>(
        storageSimulatorData + storageStartingOffset + sizeof(Supla::Preamble) +
        sizeof(Supla::SectionPreamble));
  } else {
    return reinterpret_cast<Supla::StateEntryAddress *>(
        storageSimulatorData + storageStartingOffset + sizeof(Supla::Preamble) +
        sizeof(Supla::SectionPreamble) +
        sizeof(Supla::StateEntryAddress));
  }
}

using ::testing::_;

void StorageMock::defaultInitialization(int elementStateSize) {
  if (elementStateSize == 0) {
    EXPECT_CALL(*this, commit()).Times(1);
    EXPECT_CALL(*this, readStorage(0, ::testing::_, 8, ::testing::_))
      .WillOnce(
          [](uint32_t, unsigned char *data, int, bool) {
          Supla::Preamble preamble = {};
          memcpy(data, &preamble, sizeof(preamble));
          return sizeof(preamble);
          });
    EXPECT_CALL(*this, writeStorage(0, ::testing::_, 8))
      .Times(1)
      .WillOnce(::testing::Return(8));
    EXPECT_CALL(*this, readStorage(8, ::testing::_, 7, ::testing::_))
      .WillOnce(
          [](uint32_t, unsigned char *data, int, bool) {
          Supla::SectionPreamble preamble = {};
          memcpy(data, &preamble, sizeof(preamble));
          return sizeof(preamble);
          });
    EXPECT_CALL(*this, writeStorage(8, ::testing::_, 7))
      .Times(1)
      .WillOnce(::testing::Return(7));
  } else {
    EXPECT_CALL(*this, readStorage(0, _, 8, _))
      .WillRepeatedly(
          [](uint32_t, unsigned char *data, int, bool) {
          Supla::Preamble preamble = {{'S', 'U', 'P', 'L', 'A'}, 1, 1};
          memcpy(data, &preamble, sizeof(preamble));
          return sizeof(preamble);
          });
    EXPECT_CALL(*this, readStorage(8, _, 7, _))
        .WillRepeatedly(
            [elementStateSize](
                uint32_t, unsigned char *data, int, bool) {
              Supla::SectionPreamble secPreamble = {
                  STORAGE_SECTION_TYPE_ELEMENT_STATE, 0, 0, 0};
              secPreamble.size = elementStateSize;
              memcpy(data, &secPreamble, sizeof(secPreamble));
              return sizeof(secPreamble);
            });
    EXPECT_CALL(*this, readStorage(_, _, 1, _)).Times(elementStateSize);
}

  init();
}

StorageMockFlashSimulator::StorageMockFlashSimulator(
    uint32_t offset, uint32_t size, enum Supla::Storage::WearLevelingMode mode)
    : StorageMockSimulator(offset, size, mode) {
  // init flash to FF
  memset(storageSimulatorData, 0xFF, STORAGE_SIMULATOR_SIZE);
}

int StorageMockFlashSimulator::writeStorage(unsigned int offset,
    const unsigned char *data,
    int size) {
  if (noWriteExpected) {
    assert(false && "StorageMockFlashSimulator write not expected");
  }
  if (offset + size >= STORAGE_SIMULATOR_SIZE) {
    assert(false && "StorageMockFlashSimulator out of bounds");
    return 0;
  }
  for (int i = 0; i < size; i++) {
    // flash is NAND memory, so it can only clear bits (set from 1 to 0)
    storageSimulatorData[offset + i] &= data[i];
  }
  return size;
}

void StorageMockFlashSimulator::eraseSector(unsigned int address, int size) {
  if (noWriteExpected) {
    assert(false && "StorageMockFlashSimulator write not expected");
  }
  assert(size == 4096 && "StorageMockFlashSimulator erase size not 4096");
  assert(address % 4096 == 0 &&
         "StorageMockFlashSimulator erase address not multiple of 4096");

  memset(&storageSimulatorData[address], 0xFF, size);
}

bool StorageMockFlashSimulator::isEmpty() {
  for (int i = 0; i < STORAGE_SIMULATOR_SIZE; i++) {
    if (storageSimulatorData[i] != 0xFF) {
      return false;
    }
  }
  return true;
}

StateWlSectorConfig *StorageMockFlashSimulator::getStateWlSectorConfig() {
  return reinterpret_cast<Supla::StateWlSectorConfig *>(
      storageSimulatorData + storageStartingOffset + sizeof(Supla::Preamble) +
      sizeof(Supla::SectionPreamble));
}
