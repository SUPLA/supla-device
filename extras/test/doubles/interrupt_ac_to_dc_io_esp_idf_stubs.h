// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_INTERRUPT_AC_TO_DC_IO_ESP_IDF_STUBS_H_
#define EXTRAS_TEST_DOUBLES_INTERRUPT_AC_TO_DC_IO_ESP_IDF_STUBS_H_

#include <stdint.h>

#define IRAM_ATTR
#define ESP_INTR_FLAG_IRAM 0
#define ESP_OK 0

using esp_err_t = int;
using gpio_num_t = int;
using portMUX_TYPE = int;

#define portMUX_INITIALIZER_UNLOCKED 0

constexpr int GPIO_INTR_ANYEDGE = 1;
constexpr int GPIO_MODE_INPUT = 2;

struct gpio_config_t {
  int intr_type = 0;
  int mode = 0;
  uint64_t pin_bit_mask = 0;
};

inline void portENTER_CRITICAL(portMUX_TYPE *) {}
inline void portEXIT_CRITICAL(portMUX_TYPE *) {}
inline void portENTER_CRITICAL_ISR(portMUX_TYPE *) {}
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE *) {}

esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_install_isr_service(int flags);
esp_err_t gpio_isr_handler_add(gpio_num_t gpio,
                               void (*handler)(void *),
                               void *arg);
int gpio_get_level(gpio_num_t gpio);

namespace InterruptAcToDcIoTestSupport {
void reset();
void setGpioLevel(int gpio, int level);
void triggerInterrupt(int gpio, int count = 1);
}  // namespace InterruptAcToDcIoTestSupport

#endif  // EXTRAS_TEST_DOUBLES_INTERRUPT_AC_TO_DC_IO_ESP_IDF_STUBS_H_
