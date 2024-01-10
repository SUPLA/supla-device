/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef EXTRAS_TEST_DOUBLES_STORAGE_MOCK_H_
#define EXTRAS_TEST_DOUBLES_STORAGE_MOCK_H_

#include <gmock/gmock.h>
#include <supla/storage/storage.h>
#include "supla/storage/state_wear_leveling_byte.h"

class StorageMock: public Supla::Storage {
 public:
  MOCK_METHOD(void, scheduleSave, (uint32_t), (override));
  MOCK_METHOD(void, commit, (), (override));
  MOCK_METHOD(int,
              readStorage,
              (unsigned int, unsigned char *, int, bool),
              (override));
  MOCK_METHOD(int,
              writeStorage,
              (unsigned int, const unsigned char *, int),
              (override));

  void defaultInitialization(int elementStateSize = 0);
};

#define STORAGE_SIMULATOR_SIZE 50000

class StorageMockSimulator: public Supla::Storage {
 public:
  MOCK_METHOD(void, commit, (), (override));

  StorageMockSimulator(uint32_t offset = 0,
                       uint32_t size = 0,
                       enum Supla::Storage::WearLevelingMode mode =
                           Supla::Storage::WearLevelingMode::OFF);

  int readStorage(unsigned int offset,
                  unsigned char *data,
                  int size,
                  bool log) override;
  int writeStorage(unsigned int offset,
                   const unsigned char *data,
                   int size) override;

  bool isEmpty();
  bool isPreampleInitialized(int sectionCount = 1);
  bool isEmptySimpleStatePreamplePresent();
  Supla::SectionPreamble *getSectionPreamble();
  Supla::StateEntryAddress *getStateEntryAddress(bool backup = false);
  uint8_t storageSimulatorData[STORAGE_SIMULATOR_SIZE] = {};
  bool noWriteExpected = false;
};

#endif  // EXTRAS_TEST_DOUBLES_STORAGE_MOCK_H_
