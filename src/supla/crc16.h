// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CRC16_H_
#define SRC_SUPLA_CRC16_H_

#include <stdint.h>

uint16_t crc16_update(uint16_t crc, uint8_t a);
uint16_t calculateCrc16(const uint8_t *data, int size);

#endif  // SRC_SUPLA_CRC16_H_
