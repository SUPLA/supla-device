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

#include <SuplaDevice.h>
#include <config_simulator.h>
#include <element_with_storage.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <stdio.h>
#include <storage_mock.h>
#include <string.h>
#include <supla/control/virtual_relay.h>
#include <supla/crc16.h>
#include <supla/storage/state_wear_leveling_byte.h>
#include <supla/storage/storage.h>
#include <timer_mock.h>

#include <algorithm>
#include <vector>

using ::testing::AtLeast;

namespace {

class ReadTrackingStorageMockSimulator : public StorageMockSimulator {
 public:
  using StorageMockSimulator::StorageMockSimulator;

  int readStorage(unsigned int offset,
                  unsigned char *data,
                  unsigned int size,
                  bool log) override {
    readOffsets.push_back(offset);
    return StorageMockSimulator::readStorage(offset, data, size, log);
  }

  std::vector<unsigned int> readOffsets;
};

class InitSuccessfulConfigSimulator : public ConfigSimulator {
 public:
  bool init() override {
    initResult = true;
    return true;
  }
};

uint32_t getTestSectionOffset(uint32_t storageOffset) {
  return storageOffset + sizeof(Supla::Preamble);
}

uint32_t getTestFirstSlotAddress(uint32_t storageOffset) {
  return getTestSectionOffset(storageOffset) + sizeof(Supla::SectionPreamble) +
         2 * sizeof(Supla::StateEntryAddress);
}

uint32_t getTestSlotSize(uint16_t elementStateSize) {
  return sizeof(Supla::StateWlByteHeader) + elementStateSize;
}

uint16_t getStateEntryCrc(const Supla::StateEntryAddress &entry) {
  return calculateCrc16(reinterpret_cast<const uint8_t *>(&entry),
                        sizeof(entry.address) + sizeof(entry.elementStateSize));
}

void initializeStateMetadata(StorageMockSimulator &storage,
                             uint32_t storageOffset,
                             uint16_t reservedSize,
                             Supla::StateEntryAddress main,
                             Supla::StateEntryAddress backup) {
  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreamble = {
      STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE, reservedSize, 0, 0};

  main.crc = getStateEntryCrc(main);
  backup.crc = getStateEntryCrc(backup);

  memcpy(storage.storageSimulatorData + storageOffset,
         &preamble,
         sizeof(preamble));
  memcpy(storage.storageSimulatorData + getTestSectionOffset(storageOffset),
         &sectionPreamble,
         sizeof(sectionPreamble));
  memcpy(storage.storageSimulatorData + getTestSectionOffset(storageOffset) +
             sizeof(sectionPreamble),
         &main,
         sizeof(main));
  memcpy(storage.storageSimulatorData + getTestSectionOffset(storageOffset) +
             sizeof(sectionPreamble) + sizeof(main),
         &backup,
         sizeof(backup));
}

void writeTestStateSlot(StorageMockSimulator &storage,
                        uint32_t address,
                        uint16_t writeCount,
                        int32_t firstValue,
                        int32_t secondValue) {
  int32_t values[2] = {firstValue, secondValue};
  Supla::StateWlByteHeader header = {};
  header.writeCount = writeCount;
  header.crc =
      calculateCrc16(reinterpret_cast<const uint8_t *>(values), sizeof(values));
  memcpy(storage.storageSimulatorData + address, &header, sizeof(header));
  memcpy(storage.storageSimulatorData + address + sizeof(header),
         values,
         sizeof(values));
}

}  // namespace

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
  //  EXPECT_EQ(sectionPreamble->type,
  //  STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE);
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

  Supla::StateEntryAddress stateEntryAddressMain = {address, 8, 0};
  Supla::StateEntryAddress stateEntryAddressBackup = {address, 8, 0};

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
      EXPECT_EQ(el2.stateValue, i + 1);
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
      firstSlotAddress + 3 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8, 0};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8, 0};
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
  memcpy(storage.storageSimulatorData + thirdSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

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
  uint32_t secondSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 3 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8, 0};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8, 0};
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
  memcpy(storage.storageSimulatorData + secondSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress"
  slotHeader.writeCount = 0;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

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
  uint32_t secondSlotAddress =
      firstSlotAddress + 1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8, 0};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8, 0};
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
  memcpy(storage.storageSimulatorData + secondSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress" - latest one
  slotHeader.writeCount = 3;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

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
  uint32_t secondSlotAddress =
      firstSlotAddress + 1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8, 0};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8, 0};
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
  memcpy(storage.storageSimulatorData + secondSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress"
  slotHeader.writeCount = 1;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

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
  uint32_t secondSlotAddress =
      firstSlotAddress + 1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8, 0};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8, 0};
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
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + secondSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress" - invalid crc
  slotHeader.writeCount = 1;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc =
      1 + calculateCrc16(reinterpret_cast<const uint8_t *>(valueElements),
                         sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

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
  uint32_t secondSlotAddress =
      firstSlotAddress + 1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8, 0};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8, 0};
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
  slotHeader.crc = calculateCrc16(
      reinterpret_cast<const uint8_t *>(valueElements), sizeof(valueElements));
  memcpy(storage.storageSimulatorData + secondSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress" - latest one, invalid crc
  slotHeader.writeCount = 3;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc =
      1 + calculateCrc16(reinterpret_cast<const uint8_t *>(valueElements),
                         sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

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
  uint32_t secondSlotAddress =
      firstSlotAddress + 1 * (sizeof(Supla::StateWlByteHeader) + 8);
  uint32_t thirdSlotAddress =
      firstSlotAddress + 2 * (sizeof(Supla::StateWlByteHeader) + 8);

  Supla::StateEntryAddress stateEntryAddressMain = {0, 8, 0};
  Supla::StateEntryAddress stateEntryAddressBackup = {0, 8, 0};
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
  memcpy(storage.storageSimulatorData + secondSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + secondSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

  // init actual storage data at "thirdSlotAddress" - latest one, invalid crc
  slotHeader.writeCount = 3;
  valueElements[0] = 1;
  valueElements[1] = 2;
  slotHeader.crc =
      1 + calculateCrc16(reinterpret_cast<const uint8_t *>(valueElements),
                         sizeof(valueElements));
  memcpy(storage.storageSimulatorData + thirdSlotAddress,
         &slotHeader,
         sizeof(slotHeader));
  memcpy(storage.storageSimulatorData + thirdSlotAddress + sizeof(slotHeader),
         valueElements,
         sizeof(valueElements));

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

TEST(StorageStateWlByteTests, rejectsMetadataWithAddressOverflow) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage element;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  ReadTrackingStorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const Supla::StateEntryAddress entry = {0xffff0000, 8, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);

  storage.noWriteExpected = true;
  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_EQ(std::find(storage.readOffsets.begin(),
                      storage.readOffsets.end(),
                      entry.address),
            storage.readOffsets.end());
  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
}

TEST(StorageStateWlByteTests,
     SuplaDeviceBeginRebuildsInvalidMetadataWithVirtualRelayState) {
  EXPECT_FALSE(Supla::Storage::Init());

  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  InitSuccessfulConfigSimulator config;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const Supla::StateEntryAddress invalidEntry = {0xffff0000, 8, 0};
  initializeStateMetadata(
      storage, storageOffset, reservedSize, invalidEntry, invalidEntry);

  EXPECT_CALL(storage, commit()).Times(AtLeast(1));
  SimpleTime time;
  TimerMock timer;
  Supla::Control::VirtualRelay relay;
  SuplaDeviceClass device;

  EXPECT_CALL(timer, initTimers());
  EXPECT_FALSE(device.begin());

  EXPECT_TRUE(device.getStorageInitResult());
  time.advance(5001);
  device.iterate();

  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());
  const auto *entry = storage.getStateEntryAddress();
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->address, getTestFirstSlotAddress(storageOffset));
  EXPECT_EQ(entry->elementStateSize, 5);
}

