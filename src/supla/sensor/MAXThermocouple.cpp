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

#ifdef ARDUINO
#include "MAXThermocouple.h"
#include <supla/log_wrapper.h>

namespace Supla {
namespace Sensor {
MAXThermocouple::MAXThermocouple(uint8_t pin_CLK,
                                 uint8_t pin_CS,
                                 uint8_t pin_DO)
    : pin_CLK(pin_CLK), pin_CS(pin_CS), pin_DO(pin_DO) {
}

double MAXThermocouple::getValue() {
  int32_t value;

  digitalWrite(pin_CS, LOW);
  delay(1);

  value = spiRead();

  digitalWrite(pin_CS, HIGH);

  if ((value >> 16) == (value & 0xffff)) {  // MAX6675
    value >>= 16;

    if ((value & 0x4) ||
        (value <= 0)) {  // this means there is no probe connected to Max6675
      SUPLA_LOG_ERROR("Max6675 Error");
      return TEMPERATURE_NOT_AVAILABLE;
    }
    value >>= 3;

    return static_cast<double>(value) * 0.25;

  } else {  // MAX31855
    if (value & 0x7) {
      SUPLA_LOG_ERROR("Max31855 Error");
      return TEMPERATURE_NOT_AVAILABLE;
    } else {
      //      uint16_t _internTemp = (value >> 4) & 0xfff;

      value >>= 18;
      if (value & 0x2000) {  // is -
        value |= 0xffffc000;
      }

      return static_cast<double>(value) * 0.25;
    }
  }
}

void MAXThermocouple::onInit() {
  digitalWrite(pin_CS, HIGH);

  pinMode(pin_CS, OUTPUT);
  pinMode(pin_CLK, OUTPUT);
  pinMode(pin_DO, INPUT);

  channel.setNewValue(getValue());
}

uint32_t MAXThermocouple::spiRead() {
  uint32_t d = 0;

  for (int i = 31; i >= 0; i--) {
    digitalWrite(pin_CLK, LOW);
    delay(1);
    d <<= 1;
    d |= digitalRead(pin_DO);

    digitalWrite(pin_CLK, HIGH);
    delay(1);
  }
  return d;
}

};     // namespace Sensor
};     // namespace Supla
#endif /*ARDUINO*/
