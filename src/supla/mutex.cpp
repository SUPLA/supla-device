// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
