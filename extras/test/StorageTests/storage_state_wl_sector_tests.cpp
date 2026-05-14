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

#include <element_with_storage.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <storage_mock.h>
#include <string.h>
#include <supla/crc16.h>
#include <supla/storage/state_wear_leveling_byte.h>
#include <supla/storage/storage.h>

#include "supla/storage/state_wear_leveling_sector.h"

// using ::testing::AtLeast;

#define SMALL_FLASH_SIZE (5 * 4096)
#define BIG_FLASH_SIZE   0x80000

class StrictStorageMockFlashSimulator : public StorageMockFlashSimulator {
 public:
  using StorageMockFlashSimulator::StorageMockFlashSimulator;

  bool outOfRangeAccessDetected = false;
  const char *offendingOperation = nullptr;
  uint32_t offendingOffset = 0;
  uint32_t offendingSize = 0;

  int writeStorage(unsigned int offset,
                   const unsigned char *data,
                   unsigned int size) override {
    verifyRange("write", offset, size);
    return StorageMockFlashSimulator::writeStorage(offset, data, size);
  }

  void eraseSector(unsigned int address, int size) override {
    verifyRange("erase", address, size);
    StorageMockFlashSimulator::eraseSector(address, size);
  }

 private:
  void verifyRange(const char *operation, uint32_t offset, uint32_t size) {
    uint64_t partitionBegin = storageStartingOffset;
    uint64_t partitionEnd =
        static_cast<uint64_t>(storageStartingOffset) + availableSize;
    uint64_t accessEnd = static_cast<uint64_t>(offset) + size;

    if (offset < partitionBegin || accessEnd > partitionEnd) {
      if (!outOfRangeAccessDetected) {
        outOfRangeAccessDetected = true;
        offendingOperation = operation;
        offendingOffset = offset;
        offendingSize = size;
      }
    }
  }
};

TEST(StorageStateWlSectorTests, preambleInitializationSizeIsZero) {
  EXPECT_FALSE(Supla::Storage::Init());

  StorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_TRUE(storage.isPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  // not initialized data
  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);
}

TEST(StorageStateWlSectorTests, preambleInitializationWithElements) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_TRUE(storage.isPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  // not initialized data
  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  auto sectorConfig = storage.getStateWlSectorConfig();
  EXPECT_EQ(sectorConfig->stateSlotSize, 2 + 8);
}

TEST(StorageStateWlSectorTests, storeAndLoadCheck) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  el1.stateValue = 1;
  el2.stateValue = 2;

  StorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_TRUE(storage.isPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  // not initialized data
  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  auto sectorConfig = storage.getStateWlSectorConfig();
  EXPECT_EQ(sectorConfig->stateSlotSize, 2 + 8);

  EXPECT_EQ(el1.stateValue, 1);
  EXPECT_EQ(el2.stateValue, 2);

  el1.stateValue = 10;
  el2.stateValue = 20;

  Supla::Storage::LoadStateStorage();

  EXPECT_EQ(el1.stateValue, 1);
  EXPECT_EQ(el2.stateValue, 2);
}

TEST(StorageStateWlSectorTests, storeAndLoadCheckMultiple) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  el1.stateValue = 0;
  el2.stateValue = 1;

  StorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_TRUE(storage.isPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  // not initialized data
  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  auto sectorConfig = storage.getStateWlSectorConfig();
  EXPECT_EQ(sectorConfig->stateSlotSize, 2 + 8);

  EXPECT_EQ(el1.stateValue, 0);
  EXPECT_EQ(el2.stateValue, 1);

  // we have 8 bytes stored in 3 * 4096 storage
  // 4000 writes per 8 bytes will make few rounds over whole memory
  for (int i = 1; i < 4000; i++) {
    el1.stateValue = i;
    el2.stateValue = i + 1;

    Supla::Storage::WriteStateStorage();

    el1.stateValue = 0;
    el2.stateValue = 0;

    if (i % 10 == 0) {
      // reinit will make sure that we properly save and load data
      Supla::Storage::storageInitDone = false;
      EXPECT_TRUE(Supla::Storage::Init());

      Supla::Storage::LoadStateStorage();
      EXPECT_EQ(el1.stateValue, i);
      EXPECT_EQ(el2.stateValue, i + 1);
    }
  }
}

