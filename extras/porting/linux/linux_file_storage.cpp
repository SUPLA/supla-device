// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_file_storage.h"

#include <supla/log_wrapper.h>

#include <assert.h>

#include <cstring>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <string>

namespace Supla {

LinuxFileStorage::LinuxFileStorage(const std::string &path,
                                   unsigned int storageStartingOffset,
                                   unsigned int reservedSize)
    : Storage(storageStartingOffset), reservedSize(reservedSize),
      path(path) {
}

LinuxFileStorage::~LinuxFileStorage() {
  if (data) {
    delete[] data;
    data = nullptr;
  }
}

bool LinuxFileStorage::init() {
  data = new unsigned char[reservedSize];
  memset(data, 0, reservedSize);

  std::string filePath = path + "/state.bin";
  SUPLA_LOG_INFO("Storage state file: %s", filePath.c_str());

  std::ifstream stateFile(filePath, std::ifstream::in | std::ios::binary);

  unsigned char c = stateFile.get();

  for (unsigned int i = 0; stateFile.good() && i < reservedSize; i++) {
    data[i] = c;
    c = stateFile.get();
  }

  stateFile.close();

  return Storage::init();
}

int LinuxFileStorage::readStorage(unsigned int offset,
                        unsigned char *buf,
                        unsigned int size,
                        bool logs) {
  (void)(logs);
  for (unsigned int i = 0; i < size; i++) {
    assert(offset + i < reservedSize && "Too small state Storage");
    buf[i] = data[offset + i];
  }
  return size;
}

int LinuxFileStorage::writeStorage(unsigned int offset,
                         const unsigned char *buf,
                         unsigned int size) {
  dataChanged = true;
  for (unsigned int i = 0; i < size; i++) {
    assert(offset + i < reservedSize && "Too small state Storage");
    data[offset + i] = buf[i];
  }

  return size;
}

void LinuxFileStorage::commit() {
  if (dataChanged) {
    std::ofstream stateFile(path + "/state.bin",
        std::ofstream::out | std::ios::binary);

    for (unsigned int i = 0; i < reservedSize; i++) {
      stateFile << data[i];
    }

    stateFile.close();
    // save to a file
  }
  dataChanged = false;
}

}  // namespace Supla

