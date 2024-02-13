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

#include "state_storage_interface.h"

#include <supla/log_wrapper.h>
#include <string.h>

#include "storage.h"

using Supla::StateStorageInterface;

StateStorageInterface::StateStorageInterface(Storage *storage,
                                             uint8_t sectionType)
    : storage(storage), sectionType(sectionType) {
}

StateStorageInterface::~StateStorageInterface() {
}

bool StateStorageInterface::loadPreambles(uint32_t storageStartingOffset,
    uint16_t size) {
  unsigned int currentOffset = storageStartingOffset;
  Preamble preamble = {};
  currentOffset += readStorage(currentOffset,
                               reinterpret_cast<unsigned char *>(&preamble),
                               sizeof(preamble));
  const unsigned char suplaTag[] = {'S', 'U', 'P', 'L', 'A'};

  bool suplaTagValid = memcmp(suplaTag, preamble.suplaTag, 5) == 0;

  if (!suplaTagValid || preamble.sectionsCount == 0) {
    if (!suplaTagValid) {
      SUPLA_LOG_DEBUG("Storage: missing Supla tag. Rewriting...");
    } else {
      SUPLA_LOG_DEBUG("Storage: sectionsCount == 0. Rewriting...");
    }
    memcpy(preamble.suplaTag, suplaTag, 5);
    preamble.version = SUPLA_STORAGE_VERSION;
    SectionPreamble section = {};
    section.type = sectionType;
    section.size = getSizeValue(size);

    initSectionPreamble(&section);

    preamble.sectionsCount = 1;

    writeSectionPreamble();
    writeStorage(storageStartingOffset,
        reinterpret_cast<unsigned char *>(&preamble),
        sizeof(preamble));
    commit();

    return true;
  } else if (preamble.version != SUPLA_STORAGE_VERSION) {
    SUPLA_LOG_DEBUG(
              "Storage: storage version [%d] is not supported. Storage not "
              "initialized",
              preamble.version);
    return false;
  }

  SUPLA_LOG_DEBUG("Storage: Number of sections %d", preamble.sectionsCount);

  for (int i = 0; i < preamble.sectionsCount; i++) {
    SUPLA_LOG_DEBUG("Reading section: %d", i);
    SectionPreamble section;
    currentOffset += readStorage(currentOffset,
                                 reinterpret_cast<unsigned char *>(&section),
                                 sizeof(section));

    SUPLA_LOG_DEBUG(
              "Section type: %d; size: %d",
              static_cast<int>(section.type),
              section.size);

    if (section.crc1 != section.crc2) {
      SUPLA_LOG_WARNING(
          "Warning! CRC copies on section doesn't match. Please check your "
          "storage hardware");
    }

    if (sectionType == section.type) {
      initSectionPreamble(&section);
      initFromStorage();
    } else {
      SUPLA_LOG_WARNING("Storage: section type mismatch %d != %d", sectionType,
          section.type);
    }

    currentOffset += section.size;
  }
  return true;
}

int StateStorageInterface::readStorage(unsigned int address,
                                       unsigned char *buf,
                                       int size,
                                       bool logs) {
  if (!storage) {
    return 0;
  }
  return storage->readStorage(address, buf, size, logs);
}

int StateStorageInterface::writeStorage(unsigned int address,
                                        const unsigned char *buf,
                                        int size) {
  if (!storage) {
    return 0;
  }
  return storage->writeStorage(address, buf, size);
}

int StateStorageInterface::updateStorage(unsigned int address,
                                         const unsigned char *buf,
                                         int size) {
  if (!storage) {
    return 0;
  }
  return storage->updateStorage(address, buf, size);
}

void StateStorageInterface::commit() {
  if (!storage) {
    return;
  }
  storage->commit();
}

void StateStorageInterface::eraseSector(unsigned int address, int size) {
  SUPLA_LOG_DEBUG("StateStorageInterface::eraseSector(%d, %d)", address, size);
  if (!storage) {
    return;
  }
  storage->eraseSector(address, size);
}


void StateStorageInterface::notifyUpdate() {
}

uint16_t StateStorageInterface::getSizeValue(uint16_t availableSize) {
  (void)(availableSize);
  return 0;
}
