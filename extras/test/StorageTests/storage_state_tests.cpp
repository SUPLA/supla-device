// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/storage/storage.h>
#include <storage_mock.h>
#include <string.h>
#include <stdio.h>
#include <element_with_storage.h>

namespace {

class StorageWithUpdateAccess : public StorageMock {
 public:
  int updateForTest(unsigned int offset,
                    const unsigned char *data,
                    int size) {
    return updateStorage(offset, data, size);
  }
};

}  // namespace

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

TEST(StorageStateTests, updateWritesWhenStorageReturnsNoExistingBytes) {
  StorageWithUpdateAccess storage;
  const unsigned char value[] = {0x12, 0x34, 0x56};

  EXPECT_CALL(storage,
              readStorage(0, ::testing::_, sizeof(value), ::testing::_))
      .WillOnce(::testing::Return(0));
  EXPECT_CALL(storage,
              writeStorage(0, ::testing::_, sizeof(value)))
      .WillOnce([&value](unsigned int,
                         const unsigned char *data,
                         unsigned int size) {
        EXPECT_EQ(size, sizeof(value));
        EXPECT_EQ(memcmp(data, value, sizeof(value)), 0);
        return static_cast<int>(size);
      });

  EXPECT_EQ(storage.updateForTest(0, value, sizeof(value)), sizeof(value));
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
