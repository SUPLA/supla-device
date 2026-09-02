// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_STORAGE_EEPROM_H_
#define SRC_SUPLA_STORAGE_EEPROM_H_

#include "storage.h"

namespace Supla {

class Eeprom : public Storage {
 public:
  explicit Eeprom(unsigned int storageStartingOffset = 0,
                  int reservedSize = -1);
  bool init();
  void commit();

 protected:
  int readStorage(unsigned int, unsigned char *, unsigned int, bool);
  int writeStorage(unsigned int, const unsigned char *, unsigned int);

  int reservedSize;
  bool dataChanged;
};

};  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_EEPROM_H_