TEST(StorageStateWlSectorTests, preambleReadFromBackup) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  el1.stateValue = 1;
  el2.stateValue = 2;

  StorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());

  uint32_t offset = 4096;

  // init data in storage:
  // - main sector empty
  // - backup with valid data
  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;
  memcpy(storage.storageSimulatorData + offset, &preamble, sizeof(preamble));

  offset += sizeof(preamble);
  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR, SMALL_FLASH_SIZE, 0, 0};
  memcpy(storage.storageSimulatorData + offset,
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));

  offset += sizeof(sectionPreambleMemInit);
  Supla::StateWlSectorConfig sectorConfigInit = {};
  sectorConfigInit.stateSlotSize = 10;  // 2*4 B + 2 B
  sectorConfigInit.crc = calculateCrc16(
      reinterpret_cast<unsigned char *>(&sectorConfigInit.stateSlotSize),
      sizeof(sectorConfigInit.stateSlotSize));
  memcpy(storage.storageSimulatorData + offset,
         &sectorConfigInit,
         sizeof(sectorConfigInit));

  offset += sizeof(sectorConfigInit);
  uint8_t byteIndex = 0b11111110;
  storage.storageSimulatorData[offset] = byteIndex;

  // set offset to first state storage slot (first addres at 3rd sector)
  offset = 2 * 4096 + 10;

  Supla::StateWlSectorHeader slotEntryHeader = {};
  uint32_t elValues[2] = {1, 2};
  slotEntryHeader.crc = calculateCrc16(
      reinterpret_cast<unsigned char *>(elValues), sizeof(elValues));
  memcpy(storage.storageSimulatorData + offset,
         &slotEntryHeader,
         sizeof(slotEntryHeader));
  offset += sizeof(slotEntryHeader);
  memcpy(storage.storageSimulatorData + offset, elValues, sizeof(elValues));

  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_FALSE(storage.isPreampleInitialized());
  EXPECT_TRUE(storage.isBackupPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  // not initialized data
  EXPECT_EQ(sectionPreamble->type, 0xFF);
  EXPECT_EQ(sectionPreamble->size, 0xFFFF);
  EXPECT_EQ(sectionPreamble->crc1, 0xFFFF);
  EXPECT_EQ(sectionPreamble->crc2, 0xFFFF);

  Supla::SectionPreamble *backupSectionPreamble =
      storage.getBackupSectionPreamble();
  ASSERT_NE(backupSectionPreamble, nullptr);
  // not initialized data
  EXPECT_EQ(backupSectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(backupSectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(backupSectionPreamble->crc1, 0);
  EXPECT_EQ(backupSectionPreamble->crc2, 0);

  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  Supla::Storage::LoadStateStorage();

  EXPECT_EQ(el1.stateValue, 1);
  EXPECT_EQ(el2.stateValue, 2);

  // no state change, so write will not fix main sector data
  Supla::Storage::WriteStateStorage();
  EXPECT_EQ(sectionPreamble->type, 0xFF);
  EXPECT_EQ(sectionPreamble->size, 0xFFFF);
  EXPECT_EQ(sectionPreamble->crc1, 0xFFFF);
  EXPECT_EQ(sectionPreamble->crc2, 0xFFFF);

  Supla::Storage::LoadStateStorage();

  EXPECT_EQ(el1.stateValue, 1);
  EXPECT_EQ(el2.stateValue, 2);

  el1.stateValue = 3;
  el2.stateValue = 4;

  // state changed, so write will fix main sector data
  Supla::Storage::WriteStateStorage();

  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  auto sectorConfig = storage.getStateWlSectorConfig();
  EXPECT_EQ(sectorConfig->stateSlotSize, 2 + 8);

  Supla::Storage::LoadStateStorage();

  EXPECT_EQ(el1.stateValue, 3);
  EXPECT_EQ(el2.stateValue, 4);
}

TEST(StorageStateWlSectorTests, preambleReadFromBackupInvalidSize) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  el1.stateValue = 10;
  el2.stateValue = 20;

  StorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());

  uint32_t offset = 4096;

  // init data in storage:
  // - main sector empty
  // - backup with valid data
  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;
  memcpy(storage.storageSimulatorData + offset, &preamble, sizeof(preamble));

  offset += sizeof(preamble);
  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR, SMALL_FLASH_SIZE, 0, 0};
  memcpy(storage.storageSimulatorData + offset,
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));

  offset += sizeof(sectionPreambleMemInit);
  Supla::StateWlSectorConfig sectorConfigInit = {};
  sectorConfigInit.stateSlotSize = 12;  // 2*4 B + 2 B + 2 - invalid size
  sectorConfigInit.crc = calculateCrc16(
      reinterpret_cast<unsigned char *>(&sectorConfigInit.stateSlotSize),
      sizeof(sectorConfigInit.stateSlotSize));
  memcpy(storage.storageSimulatorData + offset,
         &sectorConfigInit,
         sizeof(sectorConfigInit));

  offset += sizeof(sectorConfigInit);
  uint8_t byteIndex = 0b11111110;
  storage.storageSimulatorData[offset] = byteIndex;

  // set offset to first state storage slot (first addres at 3rd sector)
  offset = 2 * 4096 + 10;

  Supla::StateWlSectorHeader slotEntryHeader = {};
  uint32_t elValues[2] = {1, 2};
  slotEntryHeader.crc = calculateCrc16(
      reinterpret_cast<unsigned char *>(elValues), sizeof(elValues));
  memcpy(storage.storageSimulatorData + offset,
         &slotEntryHeader,
         sizeof(slotEntryHeader));
  offset += sizeof(slotEntryHeader);
  memcpy(storage.storageSimulatorData + offset, elValues, sizeof(elValues));

  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_FALSE(storage.isPreampleInitialized());
  EXPECT_TRUE(storage.isBackupPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  // not initialized data
  EXPECT_EQ(sectionPreamble->type, 0xFF);
  EXPECT_EQ(sectionPreamble->size, 0xFFFF);
  EXPECT_EQ(sectionPreamble->crc1, 0xFFFF);
  EXPECT_EQ(sectionPreamble->crc2, 0xFFFF);

  Supla::SectionPreamble *backupSectionPreamble =
      storage.getBackupSectionPreamble();
  ASSERT_NE(backupSectionPreamble, nullptr);
  // not initialized data
  EXPECT_EQ(backupSectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(backupSectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(backupSectionPreamble->crc1, 0);
  EXPECT_EQ(backupSectionPreamble->crc2, 0);

  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());

  Supla::Storage::LoadStateStorage();

  EXPECT_EQ(el1.stateValue, 10);
  EXPECT_EQ(el2.stateValue, 20);

  // There was invalid slot size stored, so this write will fix it
  Supla::Storage::WriteStateStorage();

  EXPECT_EQ(sectionPreamble->type,
            STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR);
  EXPECT_EQ(sectionPreamble->size, SMALL_FLASH_SIZE);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  auto sectorConfig = storage.getStateWlSectorConfig();
  EXPECT_EQ(sectorConfig->stateSlotSize, 2 + 8);

  el1.stateValue = 0;
  el2.stateValue = 0;

  Supla::Storage::LoadStateStorage();

  EXPECT_EQ(el1.stateValue, 10);
  EXPECT_EQ(el2.stateValue, 20);
}

TEST(StorageStateWlSectorTests,
     crashWhileLatestSectorSlotIsCorruptedShouldRecoverPreviousSlot) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el;
  el.stateValue = 123456;

  StorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());

  Supla::Preamble preamble = {};
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));

  Supla::SectionPreamble sectionPreamble = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR, SMALL_FLASH_SIZE, 0, 0};
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreamble,
         sizeof(sectionPreamble));

  Supla::StateWlSectorConfig sectorConfig = {};
  sectorConfig.stateSlotSize =
      sizeof(el.stateValue) + sizeof(Supla::StateWlSectorHeader);
  sectorConfig.crc = calculateCrc16(
      reinterpret_cast<unsigned char *>(&sectorConfig.stateSlotSize),
      sizeof(sectorConfig.stateSlotSize));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble),
         &sectorConfig,
         sizeof(sectorConfig));

  // Bitmap value 0xFE means that slot #1 is the current slot.
  // Slot #0 contains the previous valid value, slot #1 simulates a torn write.
  storage.storageSimulatorData[sizeof(Supla::Preamble) +
                               sizeof(Supla::SectionPreamble) +
                               sizeof(Supla::StateWlSectorConfig)] = 0xFE;

  const uint32_t firstSlotAddress = 2 * 4096;
  const uint32_t slotSize = sectorConfig.stateSlotSize;
  const uint32_t previousSlotAddress = firstSlotAddress;
  const uint32_t currentSlotAddress = firstSlotAddress + slotSize;

  Supla::StateWlSectorHeader slotHeader = {};
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<unsigned char *>(&el.stateValue), sizeof(el.stateValue));
  memcpy(storage.storageSimulatorData + previousSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(
      storage.storageSimulatorData + previousSlotAddress + sizeof(slotHeader),
      &el.stateValue,
      sizeof(el.stateValue));

  // Simulate a crash or torn write: the latest slot is present in metadata,
  // but its payload is erased.
  memset(storage.storageSimulatorData + currentSlotAddress, 0xFF, slotSize);

  EXPECT_TRUE(Supla::Storage::Init());

  el.stateValue = 0;
  Supla::Storage::LoadStateStorage();

  // After the fix, crash-safe sector WL should fall back to the previous valid
  // slot instead of loading the torn latest slot.
  EXPECT_EQ(el.stateValue, 123456);
}

