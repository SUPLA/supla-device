// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_LINUX_FILE_STORAGE_H_
#define EXTRAS_PORTING_LINUX_LINUX_FILE_STORAGE_H_

#include <supla/storage/storage.h>

#include <string>

namespace Supla {

class LinuxFileStorage : public Storage {
 public:
  explicit LinuxFileStorage(const std::string &path,
                            unsigned int storageStartingOffset = 0,
                            unsigned int reservedSize = 10000);
  virtual ~LinuxFileStorage();
  bool init() override;
  void commit() override;

 protected:
  int readStorage(unsigned int, unsigned char *, unsigned int, bool) override;
  int writeStorage(unsigned int, const unsigned char *, unsigned int) override;

  unsigned int reservedSize = 0;
  bool dataChanged = false;
  unsigned char *data = nullptr;
  std::string path;
};

};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_FILE_STORAGE_H_
