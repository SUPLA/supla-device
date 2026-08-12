// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_STORAGE_STATE_STORAGE_INTERFACE_H_
#define SRC_SUPLA_STORAGE_STATE_STORAGE_INTERFACE_H_

#include <stdint.h>

namespace Supla {

class Storage;
struct SectionPreamble;

class StateStorageInterface {
 public:
  explicit StateStorageInterface(Storage *storage, uint8_t sectionType);
  virtual ~StateStorageInterface();
  virtual bool loadPreambles(uint32_t storageStartingOffset, uint16_t size);
  virtual void initSectionPreamble(Supla::SectionPreamble *preamble) = 0;
  virtual bool writeSectionPreamble() = 0;
  virtual bool initFromStorage() = 0;
  virtual void deleteAll() = 0;
  virtual bool prepareSaveState() = 0;
  virtual bool prepareSizeCheck() = 0;
  virtual bool prepareLoadState() = 0;
  virtual bool readState(unsigned char *, int) = 0;
  virtual bool writeState(const unsigned char *, int) = 0;
  virtual bool finalizeSaveState() = 0;
  virtual bool finalizeSizeCheck() = 0;
  virtual bool finalizeLoadState() = 0;
  virtual void notifyUpdate();

 protected:
  int readStorage(unsigned int address,
                  unsigned char *buf,
                  int size,
                  bool logs = true);
  int writeStorage(unsigned int address, const unsigned char *buf, int size);
  int updateStorage(unsigned int address, const unsigned char *buf, int size);
  void commit();
  void eraseSector(unsigned int address, int size);
  virtual uint16_t getSizeValue(uint16_t availableSize);

  Storage *storage = nullptr;
  const uint8_t sectionType;
};
}  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_STATE_STORAGE_INTERFACE_H_
