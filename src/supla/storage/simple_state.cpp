// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/crc16.h>
#include <supla/log_wrapper.h>

#include "simple_state.h"
#include "storage.h"

using Supla::SimpleState;

SimpleState::SimpleState(Storage *storage, uint32_t offset)
    : StateStorageInterface(storage, STORAGE_SECTION_TYPE_ELEMENT_STATE),
      sectionOffset(offset) {
}

SimpleState::~SimpleState() {}

void SimpleState::initSectionPreamble(Supla::SectionPreamble *preamble) {
  if (preamble) {
    elementStateSize = preamble->size;
    if (preamble->crc1 == preamble->crc2) {
      storedCrc = preamble->crc1;
      elementStateCrcCValid = true;
    } else {
      elementStateCrcCValid = false;
    }
  }
}

bool SimpleState::writeSectionPreamble() {
  if (sectionOffset == 0) {
    SUPLA_LOG_WARNING(
        "SimpleState: initSectionPreamble failed, sectionOffset = 0");
    return false;
  }
  if (stateSectionNewSize > UINT16_MAX) {
    SUPLA_LOG_WARNING(
        "SimpleState: initSectionPreamble failed, stateSectionNewSize > "
        "UINT16_MAX");
    return false;
  }

  SectionPreamble preamble = {};
  preamble.type = STORAGE_SECTION_TYPE_ELEMENT_STATE;
  preamble.size = static_cast<uint16_t>(stateSectionNewSize);
  preamble.crc1 = crc;
  preamble.crc2 = crc;
  elementStateSize = stateSectionNewSize;

  updateStorage(sectionOffset,
                reinterpret_cast<unsigned char *>(&preamble),
                sizeof(preamble));

  return true;
}

bool SimpleState::initFromStorage() {
  // calculate crc
  crc = 0xFFFF;
  elementStateOffset = sectionOffset + sizeof(SectionPreamble);
  for (uint32_t i = 0; i < elementStateSize; i++) {
    uint8_t buf = 0;
    readStorage(elementStateOffset + i, &buf, 1, false);
    crc = crc16_update(crc, buf);
  }

  SUPLA_LOG_DEBUG("storedCRC %d, CRC calc %d", storedCrc, crc);

  // check if crc is valid. We ignore checking if storedCrc is 0
  if (crc != storedCrc && storedCrc != 0) {
    SUPLA_LOG_ERROR("Storage: invalid CRC for state data");
    elementStateCrcCValid = false;
  }

  crc = 0;

  return true;
}

void SimpleState::deleteAll() {
  elementStateSize = 0;
  elementStateOffset = 0;
  elementStateCrcCValid = false;
  storedCrc = 0;
  crc = 0;
}

bool SimpleState::prepareSaveState() {
  dryRun = false;
  stateSectionNewSize = 0;
  currentStateOffset = elementStateOffset;
  crc = 0xFFFF;
  return true;
}

bool SimpleState::prepareSizeCheck() {
  dryRun = true;
  stateSectionNewSize = 0;
  currentStateOffset = elementStateOffset;
  crc = 0xFFFF;
  return true;
}

bool SimpleState::prepareLoadState() {
  dryRun = false;
  currentStateOffset = elementStateOffset;
  return true;
}

bool SimpleState::readState(unsigned char *buf, int size) {
  if (!elementStateCrcCValid) {
    // don't read state if CRC was invalid
    return false;
  }

  if (elementStateOffset + elementStateSize < currentStateOffset + size) {
    SUPLA_LOG_DEBUG(
              "Warning! Attempt to read state outside of section size");
    return false;
  }
  currentStateOffset += readStorage(currentStateOffset, buf, size);
  return true;
}

bool SimpleState::writeState(const unsigned char *buf, int size) {
  stateSectionNewSize += size;

  if (size == 0) {
    return true;
  }

  if (elementStateSize > 0 &&
      elementStateOffset + elementStateSize < currentStateOffset + size) {
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
    elementStateOffset = sectionOffset + sizeof(SectionPreamble);
    currentStateOffset = elementStateOffset;
  }

  /*
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
   */
  for (int i = 0; i < size; i++) {
    crc = crc16_update(crc, buf[i]);
  }

  currentStateOffset += updateStorage(currentStateOffset, buf, size);

  return true;
}

bool SimpleState::finalizeSaveState() {
  elementStateSize = stateSectionNewSize;
  writeSectionPreamble();
  commit();
  crc = 0xFFFF;
  elementStateCrcCValid = true;
  return true;
}

bool SimpleState::finalizeSizeCheck() {
  dryRun = false;
  if (elementStateSize != stateSectionNewSize) {
    SUPLA_LOG_DEBUG(
        "Element state section size doesn't match current device "
        "configuration");
    elementStateOffset = 0;
    elementStateSize = 0;
    return false;
  }
  return true;
}

bool SimpleState::finalizeLoadState() {
  return true;
}
