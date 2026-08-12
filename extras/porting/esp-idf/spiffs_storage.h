// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_SPIFFS_STORAGE_H_
#define EXTRAS_PORTING_ESP_IDF_SPIFFS_STORAGE_H_

#include <supla/storage/storage.h>

namespace Supla {

class SpiffsStorage : public Storage {
 public:
  explicit SpiffsStorage(uint32_t size = 512);
  virtual ~SpiffsStorage();
  bool init();
  void commit();

 protected:
  int readStorage(unsigned int address,
                  unsigned char *buf,
                  unsigned int size,
                  bool logs);
  int writeStorage(unsigned int address,
                   const unsigned char *buf,
                   unsigned int size);

  bool dataChanged = false;
  bool spiffsInitDone = false;
  char *buffer = nullptr;
  uint32_t bufferSize = 0;
};

};  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_SPIFFS_STORAGE_H_