TEST(StorageStateWlSectorTests,
     crashAfterSlotWriteBeforeBitmapUpdateShouldRecoverWrittenSlot) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el;
  el.stateValue = 123456;

  StorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();

  auto sectorConfig = storage.getStateWlSectorConfig();
  const uint32_t slotSize = sectorConfig->stateSlotSize;
  EXPECT_EQ(slotSize,
            sizeof(el.stateValue) + sizeof(Supla::StateWlSectorHeader));

  const uint32_t bitmapOffset = sizeof(Supla::Preamble) +
                                sizeof(Supla::SectionPreamble) +
                                sizeof(Supla::StateWlSectorConfig);
  EXPECT_EQ(storage.storageSimulatorData[bitmapOffset], 0xFF);

  const uint32_t firstSlotAddress = 2 * 4096;
  const uint32_t writtenSlotAddress = firstSlotAddress;
  Supla::StateWlSectorHeader writtenSlotHeader = {};
  memcpy(&writtenSlotHeader,
         storage.storageSimulatorData + writtenSlotAddress,
         sizeof(writtenSlotHeader));
  EXPECT_EQ(writtenSlotHeader.crc,
            calculateCrc16(reinterpret_cast<unsigned char *>(&el.stateValue),
                           sizeof(el.stateValue)));

  // Simulate reset after writing the first slot and sector config. There is no
  // bitmap bit to store for slot #0, so erased bitmap must still load it.
  storage.storageSimulatorData[bitmapOffset] = 0xFF;

  const uint32_t backupSectorOffset = 4096;
  Supla::StateWlSectorConfig initialSectorConfig = {};
  initialSectorConfig.stateSlotSize = 0xFFFF;
  initialSectorConfig.crc = 0xFFFF;
  memcpy(storage.storageSimulatorData + backupSectorOffset +
             sizeof(Supla::Preamble) + sizeof(Supla::SectionPreamble),
         &initialSectorConfig,
         sizeof(initialSectorConfig));
  storage.storageSimulatorData[backupSectorOffset + bitmapOffset] = 0xFF;

  Supla::Storage::storageInitDone = false;
  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  el.stateValue = 0;
  Supla::Storage::LoadStateStorage();

  EXPECT_EQ(el.stateValue, 123456);
}

