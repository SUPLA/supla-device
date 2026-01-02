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

/*
 * This extension depends on Adafruit FRAM SPI library
 * Please install it from librarary manager in Arduino
 */

#ifndef SRC_SUPLA_STORAGE_FRAM_SPI_H_
#define SRC_SUPLA_STORAGE_FRAM_SPI_H_

#include <SPI.h>

#include "Adafruit_FRAM_SPI.h"
#include "storage.h"

#define SUPLA_FRAM_WRITING_PERIOD 1000

namespace Supla {

class FramSpi : public Storage {
 public:
  FramSpi(int8_t clk,
          int8_t miso,
          int8_t mosi,
          int8_t framCs,
          unsigned int storageStartingOffset = 0)
      : Storage(storageStartingOffset), fram(clk, miso, mosi, framCs) {
    setStateSavePeriod(SUPLA_FRAM_WRITING_PERIOD);
  }

  explicit FramSpi(int8_t framCs, unsigned int storageStartingOffset = 0)
      : Storage(storageStartingOffset), fram(framCs) {
    setStateSavePeriod(SUPLA_FRAM_WRITING_PERIOD);
  }

  bool init() {
    if (fram.begin()) {
      Serial.println(F("Storage: FRAM found"));
    } else {
      Serial.println(F("Storage: FRAM not found"));
    }

    return Storage::init();
  }

  void commit() {
  }

 protected:
  int readStorage(unsigned int offset,
                  unsigned char *buf,
                  unsigned int size,
                  bool logs) {
    if (logs) {
      Serial.print(F("readStorage: "));
      Serial.print(size);
      Serial.print(F("; Read: ["));
    }
    for (int i = 0; i < size; i++) {
      buf[i] = fram.read8(offset + i);
      if (logs) {
        Serial.print(static_cast<unsigned char *>(buf)[i], HEX);
        Serial.print(F(" "));
      }
    }
    if (logs) {
      Serial.println(F("]"));
    }
    return size;
  }

  int writeStorage(unsigned int offset,
                   const unsigned char *buf,
                   unsigned int size) {
    fram.writeEnable(true);
    fram.write(offset, const_cast<uint8_t *>(buf), size);
    fram.writeEnable(false);
    Serial.print(F("Wrote "));
    Serial.print(size);
    Serial.print(F(" bytes to storage at "));
    Serial.println(offset);
    return size;
  }

  Adafruit_FRAM_SPI fram;
};

};  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_FRAM_SPI_H_
