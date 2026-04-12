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

#include <supla/log_wrapper.h>
#include <supla/time.h>

#include "ntc10k.h"

#if defined(PANSTAMP_NRG) || defined(ESP_PLATFORM)
#define ADC_RESOLUTION 0xFFF
#else
#define ADC_RESOLUTION 1023
#endif

#ifdef ARDUINO_ARCH_ESP32
#define Analog_Read_ 4095
#else
#define Analog_Read_ 1023
#endif

namespace Supla {
namespace Sensor {
NTC10k::NTC10k(int pin, float seriesResistor,
                               float nominalResistance,
                               float nominalTemp,
                               float beta,
                               int samples)
                               : pin(pin),
                               seriesResistor(seriesResistor),
                               nominalResistance(nominalResistance),
                               nominalTemp(nominalTemp),
                               beta(beta),
                               samples(samples) {
}

void NTC10k::onInit() {
}

void NTC10k::readSensor() {
  for (int i = 0; i < samples; i++) {
    Analog_Read_Value += analogRead(pin);
    delay(2);
  }
    Analog_Read_Value /= samples;
    float voltage_ = Analog_Read_Value/Analog_Read_;
    float temperature_ = (seriesResistor * (1 /voltege_  - 1));
    temperature /= nominalResistance;
    temperature_ = log(temperature_);
    temperature_ /= beta;
    temperature_ += 1/(nominalTemp + 273.15);
    temperature_ = 1/temperature_;
    temperature_ -= 273.15;
    lastValidTemp = temperature_;
}

double NTC10k::getValue() {
    readSensor();
    return lastValidTemp;
}

void NTC10k::set(double val) {
    lastValidTemp = val;
}

void NTC10k::iterateAlways() {
    if (millis() - lastReadTime > 2000) {
      lastReadTime = millis();
      channel.setNewValue(getValue());
    }
}
};
};