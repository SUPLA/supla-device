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

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_GPIO_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_GPIO_H_

#include <stdint.h>

void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t val);
void analogWrite(uint8_t pin, int val);
int analogRead(uint8_t pin);
unsigned int pulseIn(uint8_t pin, uint8_t val, uint64_t timeoutMicro);

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_GPIO_H_
