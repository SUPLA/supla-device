/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "interrupt_ac_to_dc_io.h"

#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <driver/gpio.h>
#include <hal/gpio_types.h>
#include <esp_attr.h>
#include <esp_intr_alloc.h>

using Supla::InterruptAcToDcIo;

namespace {
static volatile uint8_t gpioInterrupts[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};

void IRAM_ATTR interruptHandler(void* arg) {
  uint32_t gpioNum = reinterpret_cast<uint32_t>(arg);
  if (gpioNum < INTERRUPT_AC_TO_DC_IO_MAX_GPIOS) {
    gpioInterrupts[gpioNum] = 1;
  }
}

}  // namespace

InterruptAcToDcIo::InterruptAcToDcIo() : Supla::Io(false) {
  for (int i = 0; i < INTERRUPT_AC_TO_DC_IO_MAX_GPIOS; i++) {
    gpioState[i] = 255;
  }
}

InterruptAcToDcIo::~InterruptAcToDcIo() {
}

void InterruptAcToDcIo::addGpio(int gpio,
                                int32_t minOffTimeoutMs) {
  if (isInitialized()) {
    SUPLA_LOG_ERROR("InterruptAcToDcIo already initialized - can't add GPIO");
    return;
  }

  if (gpio < 0 || gpio >= INTERRUPT_AC_TO_DC_IO_MAX_GPIOS) {
    SUPLA_LOG_ERROR("InterruptAcToDcIo: Invalid GPIO number %d", gpio);
    return;
  }

  gpioMinOffTimeout[gpio] = minOffTimeoutMs;
  gpioState[gpio] = 0;
}

void InterruptAcToDcIo::initialize() {
  uint64_t gpioMask = 0;
  for (int i = 0; i < INTERRUPT_AC_TO_DC_IO_MAX_GPIOS; i++) {
    if (gpioState[i] != 255) {
      gpioMask |= 1ULL << i;
    }
  }
  gpio_config_t ioConf = {};
  ioConf.intr_type = GPIO_INTR_ANYEDGE;
  ioConf.mode = GPIO_MODE_INPUT;
//  ioConf.pull_up_en = GPIO_PULLUP_ENABLE;
  ioConf.pin_bit_mask = gpioMask;
  auto ret = gpio_config(&ioConf);
  if (ret != ESP_OK) {
    SUPLA_LOG_ERROR(
        "InterruptAcToDcIo: Failed to configure GPIO (%d), %d", ret, gpioMask);
    return;
  }

  ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if (ret != ESP_OK) {
    SUPLA_LOG_ERROR("InterruptAcToDcIo: Failed to install ISR service (%d)",
                    ret);
    return;
  }

  for (int i = 0; i < INTERRUPT_AC_TO_DC_IO_MAX_GPIOS; i++) {
    if (gpioState[i] != 255) {
      ret = gpio_isr_handler_add(static_cast<gpio_num_t>(i),
          interruptHandler,
          reinterpret_cast<void*>(i));
      if (ret != ESP_OK) {
        SUPLA_LOG_ERROR(
            "InterruptAcToDcIo: Failed to add ISR handler (%d) for GPIO %d",
            ret,
            i);
      }
    }
  }

  SUPLA_LOG_INFO("InterruptAcToDcIo: initialized");
  initialized = true;
}

bool InterruptAcToDcIo::isInitialized() const {
  return initialized;
}

int InterruptAcToDcIo::customDigitalRead(int channelNumber, uint8_t pin) {
  if (!isInitialized()) {
    SUPLA_LOG_ERROR("InterruptAcToDcIo: not initialized");
    return 0;
  }
  if (gpioState[pin] == 255) {
    return 0;
  }
  return gpioState[pin] == 1 ? 1 : 0;
}

void InterruptAcToDcIo::onFastTimer() {
  uint32_t now = millis();
  for (int i = 0; i < INTERRUPT_AC_TO_DC_IO_MAX_GPIOS; i++) {
    if (gpioState[i] == 255) {
      continue;
    }
    if (gpioInterrupts[i] == 1) {
      gpioInterrupts[i] = 0;
      if (gpioState[i] == 0) {
        gpioState[i] = 2;
      } else if (gpioState[i] == 2) {
        // for "AC" case we update to ON after second interrupt
        SUPLA_LOG_DEBUG(" *** GPIO %d is ON ***", i);
        gpioState[i] = 1;
      }
      gpioLastTimestampMs[i] = now;
      continue;
    }
    if (gpioLastTimestampMs[i] != 0 &&
        now - gpioLastTimestampMs[i] > gpioMinOffTimeout[i]) {
      gpioLastTimestampMs[i] = 0;
      if (gpio_get_level(static_cast<gpio_num_t>(i)) == 0) {
        gpioState[i] = 0;
        SUPLA_LOG_DEBUG(" *** GPIO %d is OFF ***", i);
      } else {
        // for "DC" case we update to ON after filtering timeout
        gpioState[i] = 1;
        SUPLA_LOG_DEBUG(" *** GPIO %d is ON ***", i);
      }
    }
  }
}

void InterruptAcToDcIo::customPinMode(int channelNumber,
                                      uint8_t pin,
                                      uint8_t mode) {
  // do nothing
}
