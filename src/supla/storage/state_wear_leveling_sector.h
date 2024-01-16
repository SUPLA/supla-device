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

#ifndef SRC_SUPLA_STORAGE_STATE_WEAR_LEVELING_SECTOR_H_
#define SRC_SUPLA_STORAGE_STATE_WEAR_LEVELING_SECTOR_H_

/*
  Class used for writing state data storage with wear leveling in sector mode.
  "Sector mode" should be used for memory where write requires earlier
  "sector erase" operation (like flash).

  Storage is organized in following way:
  TBD
 */

#include "state_storage_interface.h"

namespace Supla {

#pragma pack(push, 1)
// StateWlSectorHeader is stored at the begining of each state data slot.
// It contain crc of state data that follows it.
struct StateWlSectorHeader {
  uint16_t crc;
};
#pragma pack(pop)

struct SectionPreamble;

class StateWearLevelingSector : public StateStorageInterface {
 public:
  explicit StateWearLevelingSector(Storage *storage,
                       uint32_t offset,
                       SectionPreamble *preamble);
  ~StateWearLevelingSector();

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
  void checkIfIsEnoughSpaceForState();
  uint32_t getFirstSlotAddress() const;
  uint32_t getNextSlotAddress(uint32_t slotAddress) const;
  uint32_t slotSize() const;
  uint32_t updateStateEntryAddress();
  uint32_t sectionOffset = 0;
  uint32_t reservedSize;
  uint32_t elementStateSize = 0;
  uint32_t stateSlotNewSize = 0;
  uint32_t currentStateOffset = 0;
  uint16_t crc = 0;   // value calculated on each save/read
  bool elementStateCrcCValid = false;
  bool dryRun = false;
  bool storageStateOk = false;
  bool initDone = false;
  int repeatBeforeSwitchToAnotherSlot = 0;

  uint16_t writeCount = 0;
  uint32_t currentSlotAddress = 0;
};

}  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_STATE_WEAR_LEVELING_SECTOR_H_
