// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_TIME_H_
#define SRC_SUPLA_TIME_H_

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdint.h>

uint32_t millis(void);
void delay(uint64_t);
void delayMicroseconds(uint64_t);

#endif

#endif  // SRC_SUPLA_TIME_H_
