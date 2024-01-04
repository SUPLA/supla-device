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
  MOCK_METHOD(bool, readState, (unsigned char *, int), (override));
  MOCK_METHOD(bool, writeState, (const unsigned char *, int), (override));
  MOCK_METHOD(bool, prepareState, (bool), (override));
  MOCK_METHOD(bool, finalizeSaveState, (), (override));
  MOCK_METHOD(bool, init, (), (override));
};

#define STORAGE_SIMULATOR_SIZE 1000

class StorageMockSimulator: public Supla::Storage {
 public:
  MOCK_METHOD(void, commit, (), (override));

  int readStorage(unsigned int offset,
                  unsigned char *data,
                  int size,
                  bool log) override {
    if (offset + size >= STORAGE_SIMULATOR_SIZE) {
      assert(false && "StorageMockSimulator out of bounds");
      return 0;
    }
    memcpy(data, &storageSimulatorData[offset], size);
    return size;
  }

  int writeStorage(unsigned int offset,
      const unsigned char *data,
      int size) override {
    if (noWriteExpected) {
      assert(false && "StorageMockSimulator write not expected");
    }
    if (offset + size >= STORAGE_SIMULATOR_SIZE) {
      assert(false && "StorageMockSimulator out of bounds");
      return 0;
    }
    memcpy(&storageSimulatorData[offset], data, size);
    return size;
  }

  uint8_t storageSimulatorData[STORAGE_SIMULATOR_SIZE] = {};
  bool noWriteExpected = false;

  bool isEmpty() {
    for (int i = 0; i < STORAGE_SIMULATOR_SIZE; i++) {
      if (storageSimulatorData[i] != 0) {
        return false;
      }
    }
    return true;
  }

  bool isPreampleInitialized(int sectionCount = 1) {
    Supla::Preamble preamble = {{'S', 'U', 'P', 'L', 'A'}, 1, 0};
    preamble.sectionsCount = sectionCount;
    return memcmp(&preamble, storageSimulatorData, sizeof(preamble)) == 0;
  }

  bool isEmptySimpleStatePreamplePresent() {
    if (isPreampleInitialized(1)) {
      Supla::SectionPreamble sectionPreamble = {
          STORAGE_SECTION_TYPE_ELEMENT_STATE, 0, 0, 0};
      return memcmp(&sectionPreamble,
                    storageSimulatorData + sizeof(Supla::Preamble),
                    sizeof(sectionPreamble)) == 0;
    }
    return false;
  }
};

#endif  // EXTRAS_TEST_DOUBLES_STORAGE_MOCK_H_
