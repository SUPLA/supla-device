// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef ARDUINO

#include "eeprom.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <stdio.h>
#include <supla/log_wrapper.h>

namespace Supla {

// By default, write to EEPROM every 3 min
#define SUPLA_EEPROM_WRITING_PERIOD 3 * 60 * 1000ul

Eeprom::Eeprom(unsigned int storageStartingOffset, int reservedSize)
    : Storage(storageStartingOffset),
      reservedSize(reservedSize),
      dataChanged(false) {
  setStateSavePeriod(SUPLA_EEPROM_WRITING_PERIOD);
}

bool Eeprom::init() {
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  if (reservedSize <= 0) {
#if defined(ARDUINO_ARCH_ESP8266)
    EEPROM.begin(1024);
#elif defined(ARDUINO_ARCH_ESP32)
    EEPROM.begin(512);
#endif
  } else {
    EEPROM.begin(reservedSize);
  }
  delay(15);
#endif

  return Storage::init();
}

int Eeprom::readStorage(unsigned int offset,
                        unsigned char *buf,
                        unsigned int size,
                        bool logs) {
  for (unsigned int i = 0; i < size; i++) {
    buf[i] = EEPROM.read(offset + i);
  }
  if (logs) {
    static constexpr uint8_t MaxLogBytes = 32;
    static constexpr uint16_t LogBufferSize = MaxLogBytes * 3 + 1;

    uint8_t sizeMax = (size > MaxLogBytes) ? MaxLogBytes : size;

    char logBuffer[LogBufferSize] = {};
    int logSize = 0;

    for (uint8_t i = 0; i < sizeMax && logSize < LogBufferSize - 1; i++) {
      logSize += snprintf(
          logBuffer + logSize, LogBufferSize - logSize, "%02X ", buf[i]);
    }

    SUPLA_LOG_INFO(
        "EEPROM: Read %d bytes [%s] (offset %d)",
        size, logBuffer, offset);
  }

  return size;
}

int Eeprom::writeStorage(unsigned int offset,
                         const unsigned char *buf,
                         unsigned int size) {
  dataChanged = true;
  for (unsigned int i = 0; i < size; i++) {
    EEPROM.write(offset + i, buf[i]);
  }
  SUPLA_LOG_INFO("EEPROM: Wrote %d bytes (offset %d)", size, offset);

  return size;
}

void Eeprom::commit() {
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  if (dataChanged) {
    EEPROM.commit();
    SUPLA_LOG_INFO("EEPROM: Commit");
  }
#endif
  dataChanged = false;
}

}  // namespace Supla

#endif /*ARDUINO*/
