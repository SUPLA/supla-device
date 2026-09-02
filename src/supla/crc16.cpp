// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "crc16.h"

uint16_t crc16_update(uint16_t crc, uint8_t a) {
  int i;

  crc ^= a;
  for (i = 0; i < 8; ++i) {
    if (crc & 1)
      crc = (crc >> 1) ^ 0xA001;
    else
      crc = (crc >> 1);
  }

  return crc;
}

uint16_t calculateCrc16(const uint8_t *data, int size) {
  if (data == nullptr) {
    return 0;
  }

  uint16_t crc = 0xFFFF;
  for (int i = 0; i < size; i++) {
    crc = crc16_update(crc, data[i]);
  }

  return crc;
}