TEST(StorageStateWlByteTests, rejectsMetadataBeforeFirstSlot) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage element;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const uint32_t firstSlotAddress = getTestFirstSlotAddress(storageOffset);
  const Supla::StateEntryAddress entry = {firstSlotAddress - 1, 8, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);

  storage.noWriteExpected = true;
  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
}

TEST(StorageStateWlByteTests, rejectsUnalignedMetadataAddress) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage element;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const uint32_t firstSlotAddress = getTestFirstSlotAddress(storageOffset);
  const Supla::StateEntryAddress entry = {firstSlotAddress + 1, 8, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);

  storage.noWriteExpected = true;
  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
}

TEST(StorageStateWlByteTests, rejectsMetadataWhoseSlotExceedsSectionEnd) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage element;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const uint32_t firstSlotAddress = getTestFirstSlotAddress(storageOffset);
  const uint32_t slotSize = getTestSlotSize(8);
  const Supla::StateEntryAddress entry = {
      firstSlotAddress + 5 * slotSize, 8, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);

  storage.noWriteExpected = true;
  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
}

TEST(StorageStateWlByteTests, countsSectionPreambleWhenCheckingSlotSpace) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage element;
  // There is room for only one 12-byte slot after the section preamble and
  // both metadata copies. The old calculation incorrectly accepted two.
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 43;
  StorageMockSimulator storage(
      storageOffset,
      reservedSize + sizeof(Supla::Preamble),
      Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const Supla::StateEntryAddress entry = {
      getTestFirstSlotAddress(storageOffset), 8, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);

  storage.noWriteExpected = true;
  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
}

