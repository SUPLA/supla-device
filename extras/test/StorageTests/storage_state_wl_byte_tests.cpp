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

using ::testing::AtLeast;

TEST(StorageStateWlByteTests, preambleInitializationMissingSize) {
  EXPECT_FALSE(Supla::Storage::Init());

  StorageMockSimulator storage(
      0, 0, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 0;

  EXPECT_CALL(storage, commit()).Times(1);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_TRUE(storage.isPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
//  EXPECT_EQ(sectionPreamble->type, STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE);
  EXPECT_EQ(sectionPreamble->type, 0);
  EXPECT_EQ(sectionPreamble->size, 0);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  ASSERT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  ASSERT_FALSE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(sectionPreamble->size, 0);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);
}

TEST(StorageStateWlByteTests, preambleInitializationNoElements) {
  EXPECT_FALSE(Supla::Storage::Init());

  // 100 bytes reserved for state storage
  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 0;

  EXPECT_CALL(storage, commit()).Times(1);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_TRUE(storage.isPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  EXPECT_EQ(sectionPreamble->type, STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE);
  EXPECT_EQ(sectionPreamble->size, 92);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(sectionPreamble->size, 92);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);
}

TEST(StorageStateWlByteTests, preambleInitializationWithElementAndMissingSize) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 0, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 0;

  EXPECT_CALL(storage, commit()).Times(1);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_TRUE(storage.isPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  EXPECT_EQ(sectionPreamble->type, 0);
  EXPECT_EQ(sectionPreamble->size, 0);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  ASSERT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  ASSERT_FALSE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(sectionPreamble->size, 0);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);
}

TEST(StorageStateWlByteTests, preambleInitialization) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 0;

  EXPECT_CALL(storage, commit()).Times(AtLeast(1));

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_TRUE(storage.isPreampleInitialized());
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  EXPECT_EQ(sectionPreamble->type, STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE);
  EXPECT_EQ(sectionPreamble->size, 92);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  Supla::StateEntryAddress *stateEntryAddress = storage.getStateEntryAddress();
  Supla::StateEntryAddress *backupStateEntryAddress =
      storage.getStateEntryAddress(true);

  ASSERT_NE(stateEntryAddress, nullptr);
  ASSERT_NE(backupStateEntryAddress, nullptr);
  ASSERT_NE(stateEntryAddress, backupStateEntryAddress);

  EXPECT_EQ(memcmp(stateEntryAddress,
                   backupStateEntryAddress,
                   sizeof(Supla::StateEntryAddress)),
            0);

  uint32_t firstAddress = 0 + sizeof(Supla::Preamble) +
                          sizeof(Supla::SectionPreamble) +
                          2 * sizeof(Supla::StateEntryAddress);
  EXPECT_EQ(stateEntryAddress->address, firstAddress);
  EXPECT_EQ(stateEntryAddress->elementStateSize, 0);  // inital value

  ASSERT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(sectionPreamble->size, 92);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);
  EXPECT_EQ(stateEntryAddress->elementStateSize, 8);
  EXPECT_EQ(stateEntryAddress->address, firstAddress);
}