TEST(StorageStateWlSectorTests, bigSizedStorageShouldNotOverflowBitmap) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockFlashSimulator storage(
      0, BIG_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();

  auto sectorConfig = storage.getStateWlSectorConfig();
  EXPECT_EQ(sectorConfig->stateSlotSize, 2 + 2 * sizeof(el1.stateValue));

  const uint32_t bitmapOffset = sizeof(Supla::Preamble) +
                                sizeof(Supla::SectionPreamble) +
                                sizeof(Supla::StateWlSectorConfig);
  const int bitmapBytesBeforeBackupSection = 4096 - bitmapOffset;
  const int firstSlotOverlappingBackupSection =
      bitmapBytesBeforeBackupSection * 8;
  for (int i = 1; i <= firstSlotOverlappingBackupSection; i++) {
    el1.stateValue = i;
    el2.stateValue = i + 1;
    Supla::Storage::WriteStateStorage();
  }

  Supla::Storage::storageInitDone = false;
  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  el1.stateValue = 0;
  el2.stateValue = 0;
  Supla::Storage::LoadStateStorage();

  EXPECT_EQ(el1.stateValue, firstSlotOverlappingBackupSection);
  EXPECT_EQ(el2.stateValue, firstSlotOverlappingBackupSection + 1);
}

TEST(StorageStateWlSectorTests, smallPartitionShouldNotWritePastStateArea) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StrictStorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();

  auto sectorConfig = storage.getStateWlSectorConfig();
  ASSERT_NE(sectorConfig, nullptr);
  EXPECT_EQ(sectorConfig->stateSlotSize, 2 + 2 * sizeof(el1.stateValue));

  const uint32_t slotSize = sectorConfig->stateSlotSize;
  ASSERT_GT(slotSize, 0u);

  const uint32_t physicalSlotCount = (SMALL_FLASH_SIZE - 2 * 4096) / slotSize;
  ASSERT_GT(physicalSlotCount, 0u);

  for (uint32_t i = 1; i <= physicalSlotCount + 1; i++) {
    el1.stateValue = i;
    el2.stateValue = i + 1;
    Supla::Storage::WriteStateStorage();
    if (storage.outOfRangeAccessDetected) {
      break;
    }
  }

  EXPECT_FALSE(storage.outOfRangeAccessDetected)
      << "first out-of-range " << storage.offendingOperation
      << " offset=" << storage.offendingOffset
      << " size=" << storage.offendingSize
      << " partition_size=" << SMALL_FLASH_SIZE << " slot_size=" << slotSize
      << " physical_slot_count=" << physicalSlotCount;
}

