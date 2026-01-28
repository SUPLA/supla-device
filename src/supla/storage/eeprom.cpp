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
  for (int i = 0; i < size; i++) {
    buf[i] = EEPROM.read(offset + i);
  }
  if (logs) {
    static constexpr uint8_t MaxLogBytes = 32;
    static constexpr uint16_t LogBufferSize = MaxLogBytes * 3 + 1;

    uint8_t sizeMax = (size > MaxLogBytes) ? MaxLogBytes : size;

    char logBuffer[LogBufferSize];
    int logSize = 0;

    for (uint8_t i = 0; i < sizeMax && logSize < LogBufferSize - 1; i++) {
      logSize += snprintf(
          logBuffer + logSize, LogBufferSize - logSize, "%02X ", buf[i]);
    }

    SUPLA_LOG_INFO(
        "EEPROM: Read %d bytes [%s] (offset %d)", sizeMax, logBuffer, offset);
  }

  return size;
}

int Eeprom::writeStorage(unsigned int offset,
                         const unsigned char *buf,
                         unsigned int size) {
  dataChanged = true;
  for (int i = 0; i < size; i++) {
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
