// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * This extension depends on Adafruit FRAM SPI library
 * Please install it from librarary manager in Arduino
 */

#ifndef SRC_SUPLA_STORAGE_FRAM_SPI_H_
#define SRC_SUPLA_STORAGE_FRAM_SPI_H_

#include <SPI.h>
#include <supla/log_wrapper.h>

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
      SUPLA_LOG_INFO("Storage: FRAM found");
    } else {
      SUPLA_LOG_INFO("Storage: FRAM not found");
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
    for (int i = 0; i < size; i++) {
      buf[i] = fram.read8(offset + i);
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
          "FRAM: Read %d bytes [%s] (offset %d)", size, logBuffer, offset);
    }

    return size;
  }

  int writeStorage(unsigned int offset,
                   const unsigned char *buf,
                   unsigned int size) {
    fram.writeEnable(true);
    fram.write(offset, const_cast<uint8_t *>(buf), size);
    fram.writeEnable(false);

    SUPLA_LOG_INFO("FRAM: Wrote %d bytes (offset %d)", size, offset);

    return size;
  }

  Adafruit_FRAM_SPI fram;
};

};  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_FRAM_SPI_H_