TEST(StorageStateWlSectorTests,
     bitmapPointingPastPhysicalAreaShouldClampLoadedSlot) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StrictStorageMockFlashSimulator storage(
      0, SMALL_FLASH_SIZE, Supla::Storage::WearLevelingMode::SECTOR_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isEmpty());

  const uint32_t sectorSize = 4096;
  const uint32_t firstSlotAddress = 2 * sectorSize;
  const uint32_t slotSize = 2 + 2 * sizeof(el1.stateValue);
  const uint32_t physicalSlotCount =
      (SMALL_FLASH_SIZE - 2 * sectorSize) / slotSize;
  ASSERT_GT(physicalSlotCount, 0u);

  const uint32_t lastPhysicalSlotAddress =
      firstSlotAddress + (physicalSlotCount - 1) * slotSize;
  const uint32_t bitmapOffset = sizeof(Supla::Preamble) +
                                sizeof(Supla::SectionPreamble) +
                                sizeof(Supla::StateWlSectorConfig);
  const uint32_t bitmapByteIndex = physicalSlotCount / 8 + 1;
  ASSERT_LT(bitmapOffset + bitmapByteIndex, sectorSize);

  const int32_t expectedEl1 = 111;
  const int32_t expectedEl2 = 222;
  const int32_t expectedPayload[] = {expectedEl1, expectedEl2};
  int32_t payloadForCrc[] = {expectedEl1, expectedEl2};

  for (uint32_t copy = 0; copy < 2; copy++) {
    uint32_t sectorOffset = copy * sectorSize;
    Supla::Preamble preamble = {};
    memcpy(preamble.suplaTag, "SUPLA", 5);
    preamble.version = 1;
    preamble.sectionsCount = 1;
    memcpy(storage.storageSimulatorData + sectorOffset,
           &preamble,
           sizeof(preamble));

    Supla::SectionPreamble sectionPreamble = {
        STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR, SMALL_FLASH_SIZE, 0, 0};
    memcpy(storage.storageSimulatorData + sectorOffset + sizeof(preamble),
           &sectionPreamble,
           sizeof(sectionPreamble));

    Supla::StateWlSectorConfig sectorConfig = {};
    sectorConfig.stateSlotSize = slotSize;
    sectorConfig.crc = calculateCrc16(
        reinterpret_cast<unsigned char *>(&sectorConfig.stateSlotSize),
        sizeof(sectorConfig.stateSlotSize));
    memcpy(storage.storageSimulatorData + sectorOffset + sizeof(preamble) +
               sizeof(sectionPreamble),
           &sectorConfig,
           sizeof(sectorConfig));

    memset(storage.storageSimulatorData + sectorOffset + bitmapOffset,
           0,
           bitmapByteIndex);
    storage
        .storageSimulatorData[sectorOffset + bitmapOffset + bitmapByteIndex] =
        0xFE;
  }

  Supla::StateWlSectorHeader slotHeader = {};
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<unsigned char *>(payloadForCrc), sizeof(payloadForCrc));
  uint8_t slotData[10] = {};
  memcpy(slotData, &slotHeader, sizeof(slotHeader));
  memcpy(
      slotData + sizeof(slotHeader), expectedPayload, sizeof(expectedPayload));
  memcpy(storage.storageSimulatorData + lastPhysicalSlotAddress,
         slotData,
         sizeof(slotData));

  Supla::Storage::storageInitDone = false;
  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  el1.stateValue = 0;
  el2.stateValue = 0;
  Supla::Storage::LoadStateStorage();

  EXPECT_FALSE(storage.outOfRangeAccessDetected)
      << "first out-of-range " << storage.offendingOperation
      << " offset=" << storage.offendingOffset
      << " size=" << storage.offendingSize
      << " partition_size=" << SMALL_FLASH_SIZE << " slot_size=" << slotSize
      << " physical_slot_count=" << physicalSlotCount;
  EXPECT_EQ(el1.stateValue, expectedEl1);
  EXPECT_EQ(el2.stateValue, expectedEl2);
}
