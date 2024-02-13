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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/storage/storage.h>
#include <storage_mock.h>
#include <string.h>
#include <stdio.h>
#include <element_with_storage.h>
#include <supla/storage/state_wear_leveling_byte.h>
#include <supla/crc16.h>
#include "supla/storage/state_wear_leveling_sector.h"

// using ::testing::AtLeast;

#define SMALL_FLASH_SIZE (5*4096)

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
      &sectionPreambleMemInit, sizeof(sectionPreambleMemInit));

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
  offset = 2*4096 + 10;

  Supla::StateWlSectorHeader slotEntryHeader = {};
  uint32_t elValues[2] = {1, 2};
  slotEntryHeader.crc = calculateCrc16(
      reinterpret_cast<unsigned char *>(elValues), sizeof(elValues));
  memcpy(storage.storageSimulatorData + offset, &slotEntryHeader,
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
      &sectionPreambleMemInit, sizeof(sectionPreambleMemInit));

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
  offset = 2*4096 + 10;

  Supla::StateWlSectorHeader slotEntryHeader = {};
  uint32_t elValues[2] = {1, 2};
  slotEntryHeader.crc = calculateCrc16(
      reinterpret_cast<unsigned char *>(elValues), sizeof(elValues));
  memcpy(storage.storageSimulatorData + offset, &slotEntryHeader,
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

