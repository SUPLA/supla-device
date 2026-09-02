// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_GPIO_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_GPIO_H_

#include <stdint.h>

void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t val);
void analogWrite(uint8_t pin, int val);
int analogRead(uint8_t pin);
unsigned int pulseIn(uint8_t pin, uint8_t val, uint64_t timeoutMicro);
void attachInterrupt(uint8_t pin, void (*func)(void), int mode);
void detachInterrupt(uint8_t pin);
uint8_t digitalPinToInterrupt(uint8_t pin);

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_GPIO_H_