TEST(StorageStateWlByteTests, rejectsStateSizeThatDoesNotFitTwoSlots) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage element;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const Supla::StateEntryAddress entry = {
      getTestFirstSlotAddress(storageOffset), UINT16_MAX, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);

  storage.noWriteExpected = true;
  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
}

TEST(StorageStateWlByteTests,
     rejectsAddressCalculationOverflowFromLargeAddress) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage element;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const Supla::StateEntryAddress entry = {
      UINT32_MAX - sizeof(Supla::StateWlByteHeader) + 1, 8, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);

  storage.noWriteExpected = true;
  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
}

TEST(StorageStateWlByteTests, rejectsConflictingSizesForSameMetadataAddress) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage element;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const uint32_t firstSlotAddress = getTestFirstSlotAddress(storageOffset);
  const Supla::StateEntryAddress mainEntry = {firstSlotAddress, 8, 0};
  const Supla::StateEntryAddress backupEntry = {firstSlotAddress, 4, 0};
  initializeStateMetadata(
      storage, storageOffset, reservedSize, mainEntry, backupEntry);

  storage.noWriteExpected = true;
  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_FALSE(Supla::Storage::IsStateStorageValid());
}

TEST(StorageStateWlByteTests, acceptsPersistedEmptyStateMetadata) {
  EXPECT_FALSE(Supla::Storage::Init());

  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const Supla::StateEntryAddress entry = {
      getTestFirstSlotAddress(storageOffset), 0, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);

  storage.noWriteExpected = true;
  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());
}

TEST(StorageStateWlByteTests, sizeChangeRebuildsLayoutFromFirstSlot) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage element;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const uint32_t firstSlotAddress = getTestFirstSlotAddress(storageOffset);
  const uint32_t oldSlotSize = getTestSlotSize(8);
  const uint32_t oldSlotAddress = firstSlotAddress + 3 * oldSlotSize;
  const Supla::StateEntryAddress entry = {oldSlotAddress, 8, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);
  writeTestStateSlot(storage, oldSlotAddress, 2, 11, 22);
  writeTestStateSlot(storage, oldSlotAddress + oldSlotSize, 0, 0, 0);

  EXPECT_CALL(storage, commit()).Times(AtLeast(1));

  ASSERT_TRUE(Supla::Storage::Init());
  ASSERT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());

  const auto *updatedEntry = storage.getStateEntryAddress();
  ASSERT_NE(updatedEntry, nullptr);
  EXPECT_EQ(updatedEntry->address, firstSlotAddress);
  EXPECT_EQ(updatedEntry->elementStateSize, sizeof(element.stateValue));
}

TEST(StorageStateWlByteTests, acceptsOnlyValidMainMetadataCopy) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const uint32_t firstSlotAddress = getTestFirstSlotAddress(storageOffset);
  const uint32_t slotSize = getTestSlotSize(8);
  const Supla::StateEntryAddress mainEntry = {firstSlotAddress, 8, 0};
  const Supla::StateEntryAddress backupEntry = {firstSlotAddress + 1, 8, 0};
  initializeStateMetadata(
      storage, storageOffset, reservedSize, mainEntry, backupEntry);
  writeTestStateSlot(storage, firstSlotAddress, 2, 11, 22);
  writeTestStateSlot(storage, firstSlotAddress + slotSize, 0, 0, 0);

  EXPECT_CALL(storage, commit()).Times(0);

  ASSERT_TRUE(Supla::Storage::Init());
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 11);
  EXPECT_EQ(el2.stateValue, 22);
}

