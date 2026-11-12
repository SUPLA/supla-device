// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/storage/storage.h>
#include <storage_mock.h>
#include <string.h>
#include <stdio.h>
#include <element_with_storage.h>

class TestableStorageMock : public StorageMock {
 public:
  using Supla::Storage::updateStorage;
};

class FixedFactoryStorageSimulator : public StorageMockSimulator {
 public:
  using Factory = StateStorageFactory;
  using Supla::Storage::CreateSimpleStateStorage;
  using Supla::Storage::CreateByteStateStorage;
  using Supla::Storage::CreateSectorStateStorage;

  FixedFactoryStorageSimulator(uint32_t offset, uint32_t size,
                               Factory factory)
      : StorageMockSimulator(offset, size, factory) {}

  bool reinitialize() {
    return Supla::Storage::init();
  }
};

TEST(StorageStateTests, fixedFactoriesPreserveBackendOnReinitialization) {
  const struct {
    FixedFactoryStorageSimulator::Factory factory;
    uint8_t sectionType;
  } cases[] = {
      {FixedFactoryStorageSimulator::CreateSimpleStateStorage,
       STORAGE_SECTION_TYPE_ELEMENT_STATE},
      {FixedFactoryStorageSimulator::CreateByteStateStorage,
       STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_BYTE},
      {FixedFactoryStorageSimulator::CreateSectorStateStorage,
       STORAGE_SECTION_TYPE_ELEMENT_STATE_WL_SECTOR},
  };

  for (const auto& testCase : cases) {
    SCOPED_TRACE(testCase.sectionType);
    testing::NiceMock<FixedFactoryStorageSimulator> storage(
        0, 5 * 4096, testCase.factory);
    ASSERT_TRUE(Supla::Storage::Init());
    EXPECT_EQ(testCase.sectionType, storage.getSectionPreamble()->type);
    ASSERT_TRUE(storage.reinitialize());
    EXPECT_EQ(testCase.sectionType, storage.getSectionPreamble()->type);
  }
}

TEST(StorageStateTests, fixedFactoryReceivesOffsetsAndCanRetryAfterFailure) {
  static int calls = 0;
  calls = 0;
  auto factory = [](Supla::Storage* storage, uint32_t offset, uint32_t size)
      -> Supla::StateStorageInterface* {
    EXPECT_EQ(Supla::Storage::Instance(), storage);
    EXPECT_EQ(128 + sizeof(Supla::Preamble), offset);
    EXPECT_EQ(8192u, size);
    if (++calls == 1) {
      return nullptr;
    }
    return FixedFactoryStorageSimulator::CreateSimpleStateStorage(
        storage, offset, size);
  };
  testing::NiceMock<FixedFactoryStorageSimulator> storage(
      128, 8192, factory);
  EXPECT_FALSE(storage.reinitialize());
  ASSERT_TRUE(storage.reinitialize());
  EXPECT_EQ(2, calls);
  EXPECT_EQ(STORAGE_SECTION_TYPE_ELEMENT_STATE,
            storage.getSectionPreamble()->type);
}

TEST(StorageStateTests, invalidLegacyModeAndNullFactoryFailInitialization) {
  using Mode = Supla::Storage::WearLevelingMode;
  {
    testing::NiceMock<StorageMockSimulator> storage(
        0, 0, static_cast<Mode>(99));
    EXPECT_FALSE(Supla::Storage::Init());
  }
  {
    testing::NiceMock<FixedFactoryStorageSimulator> storage(
        0, 0, nullptr);
    EXPECT_FALSE(Supla::Storage::Init());
  }
}

TEST(StorageStateTests, preambleInitialization) {
  EXPECT_FALSE(Supla::Storage::Init());

  StorageMockSimulator storage;

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 0;

  EXPECT_CALL(storage, commit()).Times(1);

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(Supla::Storage::Init());

  EXPECT_TRUE(storage.isPreampleInitialized());
}

TEST(StorageStateTests, preambleAlreadyInitialized) {
  EXPECT_FALSE(Supla::Storage::Init());
  StorageMockSimulator storage;

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  Supla::SectionPreamble sectionPreamble = {STORAGE_SECTION_TYPE_ELEMENT_STATE,
                                            0,
                                            0,
                                            0};

  EXPECT_TRUE(storage.isEmpty());
  memcpy(storage.storageSimulatorData, &preamble, sizeof(preamble));
  memcpy(storage.storageSimulatorData + sizeof(Supla::Preamble),
         &sectionPreamble,
         sizeof(sectionPreamble));

  EXPECT_CALL(storage, commit()).Times(0);

  EXPECT_TRUE(storage.isPreampleInitialized(1));
  EXPECT_TRUE(storage.isEmptySimpleStatePreamplePresent());
  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_TRUE(storage.isPreampleInitialized(1));
  EXPECT_TRUE(storage.isEmptySimpleStatePreamplePresent());
}

