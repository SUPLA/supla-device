/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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

