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

#include "mutex.h"

#if defined(ARDUINO_ARCH_ESP8266) || defined(SUPLA_TEST) || \
    defined(SUPLA_LINUX) || defined(SUPLA_FREERTOS) || defined(ARDUINO_ARCH_AVR)
// TODO(klew): implement mutex for Arduino targets on ESP
Supla::Mutex *Supla::Mutex::Create() {
  // put target specific stuff here
  return new Supla::Mutex;
}
#endif

Supla::Mutex::~Mutex() {
  unlock();
}

Supla::Mutex::Mutex() {
}


void Supla::Mutex::lock() {
}

void Supla::Mutex::unlock() {
}
