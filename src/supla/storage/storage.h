/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_STORAGE_STORAGE_H_
#define SRC_SUPLA_STORAGE_STORAGE_H_

#include <stdint.h>

#define STORAGE_SECTION_TYPE_DEVICE_CONFIG  1
#define STORAGE_SECTION_TYPE_ELEMENT_CONFIG 2
#define STORAGE_SECTION_TYPE_ELEMENT_STATE  3

namespace Supla {

class Config;

class Storage {
 public:
  static Storage *Instance();
  static Config *ConfigInstance();
  static bool Init();
  static bool ReadState(unsigned char *, int);
  static bool WriteState(const unsigned char *, int);
  static bool PrepareState(bool dryRun = false);
  static bool FinalizeSaveState();
  static bool SaveStateAllowed(uint64_t);
  static void ScheduleSave(uint64_t delayMs);
  static void SetConfigInstance(Config *instance);
  static bool IsConfigStorageAvailable();

  explicit Storage(unsigned int storageStartingOffset = 0);
  virtual ~Storage();

  // Changes default state save period time
  virtual void setStateSavePeriod(uint64_t periodMs);

  virtual bool init();
  virtual bool readState(unsigned char *, int);
  virtual bool writeState(const unsigned char *, int);

  virtual bool prepareState(bool performDryRun);
  virtual bool finalizeSaveState();
  virtual bool saveStateAllowed(uint64_t);
  virtual void scheduleSave(uint64_t delayMs);

  virtual void commit() = 0;

  virtual void deleteAll();

 protected:
  virtual int readStorage(unsigned int, unsigned char *, int, bool = true) = 0;
  virtual int writeStorage(unsigned int, const unsigned char *, int) = 0;
  virtual int updateStorage(unsigned int, const unsigned char *, int);

  unsigned int storageStartingOffset;
  unsigned int deviceConfigOffset;
  unsigned int elementConfigOffset;
  unsigned int elementStateOffset;

  unsigned int deviceConfigSize;
  unsigned int elementConfigSize;
  unsigned int elementStateSize;

  unsigned int currentStateOffset;

  unsigned int newSectionSize;
  int sectionsCount;
  bool dryRun;

  uint64_t saveStatePeriod;
  uint64_t lastWriteTimestamp;

  static Storage *instance;
  static Config *configInstance;
};

#pragma pack(push, 1)
struct Preamble {
  unsigned char suplaTag[5];
  uint16_t version;
  uint8_t sectionsCount;
};

struct SectionPreamble {
  unsigned char type;
  uint16_t size;
  uint16_t crc1;
  uint16_t crc2;
};
#pragma pack(pop)

};  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_STORAGE_H_