TEST(StorageStateWlByteTests, preambleAlreadyInitializedMultipleWriteAndRead) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, 92, 0, 0};

  // main state address:
  uint32_t address = sizeof(Supla::Preamble) + sizeof(Supla::SectionPreamble) +
                     2 * sizeof(Supla::StateEntryAddress);

  Supla::StateEntryAddress stateEntryAddressMain = {address, 8};
  Supla::StateEntryAddress stateEntryAddressBackup = {address, 8};

  uint16_t crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddressMain),
                     sizeof(stateEntryAddressMain.address) +
                     sizeof(stateEntryAddressMain.elementStateSize));
  stateEntryAddressMain.crc = crc;
  stateEntryAddressBackup.crc = crc;

  EXPECT_TRUE(storage.isEmpty());
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble),
         &stateEntryAddressMain,
         sizeof(stateEntryAddressMain));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble) + sizeof(Supla::StateEntryAddress),
         &stateEntryAddressBackup,
         sizeof(stateEntryAddressBackup));

  EXPECT_CALL(storage, commit()).Times(AtLeast(1));

  EXPECT_TRUE(storage.isPreampleInitialized(1));
  Supla::SectionPreamble *sectionPreamble = storage.getSectionPreamble();
  ASSERT_NE(sectionPreamble, nullptr);
  EXPECT_EQ(sectionPreamble->type, STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE);
  EXPECT_EQ(sectionPreamble->size, 92);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  Supla::StateEntryAddress *stateEntryAddress = storage.getStateEntryAddress();
  Supla::StateEntryAddress *backupStateEntryAddress =
      storage.getStateEntryAddress(true);

  ASSERT_NE(stateEntryAddress, nullptr);
  ASSERT_NE(backupStateEntryAddress, nullptr);
  ASSERT_NE(stateEntryAddress, backupStateEntryAddress);

  EXPECT_EQ(memcmp(stateEntryAddress,
                   backupStateEntryAddress,
                   sizeof(Supla::StateEntryAddress)),
            0);

  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_EQ(sectionPreamble->type, STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE);
  EXPECT_EQ(sectionPreamble->size, 92);
  EXPECT_EQ(sectionPreamble->crc1, 0);
  EXPECT_EQ(sectionPreamble->crc2, 0);

  EXPECT_EQ(memcmp(stateEntryAddress,
                   backupStateEntryAddress,
                   sizeof(Supla::StateEntryAddress)),
            0);

  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  el1.stateValue = 5;
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  el2.stateValue = 6;
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  el1.stateValue = 1;
  el2.stateValue = 2;
  Supla::Storage::WriteStateStorage();
  EXPECT_EQ(el1.stateValue, 1);
  EXPECT_EQ(el2.stateValue, 2);

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 1);
  EXPECT_EQ(el2.stateValue, 2);

  for (int i = 0; i < 1000; i++) {
    el1.stateValue = i;
    el2.stateValue = i + 1;
    Supla::Storage::WriteStateStorage();
    if (i % 3) {
      el1.stateValue = 0;
      el2.stateValue = 0;
      Supla::Storage::LoadStateStorage();
      EXPECT_EQ(el1.stateValue, i);
      EXPECT_EQ(el2.stateValue, i+1);
    }
  }
  for (int i = 4440; i < 8880; i++) {
    el1.stateValue = i;
    el2.stateValue = i + 1;
    Supla::Storage::WriteStateStorage();
  }
  el1.stateValue = 0;
  el2.stateValue = 0;
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 8879);
  EXPECT_EQ(el2.stateValue, 8880);
}

TEST(StorageStateWlByteTests, loadDataFromStorage) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, 92, 0, 0};

  // main state address:
  uint32_t firstSlotAddress = sizeof(Supla::Preamble) +
                              sizeof(Supla::SectionPreamble) +
                              2 * sizeof(Supla::StateEntryAddress);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 3 * (sizeof(Supla::StateEntryAddress) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8};
  stateEntryAddressMain.address = thirdSlotAddress;
  stateEntryAddressBackup.address = thirdSlotAddress;

  uint16_t crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddressMain),
                     sizeof(stateEntryAddressMain.address) +
                     sizeof(stateEntryAddressMain.elementStateSize));
  stateEntryAddressMain.crc = crc;
  stateEntryAddressBackup.crc = crc;

  // init preambles and headers:
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble),
         &stateEntryAddressMain,
         sizeof(stateEntryAddressMain));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble) + sizeof(Supla::StateEntryAddress),
         &stateEntryAddressBackup,
         sizeof(stateEntryAddressBackup));

  // init actual storage data at "thirdSlotAddress"
  Supla::StateWlByteHeader slotHeader = {};
  int valueElements[2] = {123, 456};
  slotHeader.writeCount = 2;
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());

  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 123);
  EXPECT_EQ(el2.stateValue, 456);
}

TEST(StorageStateWlByteTests, loadDataFromStorageWithBothSlots) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, 92, 0, 0};

  // main state address:
  uint32_t firstSlotAddress = sizeof(Supla::Preamble) +
                              sizeof(Supla::SectionPreamble) +
                              2 * sizeof(Supla::StateEntryAddress);
  uint32_t secondSlotAddress = firstSlotAddress +
                                2 * (sizeof(Supla::StateEntryAddress) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 3 * (sizeof(Supla::StateEntryAddress) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8};
  stateEntryAddressMain.address = secondSlotAddress;
  stateEntryAddressBackup.address = secondSlotAddress;

  uint16_t crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddressMain),
                     sizeof(stateEntryAddressMain.address) +
                     sizeof(stateEntryAddressMain.elementStateSize));
  stateEntryAddressMain.crc = crc;
  stateEntryAddressBackup.crc = crc;

  // init preambles and headers:
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble),
         &stateEntryAddressMain,
         sizeof(stateEntryAddressMain));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble) + sizeof(Supla::StateEntryAddress),
         &stateEntryAddressBackup,
         sizeof(stateEntryAddressBackup));

  // init actual storage data at "secondSlotAddress" - latest one
  Supla::StateWlByteHeader slotHeader = {};
  int valueElements[2] = {123, 456};
  slotHeader.writeCount = 2;
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + secondSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress"
  slotHeader.writeCount = 1;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());

  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 123);
  EXPECT_EQ(el2.stateValue, 456);
}

