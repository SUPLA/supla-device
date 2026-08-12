// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "crc8.h"

uint8_t crc8(uint8_t *bytes, int size) {
  uint8_t crc = 0;

  for (int j = 0; j < size; j++) {
    crc ^= bytes[j];

    for (int i = 0; i < 8; i++) {
      if ((crc & 0x80) != 0)
        crc = (crc << 1) ^ static_cast<uint8_t>(0x7);
      else
        crc <<= 1;
    }
  }
  return crc;
}

