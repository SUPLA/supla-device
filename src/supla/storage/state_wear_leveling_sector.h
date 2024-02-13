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
  First sector 4096 B:
    1. Preamble (8 B)
    2. State storage section preamble (7 B)
    3. StateWlSectorConfig with size of state data and crc (4 B)
    4. All remmaining bytes are used as bitmap to store last written state
       slot number.
  Second sector 4096 B:
    Backup copy of first sector
  Third and next sectors:
    Array of state data slots with:
     - StateWlSectorHeader (2 B) (crc)
     - State data (size depends on used Element configuration)

  On each write, new state data is written to next free slot. If it will not
  fit in current sector, the next sector is erased and data is written to
  current sector and next one. After each save, state bitmap is updated by
  writing bit 0 to next bit in StateBitmapAddress, and then the same is written
  to backup copy of first sector.

  Count of zeroed bits in StateBitmapAddress is a number of last written state
  slot.

 */

#include "state_storage_interface.h"

namespace Supla {

#pragma pack(push, 1)
struct StateWlSectorConfig {
  uint16_t stateSlotSize;
  uint16_t crc;
};
// StateWlSectorHeader is stored at the begining of each state data slot.
// It contain crc of state data that follows it.
struct StateWlSectorHeader {
  uint16_t crc;
};
#pragma pack(pop)

struct SectionPreamble;

class StateWearLevelingSector : public StateStorageInterface {
 public:
  enum class State : uint8_t {
    NONE,
    ERROR,
    SIZE_CHECK,
    WRITE,
    LOAD
  };

  explicit StateWearLevelingSector(Storage *storage,
                       uint32_t offset,  // address of StateWlSectorConfig in
                                         // first sector
                       uint32_t availableSize);
  ~StateWearLevelingSector();

  bool loadPreambles(uint32_t storageStartingOffset, uint16_t size) override;
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
  void notifyUpdate() override;

 protected:
  virtual uint16_t getSectorSize() const;

 private:
  uint16_t getSizeValue(uint16_t availableSize) override;
  bool tryLoadPreamblesFrom(uint32_t offset);
  bool isDataDifferent(uint32_t address, const uint8_t *data, uint32_t size);
  int getSlotSize() const;
  uint32_t getFirstSlotAddress() const;
  uint32_t getNextSlotAddress(uint32_t slotAddress) const;
  uint16_t slotSize() const;
  uint32_t updateStateEntryAddress();
  uint32_t sectionOffset = 0;
  uint32_t availableSize;
  uint16_t elementStateSize = 0xFFFF;
  uint16_t stateSlotNewSize = 0;
  uint32_t currentStateBufferOffset = 0;
  uint16_t crc = 0;   // value calculated on each save/read
  bool elementStateCrcCValid = false;
  bool storageStateOk = false;
  bool initDone = false;
  int repeatBeforeSwitchToAnotherSlot = 0;

  uint32_t currentSlotAddress = 0;
  uint32_t lastStoredSlotAddress = 0;
  uint32_t lastValidAddress = 0;
  uint8_t *dataBuffer = nullptr;

  State state = State::NONE;
};

}  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_STATE_WEAR_LEVELING_SECTOR_H_