TEST(StorageStateTests, invalidPreambleAlreadyInitialized) {
  EXPECT_FALSE(Supla::Storage::Init());

  StorageMockSimulator storage;

  Supla::Preamble invalidPreamble;
  memcpy(invalidPreamble.suplaTag, "SuPLa", 5);
  invalidPreamble.version = 1;
  invalidPreamble.sectionsCount = 0;

  EXPECT_TRUE(storage.isEmpty());
  memcpy(storage.storageSimulatorData, &invalidPreamble, 8);
  EXPECT_FALSE(storage.isPreampleInitialized(0));

  EXPECT_CALL(storage, commit()).Times(1);

  EXPECT_TRUE(Supla::Storage::Init());
  EXPECT_TRUE(storage.isPreampleInitialized(1));
  EXPECT_TRUE(storage.isEmptySimpleStatePreamplePresent());
}


TEST(StorageStateTests, preambleInitializationWithElement) {
  EXPECT_FALSE(Supla::Storage::Init());

  StorageMockSimulator storage;
  ElementWithStorage el;

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 0;

  EXPECT_TRUE(storage.isEmpty());
  memcpy(storage.storageSimulatorData, &preamble, 8);
  EXPECT_TRUE(storage.isPreampleInitialized(0));

  EXPECT_CALL(storage, commit()).Times(3);

  EXPECT_TRUE(Supla::Storage::Init());
  ASSERT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();

  EXPECT_EQ(el.stateValue, -1);

  el.stateValue = 123456;

  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el.stateValue, -1);

  el.stateValue = 123456;
  Supla::Storage::WriteStateStorage();
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el.stateValue, 123456);

  EXPECT_TRUE(storage.isPreampleInitialized(1));

  Supla::SectionPreamble secPreamble = {};
  secPreamble.type = STORAGE_SECTION_TYPE_ELEMENT_STATE;
  secPreamble.size = 4;
  secPreamble.crc1 = 17076;
  secPreamble.crc2 = 17076;
  EXPECT_EQ(memcmp(&secPreamble, storage.storageSimulatorData + 8, 7), 0);
}

TEST(StorageStateTests, updateStorageWritesAfterShortRead) {
  TestableStorageMock storage;
  const unsigned char expectedData[] = {0x12, 0x34, 0x56, 0x78};

  EXPECT_CALL(storage,
              readStorage(0, ::testing::_, sizeof(expectedData), false))
      .WillOnce([&expectedData](unsigned int, unsigned char *data,
                                unsigned int size, bool) {
        // Make the old implementation's memcmp equal deterministically even
        // though the storage reports a short read.
        memcpy(data, expectedData, size);
        return static_cast<int>(size - 1);
      });
  EXPECT_CALL(storage,
              writeStorage(0, ::testing::_, sizeof(expectedData)))
      .WillOnce([&expectedData](unsigned int, const unsigned char *data,
                               unsigned int size) {
        EXPECT_EQ(memcmp(data, expectedData, size), 0);
        return static_cast<int>(size);
      });

  EXPECT_EQ(storage.updateStorage(0, expectedData, sizeof(expectedData)),
            sizeof(expectedData));
}

TEST(StorageStateTests, preambleAlreadyInitializedWithElement) {
  EXPECT_FALSE(Supla::Storage::Init());

  StorageMockSimulator storage;
  ElementWithStorage el;

  Supla::Preamble preamble;
  memcpy(preamble.suplaTag, "SUPLA", 5);
  preamble.version = 1;
  preamble.sectionsCount = 1;

  EXPECT_TRUE(storage.isEmpty());
  memcpy(storage.storageSimulatorData, &preamble, 8);
  EXPECT_TRUE(storage.isPreampleInitialized(1));

  Supla::SectionPreamble secPreamble = {};
  secPreamble.type = STORAGE_SECTION_TYPE_ELEMENT_STATE;
  secPreamble.size = 4;
  secPreamble.crc1 = 17076;
  secPreamble.crc2 = 17076;

  memcpy(storage.storageSimulatorData + 8, &secPreamble, 7);

  int32_t valueInStorage = 123456;
  memcpy(storage.storageSimulatorData + 15, &valueInStorage, 4);

  EXPECT_CALL(storage, commit()).Times(2);

  EXPECT_TRUE(Supla::Storage::Init());
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());

  EXPECT_EQ(el.stateValue, -1);
  Supla::Storage::LoadStateStorage();
  EXPECT_EQ(el.stateValue, 123456);

  storage.noWriteExpected = true;
  // No change in state -> no write operations
  Supla::Storage::WriteStateStorage();

  // Change state and update storage
  el.stateValue = 44;
  storage.noWriteExpected = false;
  Supla::Storage::WriteStateStorage();
  EXPECT_EQ(el.stateValue, 44);
  valueInStorage = 44;
  EXPECT_EQ(memcmp(storage.storageSimulatorData + 15, &valueInStorage, 4), 0);
}
