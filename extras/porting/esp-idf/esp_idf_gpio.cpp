// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_idf_gpio.h"

#include <driver/gpio.h>
#include <supla-common/log.h>
#include <supla/definitions.h>
#include <supla/io.h>
#include <supla/log_wrapper.h>

#ifndef ESP_PLATFORM
#error This file is for ESP-IDF platform
#endif

namespace {

bool gpioSupportsHold(gpio_num_t gpio) {
#ifdef GPIO_IS_VALID_OUTPUT_GPIO
  return GPIO_IS_VALID_OUTPUT_GPIO(gpio);
#elif defined(CONFIG_IDF_TARGET_ESP32)
  return gpio >= 0 && gpio <= 33;
#else
  (void)(gpio);
  return true;
#endif
}

bool gpioSupportsPullUp(gpio_num_t gpio) {
#if defined(CONFIG_IDF_TARGET_ESP32)
  return gpio < 34 || gpio > 39;
#else
  (void)(gpio);
  return true;
#endif
}

void disableGpioHoldIfSupported(gpio_num_t gpio) {
  if (gpioSupportsHold(gpio)) {
    gpio_hold_dis(gpio);
  }
}

void enableGpioHoldIfSupported(gpio_num_t gpio) {
  if (gpioSupportsHold(gpio)) {
    gpio_hold_en(gpio);
  }
}

}  // namespace

void pinMode(uint8_t pin, uint8_t mode) {
  SUPLA_LOG_DEBUG(" *** GPIO %d set mode %d", pin, mode);

  gpio_config_t cfg = {};
  cfg.pin_bit_mask = (1ULL << pin);
  gpio_num_t gpio = static_cast<gpio_num_t>(pin);
  switch (mode) {
    case INPUT: {
      cfg.mode = GPIO_MODE_INPUT;
      disableGpioHoldIfSupported(gpio);
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
      if (gpioSupportsPullUp(gpio)) {
        cfg.pull_up_en = GPIO_PULLUP_ENABLE;
      } else {
        SUPLA_LOG_WARNING(
            "GPIO pinMode: GPIO %d doesn't support internal pull-up", pin);
      }
      disableGpioHoldIfSupported(gpio);
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
  disableGpioHoldIfSupported(gpio);
  gpio_set_level(gpio, val);
  enableGpioHoldIfSupported(gpio);
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

void attachInterrupt(uint8_t pin, void (*func)(void), int mode) {
  (void)(func);
  SUPLA_LOG_ERROR(
      " *** NOT IMPLEMENTED *** GPIO %d attach interrupt %d", pin, mode);
}

void detachInterrupt(uint8_t pin) {
  SUPLA_LOG_ERROR(" *** NOT IMPLEMENTED *** GPIO %d detach interrupt", pin);
}

uint8_t digitalPinToInterrupt(uint8_t pin) {
  return pin;
}
