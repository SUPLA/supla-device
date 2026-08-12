// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_STORAGE_MOCK_H_
#define EXTRAS_TEST_DOUBLES_STORAGE_MOCK_H_

#include <gmock/gmock.h>
#include <supla/storage/storage.h>
#include <supla/storage/state_wear_leveling_byte.h>
#include <supla/storage/state_wear_leveling_sector.h>

class StorageMock: public Supla::Storage {
 public:
  MOCK_METHOD(void, scheduleSave, (uint32_t, uint32_t), (override));
  MOCK_METHOD(void, commit, (), (override));
  MOCK_METHOD(int,
              readStorage,
              (unsigned int, unsigned char *, unsigned int, bool),
              (override));
  MOCK_METHOD(int,
              writeStorage,
              (unsigned int, const unsigned char *, unsigned int),
              (override));

  void defaultInitialization(int elementStateSize = 0);
};

#define STORAGE_SIMULATOR_SIZE 0x90000

class StorageMockSimulator: public Supla::Storage {
 public:
  MOCK_METHOD(void, commit, (), (override));

  StorageMockSimulator(uint32_t offset = 0,
                       uint32_t size = 0,
                       enum Supla::Storage::WearLevelingMode mode =
                           Supla::Storage::WearLevelingMode::OFF);

  int readStorage(unsigned int offset,
                  unsigned char *data,
                  unsigned int size,
                  bool log) override;
  int writeStorage(unsigned int offset,
                   const unsigned char *data,
                   unsigned int size) override;

  bool isEmpty();
  bool isPreampleInitialized(int sectionCount = 1);
  bool isBackupPreampleInitialized(int sectionCount = 1);
  bool isEmptySimpleStatePreamplePresent();
  Supla::SectionPreamble *getSectionPreamble();
  Supla::SectionPreamble *getBackupSectionPreamble();
  Supla::StateEntryAddress *getStateEntryAddress(bool backup = false);
  uint8_t storageSimulatorData[STORAGE_SIMULATOR_SIZE] = {};
  bool noWriteExpected = false;
};

class StorageMockFlashSimulator: public StorageMockSimulator {
 public:
  StorageMockFlashSimulator(uint32_t offset = 0,
                       uint32_t size = 0,
                       enum Supla::Storage::WearLevelingMode mode =
                           Supla::Storage::WearLevelingMode::OFF);

  Supla::StateWlSectorConfig *getStateWlSectorConfig();

  int writeStorage(unsigned int offset,
                   const unsigned char *data,
                   unsigned int size) override;
  void eraseSector(unsigned int address, int size) override;

  bool isEmpty();
};

#endif  // EXTRAS_TEST_DOUBLES_STORAGE_MOCK_H_