TEST(StorageStateWlByteTests, acceptsOnlyValidBackupMetadataCopy) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const uint32_t firstSlotAddress = getTestFirstSlotAddress(storageOffset);
  const uint32_t slotSize = getTestSlotSize(8);
  const Supla::StateEntryAddress mainEntry = {firstSlotAddress + 1, 8, 0};
  const Supla::StateEntryAddress backupEntry = {firstSlotAddress, 8, 0};
  initializeStateMetadata(
      storage, storageOffset, reservedSize, mainEntry, backupEntry);
  writeTestStateSlot(storage, firstSlotAddress, 2, 33, 44);
  writeTestStateSlot(storage, firstSlotAddress + slotSize, 0, 0, 0);

  EXPECT_CALL(storage, commit()).Times(0);

  ASSERT_TRUE(Supla::Storage::Init());
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 33);
  EXPECT_EQ(el2.stateValue, 44);
}

TEST(StorageStateWlByteTests, selectsValidatedBackupNextSlotAfterPowerLoss) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = 92;
  StorageMockSimulator storage(
      storageOffset, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const uint32_t firstSlotAddress = getTestFirstSlotAddress(storageOffset);
  const uint32_t slotSize = getTestSlotSize(8);
  const Supla::StateEntryAddress mainEntry = {firstSlotAddress, 8, 0};
  const Supla::StateEntryAddress backupEntry = {
      firstSlotAddress + slotSize, 8, 0};
  initializeStateMetadata(
      storage, storageOffset, reservedSize, mainEntry, backupEntry);
  writeTestStateSlot(storage, firstSlotAddress, 4, 11, 22);
  writeTestStateSlot(storage, firstSlotAddress + slotSize, 2, 33, 44);

  EXPECT_CALL(storage, commit()).Times(0);

  ASSERT_TRUE(Supla::Storage::Init());
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 33);
  EXPECT_EQ(el2.stateValue, 44);
}

TEST(StorageStateWlByteTests,
     readsAndWritesValidStateWithNonZeroStorageOffset) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;
  StorageMockSimulator storage(
      32, 100, Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);

  EXPECT_CALL(storage, commit()).Times(AtLeast(1));

  ASSERT_TRUE(Supla::Storage::Init());
  ASSERT_FALSE(Supla::Storage::IsStateStorageValid());

  el1.stateValue = 123;
  el2.stateValue = 456;
  Supla::Storage::WriteStateStorage();
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());

  el1.stateValue = 0;
  el2.stateValue = 0;
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 123);
  EXPECT_EQ(el2.stateValue, 456);
}

TEST(StorageStateWlByteTests, acceptsSlotEndingExactlyAtSectionEnd) {
  EXPECT_FALSE(Supla::Storage::Init());

  ElementWithStorage el1;
  ElementWithStorage el2;
  const uint32_t storageOffset = 0;
  const uint16_t reservedSize = sizeof(Supla::SectionPreamble) +
                                2 * sizeof(Supla::StateEntryAddress) +
                                2 * getTestSlotSize(8);
  StorageMockSimulator storage(
      storageOffset,
      reservedSize + sizeof(Supla::Preamble),
      Supla::Storage::WearLevelingMode::BYTE_WRITE_MODE);
  const uint32_t firstSlotAddress = getTestFirstSlotAddress(storageOffset);
  const uint32_t lastSlotAddress = firstSlotAddress + getTestSlotSize(8);
  const Supla::StateEntryAddress entry = {lastSlotAddress, 8, 0};
  initializeStateMetadata(storage, storageOffset, reservedSize, entry, entry);
  writeTestStateSlot(storage, firstSlotAddress, 0, 0, 0);
  writeTestStateSlot(storage, lastSlotAddress, 2, 55, 66);

  EXPECT_CALL(storage, commit()).Times(0);

  ASSERT_TRUE(Supla::Storage::Init());
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el1.stateValue, 55);
  EXPECT_EQ(el2.stateValue, 66);
}