TEST(StorageStateWlByteTests, loadDataFromStorageWithBothSlotsInvert) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, 92, 0, 0};

  // main state address:
  uint32_t firstSlotAddress = sizeof(Supla::Preamble) +
                              sizeof(Supla::SectionPreamble) +
                              2 * sizeof(Supla::StateEntryAddress);
  uint32_t secondSlotAddress = firstSlotAddress +
                                1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8};
  stateEntryAddressMain.address = secondSlotAddress;
  stateEntryAddressBackup.address = secondSlotAddress;

  uint16_t crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddressMain),
                     sizeof(stateEntryAddressMain.address) +
                     sizeof(stateEntryAddressMain.elementStateSize));
  stateEntryAddressMain.crc = crc;
  stateEntryAddressBackup.crc = crc;

  // init preambles and headers:
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble),
         &stateEntryAddressMain,
         sizeof(stateEntryAddressMain));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble) + sizeof(Supla::StateEntryAddress),
         &stateEntryAddressBackup,
         sizeof(stateEntryAddressBackup));

  // init actual storage data at "secondSlotAddress"
  Supla::StateWlByteHeader slotHeader = {};
  int valueElements[2] = {123, 456};
  slotHeader.writeCount = 2;
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + secondSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress" - latest one
  slotHeader.writeCount = 3;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());

  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 1);
  EXPECT_EQ(el2.stateValue, 2);
}

TEST(StorageStateWlByteTests, loadDataFromStorageWithBothSlotsOneInvalid) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, 92, 0, 0};

  // main state address:
  uint32_t firstSlotAddress = sizeof(Supla::Preamble) +
                              sizeof(Supla::SectionPreamble) +
                              2 * sizeof(Supla::StateEntryAddress);
  uint32_t secondSlotAddress = firstSlotAddress +
                                1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8};
  stateEntryAddressMain.address = secondSlotAddress;
  stateEntryAddressBackup.address = secondSlotAddress;

  uint16_t crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddressMain),
                     sizeof(stateEntryAddressMain.address) +
                     sizeof(stateEntryAddressMain.elementStateSize));
  stateEntryAddressMain.crc = crc;
  stateEntryAddressBackup.crc = crc;

  // init preambles and headers:
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble),
         &stateEntryAddressMain,
         sizeof(stateEntryAddressMain));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble) + sizeof(Supla::StateEntryAddress),
         &stateEntryAddressBackup,
         sizeof(stateEntryAddressBackup));

  // init actual storage data at "secondSlotAddress" - latest one, but crc is
  // invalid
  Supla::StateWlByteHeader slotHeader = {};
  int valueElements[2] = {123, 456};
  slotHeader.writeCount = 2;
  // set invalid crc
  slotHeader.crc =
      1 + calculateCrc16(reinterpret_cast<const uint8_t *>(valueElements),
                         sizeof(valueElements));
  memcpy(storage.storageSimulatorData + secondSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress"
  slotHeader.writeCount = 1;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());

  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 1);
  EXPECT_EQ(el2.stateValue, 2);
}

TEST(StorageStateWlByteTests, loadDataFromStorageWithBothSlotsSecondInvalid) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, 92, 0, 0};

  // main state address:
  uint32_t firstSlotAddress = sizeof(Supla::Preamble) +
                              sizeof(Supla::SectionPreamble) +
                              2 * sizeof(Supla::StateEntryAddress);
  uint32_t secondSlotAddress = firstSlotAddress +
                                1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8};
  stateEntryAddressMain.address = secondSlotAddress;
  stateEntryAddressBackup.address = secondSlotAddress;

  uint16_t crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddressMain),
                     sizeof(stateEntryAddressMain.address) +
                     sizeof(stateEntryAddressMain.elementStateSize));
  stateEntryAddressMain.crc = crc;
  stateEntryAddressBackup.crc = crc;

  // init preambles and headers:
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble),
         &stateEntryAddressMain,
         sizeof(stateEntryAddressMain));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble) + sizeof(Supla::StateEntryAddress),
         &stateEntryAddressBackup,
         sizeof(stateEntryAddressBackup));

  // init actual storage data at "secondSlotAddress" - latest one
  Supla::StateWlByteHeader slotHeader = {};
  int valueElements[2] = {123, 456};
  slotHeader.writeCount = 2;
  // set invalid crc
  slotHeader.crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(valueElements),
                         sizeof(valueElements));
  memcpy(storage.storageSimulatorData + secondSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress" - invalid crc
  slotHeader.writeCount = 1;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc = 1 + calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());

  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 123);
  EXPECT_EQ(el2.stateValue, 456);
}

