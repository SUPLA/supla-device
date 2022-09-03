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

#include "esp_idf_gpio.h"

#include <driver/gpio.h>
#include <supla-common/log.h>
#include <supla/definitions.h>
#include <supla/io.h>
#include <supla/log_wrapper.h>

#ifndef ESP_PLATFORM
#error This file is for ESP-IDF platform
#endif

#ifndef SUPLA_DEVICE_ESP32
// ESP8266 RTOS SDK doesn't provide some methods, so we add empty implementation

void gpio_hold_en(gpio_num_t gpio) {
  (void)(gpio);
}

void gpio_hold_dis(gpio_num_t gpio) {
  (void)(gpio);
}
#endif

void pinMode(uint8_t pin, uint8_t mode) {
  SUPLA_LOG_DEBUG(" *** GPIO %d set mode %d", pin, mode);

  gpio_config_t cfg = {};
  cfg.pin_bit_mask = (1ULL << pin);
  switch (mode) {
    case INPUT: {
      cfg.mode = GPIO_MODE_INPUT;
      gpio_num_t gpio = static_cast<gpio_num_t>(pin);
      gpio_hold_dis(gpio);
      break;
    }
    case OUTPUT: {
      // in OUTPUT mode we also want to read GPIO value
      cfg.mode = static_cast<gpio_mode_t>((GPIO_MODE_DEF_INPUT) |
                                          (GPIO_MODE_DEF_OUTPUT));
      break;
    }
    case INPUT_PULLUP: {
      cfg.mode = GPIO_MODE_INPUT;
      cfg.pull_up_en = GPIO_PULLUP_ENABLE;
      gpio_num_t gpio = static_cast<gpio_num_t>(pin);
      gpio_hold_dis(gpio);
      break;
    }
    default: {
      SUPLA_LOG_ERROR("GPIO pinMode: mode %d is not implemented", mode);
      break;
    }
  }

  gpio_config(&cfg);
}

int digitalRead(uint8_t pin) {
  return gpio_get_level(static_cast<gpio_num_t>(pin));
}

void digitalWrite(uint8_t pin, uint8_t val) {
  gpio_num_t gpio = static_cast<gpio_num_t>(pin);
  gpio_hold_dis(gpio);
  gpio_set_level(gpio, val);
  gpio_hold_en(gpio);
}

void analogWrite(uint8_t pin, int val) {
  SUPLA_LOG_ERROR(
      " *** NOT IMPLEMENTED *** GPIO %d analog write %d", pin, val);
}

int analogRead(uint8_t pin) {
  SUPLA_LOG_ERROR(
      " *** NOT IMPLEMENTED *** GPIO %d analog read", pin);
  return 0;
}


unsigned int pulseIn(uint8_t pin, uint8_t val, uint64_t timeoutMicro) {
  SUPLA_LOG_ERROR(" *** NOT IMPLEMENTED *** GPIO %d pulse in %d", pin, val);
  return 0;
}
