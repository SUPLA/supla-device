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

#include "linux_file_storage.h"

#include <supla/log_wrapper.h>

#include <cstring>
#include <filesystem>
#include <fstream>

namespace Supla {

LinuxFileStorage::LinuxFileStorage(const std::string &path,
                                   unsigned int storageStartingOffset,
                                   int reservedSize)
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

  for (int i = 0; stateFile.good() && i < reservedSize; i++) {
    data[i] = c;
    c = stateFile.get();
  }

  stateFile.close();

  return Storage::init();
}

int LinuxFileStorage::readStorage(unsigned int offset,
                        unsigned char *buf,
                        int size,
                        bool logs) {
  for (int i = 0; i < size; i++) {
    buf[i] = data[offset + i];
  }
  return size;
}

int LinuxFileStorage::writeStorage(unsigned int offset,
                         const unsigned char *buf,
                         int size) {
  dataChanged = true;
  for (int i = 0; i < size; i++) {
    data[offset + i] = buf[i];
  }

  return size;
}

void LinuxFileStorage::commit() {
  if (dataChanged) {
    std::ofstream stateFile(path + "/state.bin",
        std::ofstream::out | std::ios::binary);

    for (int i = 0; i < reservedSize; i++) {
      stateFile << data[i];
    }

    stateFile.close();
    // save to a file
  }
  dataChanged = false;
}

}  // namespace Supla

