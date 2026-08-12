// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_STORAGE_SIMPLE_STATE_H_
#define SRC_SUPLA_STORAGE_SIMPLE_STATE_H_

#include "state_storage_interface.h"

namespace Supla {

struct SectionPreamble;

class SimpleState : public StateStorageInterface {
 public:
  explicit SimpleState(Storage *storage, uint32_t offset);
  ~SimpleState();

  void initSectionPreamble(SectionPreamble *preamble) override;

  bool writeSectionPreamble() override;
  bool initFromStorage() override;
  void deleteAll() override;
  bool prepareSaveState() override;
  bool prepareSizeCheck() override;
  bool prepareLoadState() override;
  bool readState(unsigned char *, int) override;
  bool writeState(const unsigned char *, int) override;
  bool finalizeSaveState() override;
  bool finalizeSizeCheck() override;
  bool finalizeLoadState() override;

 private:
  uint32_t sectionOffset = 0;
  uint32_t elementStateOffset = 0;
  uint32_t elementStateSize = 0;
  uint32_t stateSectionNewSize = 0;
  uint32_t currentStateOffset = 0;
  uint16_t storedCrc = 0;  // CRC value stored in section preamble
  uint16_t crc = 0;   // value calculated on each save/read
  bool elementStateCrcCValid = false;
  bool dryRun = false;
};

}  // namespace Supla
#endif  // SRC_SUPLA_STORAGE_SIMPLE_STATE_H_
