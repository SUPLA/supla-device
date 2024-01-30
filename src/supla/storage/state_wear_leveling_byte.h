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

#ifndef SRC_SUPLA_STORAGE_STATE_WEAR_LEVELING_BYTE_H_
#define SRC_SUPLA_STORAGE_STATE_WEAR_LEVELING_BYTE_H_

/*
  Class used for writing state data storage with wear leveling in byte mode.
  "Byte mode" should be used for random access memory types (like EEPROM), where
  memory allows to write data to any byte without clearing whole sector of
  memory.

  Storage is organized in following way:
  1. Preamble (8 B)
  2. State storage section preamble (7 B)
  3. StateEntryAddress with address of current element state slot and its own
     crc (8 B)
  4. Copy of StateEntryAddress (8 B)
  5. State data with:
     - StateWlByteHeader (4 B)
     - State data (size depends on used Element configuration)

  State data is saved always at slot pointed by StateEntryAddress and to
  followin slot (next one). It is done in order to ensure that state data
  will have backup copy.
  writeCount is incremented each time slot is written.
  On storage init, state is read from slot pointed by StateEntryAddress
  or from next slot after it (depending on which writeCount is higher).
  StateEndryAddress is incremented each time when writeCount is higher
  than repeatBeforeSwitchToAnotherSlot. Then next slot use writeCount == 1.
  Slot with writeCount == 1 is considered as newer than slot with
  writeCount == repeatBeforeSwitchToAnotherSlot.

  repeatBeforeSwitchToAnotherSlot is configured to be equal to number of
  slots in state storage, rounded up to the next odd number.

  Slots are used in round-robin way.

  Exact sequnce of writes is done in a way that there will be always correct
  copy of data stored.
 */

#include "state_storage_interface.h"

namespace Supla {

#pragma pack(push, 1)
// StateEntryAddress is stored after section preamble in two copies.
// It contain address of current element state slot and its own crc.
struct StateEntryAddress {
  uint32_t address;
  uint16_t elementStateSize;
  uint16_t crc;
};

// StateWlByteHeader is stored at the begining of each state data slot.
// It contain information how many times slot was used for writing and
// crc of state data that follows it.
struct StateWlByteHeader {
  uint16_t writeCount;
  uint16_t crc;
};
#pragma pack(pop)

struct SectionPreamble;

class StateWearLevelingByte : public StateStorageInterface {
 public:
  explicit StateWearLevelingByte(Storage *storage,
                       uint32_t offset,
                       SectionPreamble *preamble);
  ~StateWearLevelingByte();

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
  void notifyUpdate() override;

 private:
  void checkIfIsEnoughSpaceForState();
  uint32_t getFirstSlotAddress() const;
  uint32_t getNextSlotAddress(uint32_t slotAddress) const;
  uint32_t slotSize() const;
  uint32_t updateStateEntryAddress();
  bool isDataDifferent(uint32_t firstAddress, uint32_t secondAddress, int size);
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
  bool dataChanged = false;
  int repeatBeforeSwitchToAnotherSlot = 0;

  uint16_t writeCount = 0;
  uint32_t currentSlotAddress = 0;
};

}  // namespace Supla
#endif  // SRC_SUPLA_STORAGE_STATE_WEAR_LEVELING_BYTE_H_
