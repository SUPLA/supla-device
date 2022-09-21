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

#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include "config.h"
#include "storage.h"

#define SUPLA_STORAGE_VERSION 1

namespace Supla {

static bool storageInitDone = false;
static bool configInitDone = false;
Storage *Storage::instance = nullptr;
Config *Storage::configInstance = nullptr;

Storage *Storage::Instance() {
  return instance;
}

Config *Storage::ConfigInstance() {
  return configInstance;
}

bool Storage::Init() {
  bool result = false;
  if (Instance()) {
    if (!storageInitDone) {
      storageInitDone = true;
      result = Instance()->init();
    }
  } else {
    SUPLA_LOG_DEBUG("Main storage not configured");
  }
  if (ConfigInstance()) {
    if (!configInitDone) {
      configInitDone = true;
      result = ConfigInstance()->init();
    }
  } else {
    SUPLA_LOG_DEBUG("Config storage not configured");
  }
  return result;
}

bool Storage::ReadState(unsigned char *buf, int size) {
  if (Instance()) {
    return Instance()->readState(buf, size);
  }
  return false;
}

bool Storage::WriteState(const unsigned char *buf, int size) {
  if (Instance()) {
    return Instance()->writeState(buf, size);
  }
  return false;
}

bool Storage::PrepareState(bool dryRun) {
  if (Instance()) {
    return Instance()->prepareState(dryRun);
  }
  return false;
}

bool Storage::FinalizeSaveState() {
  if (Instance()) {
    return Instance()->finalizeSaveState();
  }
  return false;
}

bool Storage::SaveStateAllowed(uint64_t ms) {
  if (Instance()) {
    return Instance()->saveStateAllowed(ms);
  }
  return false;
}

void Storage::ScheduleSave(uint64_t delayMs) {
  if (Instance()) {
    Instance()->scheduleSave(delayMs);
  }
}

void Storage::SetConfigInstance(Config *instance) {
  configInstance = instance;
  configInitDone = false;
}

bool Storage::IsConfigStorageAvailable() {
  return (ConfigInstance() != nullptr);
}

Storage::Storage(unsigned int storageStartingOffset)
    : storageStartingOffset(storageStartingOffset),
      deviceConfigOffset(0),
      elementConfigOffset(0),
      elementStateOffset(0),
      deviceConfigSize(0),
      elementConfigSize(0),
      elementStateSize(0),
      currentStateOffset(0),
      newSectionSize(0),
      sectionsCount(0),
      dryRun(false),
      saveStatePeriod(1000),
      lastWriteTimestamp(0) {
  instance = this;
}

Storage::~Storage() {
  instance = nullptr;
  storageInitDone = false;
}

bool Storage::prepareState(bool performDryRun) {
  dryRun = performDryRun;
  newSectionSize = 0;
  currentStateOffset = elementStateOffset + sizeof(SectionPreamble);
  return true;
}

bool Storage::readState(unsigned char *buf, int size) {
  if (elementStateOffset + sizeof(SectionPreamble) + elementStateSize <
      currentStateOffset + size) {
    SUPLA_LOG_DEBUG(
              "Warning! Attempt to read state outside of section size");
    return false;
  }
  currentStateOffset += readStorage(currentStateOffset, buf, size);
  return true;
}

bool Storage::writeState(const unsigned char *buf, int size) {
  newSectionSize += size;

  if (size == 0) {
    return true;
  }

  if (elementStateSize > 0 &&
      elementStateOffset + sizeof(SectionPreamble) + elementStateSize <
          currentStateOffset + size) {
    SUPLA_LOG_DEBUG(
              "Warning! Attempt to write state outside of section size.");
    SUPLA_LOG_DEBUG(
        "Storage: rewriting element state section. All data will be lost.");
    elementStateSize = 0;
    elementStateOffset = 0;
    return false;
  }

  if (dryRun) {
    currentStateOffset += size;
    return true;
  }

  // Calculation of offset for section data - in case sector is missing
  if (elementStateOffset == 0) {
    elementStateOffset = storageStartingOffset + sizeof(Preamble);
    if (deviceConfigOffset != 0) {
      elementStateOffset += sizeof(SectionPreamble) + deviceConfigSize;
    }
    if (elementConfigOffset != 0) {
      elementStateOffset += sizeof(SectionPreamble) + elementConfigSize;
    }
    SUPLA_LOG_DEBUG(
              "Initialization of elementStateOffset: %d",
              elementStateOffset);

    currentStateOffset = elementStateOffset + sizeof(SectionPreamble);

    sectionsCount++;

    // Update Storage preamble with new section count
    SUPLA_LOG_DEBUG("Updating Storage preamble");
    unsigned char suplaTag[] = {'S', 'U', 'P', 'L', 'A'};
    Preamble preamble;
    memcpy(preamble.suplaTag, suplaTag, 5);
    preamble.version = SUPLA_STORAGE_VERSION;
    preamble.sectionsCount = sectionsCount;

    updateStorage(
        storageStartingOffset, (unsigned char *)&preamble, sizeof(preamble));
  }

  currentStateOffset += updateStorage(currentStateOffset, buf, size);

  return true;
}

bool Storage::finalizeSaveState() {
  if (dryRun) {
    dryRun = false;
    if (elementStateSize != newSectionSize) {
      SUPLA_LOG_DEBUG(
                "Element state section size doesn't match current device "
                "configuration");
      elementStateOffset = 0;
      elementStateSize = 0;
      return false;
    }
    return true;
  }

  SectionPreamble preamble;
  preamble.type = STORAGE_SECTION_TYPE_ELEMENT_STATE;
  preamble.size = newSectionSize;
  preamble.crc1 = 0;
  preamble.crc2 = 0;
  // TODO(klew): add crc calculation

  updateStorage(
      elementStateOffset, (unsigned char *)&preamble, sizeof(preamble));

  commit();
  return true;
}

bool Storage::init() {
  SUPLA_LOG_DEBUG("Storage initialization");
  unsigned int currentOffset = storageStartingOffset;
  Preamble preamble = {};
  currentOffset +=
      readStorage(currentOffset, (unsigned char *)&preamble, sizeof(preamble));

  unsigned char suplaTag[] = {'S', 'U', 'P', 'L', 'A'};

  if (memcmp(suplaTag, preamble.suplaTag, 5)) {
    SUPLA_LOG_DEBUG("Storage: missing Supla tag. Rewriting...");

    memcpy(preamble.suplaTag, suplaTag, 5);
    preamble.version = SUPLA_STORAGE_VERSION;
    preamble.sectionsCount = 0;

    writeStorage(
        storageStartingOffset, (unsigned char *)&preamble, sizeof(preamble));
    commit();

  } else if (preamble.version != SUPLA_STORAGE_VERSION) {
    SUPLA_LOG_DEBUG(
              "Storage: storage version [%d] is not supported. Storage not "
              "initialized",
              preamble.version);
    return false;
  } else {
    SUPLA_LOG_DEBUG("Storage: Number of sections %d", preamble.sectionsCount);
  }

  if (preamble.sectionsCount == 0) {
    return true;
  }

  for (int i = 0; i < preamble.sectionsCount; i++) {
    SUPLA_LOG_DEBUG("Reading section: %d", i);
    SectionPreamble section;
    unsigned int sectionOffset = currentOffset;
    currentOffset +=
        readStorage(currentOffset, (unsigned char *)&section, sizeof(section));

    SUPLA_LOG_DEBUG(
              "Section type: %d; size: %d",
              static_cast<int>(section.type),
              section.size);

    if (section.crc1 != section.crc2) {
      SUPLA_LOG_DEBUG(
          "Warning! CRC copies on section doesn't match. Please check your "
          "storage hardware");
    }

    switch (section.type) {
      case STORAGE_SECTION_TYPE_DEVICE_CONFIG: {
        deviceConfigOffset = sectionOffset;
        deviceConfigSize = section.size;
        break;
      }
      case STORAGE_SECTION_TYPE_ELEMENT_CONFIG: {
        elementConfigOffset = sectionOffset;
        elementConfigSize = section.size;
        break;
      }
      case STORAGE_SECTION_TYPE_ELEMENT_STATE: {
        elementStateOffset = sectionOffset;
        elementStateSize = section.size;
        break;
      }
      default: {
        SUPLA_LOG_DEBUG("Warning! Unknown section type");
        break;
      }
    }
    currentOffset += section.size;
  }

  return true;
}

int Storage::updateStorage(unsigned int offset,
                           const unsigned char *buf,
                           int size) {
  if (offset < storageStartingOffset) {
    return 0;
  }

  unsigned char *currentData = new unsigned char[size];
  readStorage(offset, currentData, size, false);

  if (memcmp(currentData, buf, size)) {
    delete[] currentData;
    return writeStorage(offset, buf, size);
  }
  delete[] currentData;
  return size;
}

void Storage::setStateSavePeriod(uint64_t periodMs) {
  if (periodMs < 1000) {
    saveStatePeriod = 1000;
  } else {
    saveStatePeriod = periodMs;
  }
}

bool Storage::saveStateAllowed(uint64_t ms) {
  if (ms - lastWriteTimestamp > saveStatePeriod) {
    lastWriteTimestamp = ms;
    return true;
  }
  return false;
}

void Storage::scheduleSave(uint64_t delayMs) {
  uint64_t currentMs = millis();
  uint64_t newTimestamp = currentMs - saveStatePeriod - 1 + delayMs;
  if (currentMs - lastWriteTimestamp < currentMs - newTimestamp) {
    lastWriteTimestamp = newTimestamp;
  }
}

}  // namespace Supla
