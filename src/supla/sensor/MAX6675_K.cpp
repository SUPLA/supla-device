// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef ARDUINO
#include "MAX6675_K.h"
#include <supla/log_wrapper.h>

namespace Supla {
namespace Sensor {
MAX6675_K::MAX6675_K(uint8_t pin_CLK, uint8_t pin_CS, uint8_t pin_DO)
    : pin_CLK(pin_CLK), pin_CS(pin_CS), pin_DO(pin_DO) {
}

double MAX6675_K::getValue() {
  uint16_t value;

  digitalWrite(pin_CS, LOW);
  delay(1);

  value = spiRead();
  value <<= 8;
  value |= spiRead();

  digitalWrite(pin_CS, HIGH);

  if (value & 0x4) {  // this means there is no probe connected to Max6675
    SUPLA_LOG_ERROR("no probe connected to Max6675");
    return TEMPERATURE_NOT_AVAILABLE;
  }
  value >>= 3;

  return value * 0.25;
}

void MAX6675_K::onInit() {
  digitalWrite(pin_CS, HIGH);

  pinMode(pin_CS, OUTPUT);
  pinMode(pin_CLK, OUTPUT);
  pinMode(pin_DO, INPUT);

  channel.setNewValue(getValue());
}

byte MAX6675_K::spiRead() {
  int i;
  byte d = 0;

  for (i = 7; i >= 0; i--) {
    digitalWrite(pin_CLK, LOW);
    delay(1);
    if (digitalRead(pin_DO)) {
      d |= (1 << i);
    }

    digitalWrite(pin_CLK, HIGH);
    delay(1);
  }
  return d;
}

};  // namespace Sensor
};  // namespace Supla
#endif /*ARDUINO*/