TEST(StorageStateWlByteTests, loadDataFromStorageWithBothSlotsSecondInvalid2) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, 92, 0, 0};

  // main state address:
  uint32_t firstSlotAddress = sizeof(Supla::Preamble) +
                              sizeof(Supla::SectionPreamble) +
                              2 * sizeof(Supla::StateEntryAddress);
  uint32_t secondSlotAddress = firstSlotAddress +
                                1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8};
  stateEntryAddressMain.address = secondSlotAddress;
  stateEntryAddressBackup.address = secondSlotAddress;

  uint16_t crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddressMain),
                     sizeof(stateEntryAddressMain.address) +
                     sizeof(stateEntryAddressMain.elementStateSize));
  stateEntryAddressMain.crc = crc;
  stateEntryAddressBackup.crc = crc;

  // init preambles and headers:
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble),
         &stateEntryAddressMain,
         sizeof(stateEntryAddressMain));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble) + sizeof(Supla::StateEntryAddress),
         &stateEntryAddressBackup,
         sizeof(stateEntryAddressBackup));

  // init actual storage data at "secondSlotAddress"
  Supla::StateWlByteHeader slotHeader = {};
  int valueElements[2] = {123, 456};
  slotHeader.writeCount = 2;
  // set invalid crc
  slotHeader.crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(valueElements),
                         sizeof(valueElements));
  memcpy(storage.storageSimulatorData + secondSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress" - latest one, invalid crc
  slotHeader.writeCount = 3;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc = 1 + calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());

  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 123);
  EXPECT_EQ(el2.stateValue, 456);
}

TEST(StorageStateWlByteTests, loadDataFromStorageWithBothSlotsInvalid) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;

  StorageMockSimulator storage(
      0, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreambleMemInit = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, 92, 0, 0};

  // main state address:
  uint32_t firstSlotAddress = sizeof(Supla::Preamble) +
                              sizeof(Supla::SectionPreamble) +
                              2 * sizeof(Supla::StateEntryAddress);
  uint32_t secondSlotAddress = firstSlotAddress +
                                1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8};
  stateEntryAddressMain.address = secondSlotAddress;
  stateEntryAddressBackup.address = secondSlotAddress;

  uint16_t crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(&stateEntryAddressMain),
                     sizeof(stateEntryAddressMain.address) +
                     sizeof(stateEntryAddressMain.elementStateSize));
  stateEntryAddressMain.crc = crc;
  stateEntryAddressBackup.crc = crc;

  // init preambles and headers:
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreambleMemInit,
         sizeof(sectionPreambleMemInit));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble),
         &stateEntryAddressMain,
         sizeof(stateEntryAddressMain));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble) +
             sizeof(Supla::SectionPreamble) + sizeof(Supla::StateEntryAddress),
         &stateEntryAddressBackup,
         sizeof(stateEntryAddressBackup));

  // init actual storage data at "secondSlotAddress" - invalid crc
  Supla::StateWlByteHeader slotHeader = {};
  int valueElements[2] = {123, 456};
  slotHeader.writeCount = 2;
  slotHeader.crc =
      1 + calculateCrc16(reinterpret_cast<const uint8_t *>(valueElements),
                         sizeof(valueElements));
  memcpy(storage.storageSimulatorData + secondSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress" - latest one, invalid crc
  slotHeader.writeCount = 3;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc = 1 + calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress, &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements, sizeof(valueElements));

  EXPECT_CALL(storage, commit()).Times(1);

  EXPECT_TRUE(Supla::Storage::Init());

  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, -1);
  EXPECT_EQ(el2.stateValue, -1);

  el1.stateValue = 10;
  el2.stateValue = 20;
  Supla::Storage::WriteStateStorage();
  el1.stateValue = 0;
  el2.stateValue = 0;

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 10);
  EXPECT_EQ(el2.stateValue, 20);
}
