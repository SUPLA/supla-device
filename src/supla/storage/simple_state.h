/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

#ifndef SRC_SUPLA_STORAGE_SIMPLE_STATE_H_
#define SRC_SUPLA_STORAGE_SIMPLE_STATE_H_

#include "state_storage_interface.h"

namespace Supla {

struct SectionPreamble;

class SimpleState : public StateStorageInterface {
 public:
  explicit SimpleState(Storage *storage,
                       uint32_t offset,
                       SectionPreamble *preamble);
  ~SimpleState();

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
