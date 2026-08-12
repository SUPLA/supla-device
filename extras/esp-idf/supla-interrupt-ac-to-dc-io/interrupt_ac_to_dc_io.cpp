// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "interrupt_ac_to_dc_io.h"

#include <supla/input_noise_guard.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#ifdef SUPLA_TEST
#include <interrupt_ac_to_dc_io_esp_idf_stubs.h>
#else
#include <driver/gpio.h>
#include <hal/gpio_types.h>
#include <esp_attr.h>
#include <esp_intr_alloc.h>
#include <freertos/FreeRTOS.h>
#endif

using Supla::InterruptAcToDcIo;

namespace {
static volatile uint8_t
gpioInterruptCounts[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
static portMUX_TYPE gpioInterruptCountsMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR interruptHandler(void* arg) {
  uintptr_t gpioNum = reinterpret_cast<uintptr_t>(arg);
  if (gpioNum < INTERRUPT_AC_TO_DC_IO_MAX_GPIOS) {
    portENTER_CRITICAL_ISR(&gpioInterruptCountsMux);
    uint8_t interruptCount = gpioInterruptCounts[gpioNum];
    if (interruptCount < 255) {
      gpioInterruptCounts[gpioNum] = interruptCount + 1;
    }
    portEXIT_CRITICAL_ISR(&gpioInterruptCountsMux);
  }
}

}  // namespace

InterruptAcToDcIo::InterruptAcToDcIo() : Supla::Io::Base() {
  for (int i = 0; i < INTERRUPT_AC_TO_DC_IO_MAX_GPIOS; i++) {
    gpioState[i] = 255;
  }
}

InterruptAcToDcIo::~InterruptAcToDcIo() {
}

void InterruptAcToDcIo::addGpio(int gpio,
                                int32_t minOffTimeoutMs,
                                uint8_t minQuietBeforeNextActivityMs) {
  if (isInitialized()) {
    SUPLA_LOG_ERROR("InterruptAcToDcIo already initialized - can't add GPIO");
    return;
  }

  if (gpio < 0 || gpio >= INTERRUPT_AC_TO_DC_IO_MAX_GPIOS) {
    SUPLA_LOG_ERROR("InterruptAcToDcIo: Invalid GPIO number %d", gpio);
    return;
  }

  gpioMinOffTimeout[gpio] = minOffTimeoutMs;
  gpioMinQuietBeforeNextActivityMs[gpio] = minQuietBeforeNextActivityMs;
  if (minOffTimeoutMs > 0 &&
      static_cast<uint32_t>(minOffTimeoutMs) > initCounter) {
    initCounter = minOffTimeoutMs;
  }
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
//  ioConf.pull_down_en = GPIO_PULLDOWN_ENABLE;
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
      // configure initial state
      if (gpio_get_level(static_cast<gpio_num_t>(i)) == offStateLevel) {
        gpioState[i] = 0;
        SUPLA_LOG_DEBUG(" *** GPIO %d is OFF (initial) ***", i);
      } else {
        gpioState[i] = 1;
        SUPLA_LOG_DEBUG(" *** GPIO %d is ON (initial) ***", i);
      }
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

bool InterruptAcToDcIo::isReady() const {
  return initialized && initCounter == 0;
}

void InterruptAcToDcIo::enableInputNoiseGuardForGpio(int gpio, bool enabled) {
  if (gpio < 0 || gpio >= INTERRUPT_AC_TO_DC_IO_MAX_GPIOS) {
    SUPLA_LOG_ERROR("InterruptAcToDcIo: Invalid GPIO number %d", gpio);
    return;
  }
  gpioInputNoiseGuardEnabled[gpio] = enabled ? 1 : 0;
}

void InterruptAcToDcIo::disableInputNoiseGuardForGpio(int gpio) {
  enableInputNoiseGuardForGpio(gpio, false);
}

int InterruptAcToDcIo::customDigitalRead(int channelNumber, uint8_t pin) {
  (void)channelNumber;
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

  if (initCounter > 0) {
    initCounter--;
  }

  for (int i = 0; i < INTERRUPT_AC_TO_DC_IO_MAX_GPIOS; i++) {
    if (gpioState[i] == 255) {
      continue;
    }
    portENTER_CRITICAL(&gpioInterruptCountsMux);
    uint8_t interruptCount = gpioInterruptCounts[i];
    if (interruptCount > 0) {
      gpioInterruptCounts[i] = 0;
    }
    portEXIT_CRITICAL(&gpioInterruptCountsMux);

    if (gpioInputNoiseGuardEnabled[i] &&
        Supla::InputNoiseGuard::IsActive()) {
      gpioInputNoiseGuardWasActive[i] = 1;
      gpioRawActivitySeen[i] = 0;
      gpioLastRawTimestampMs[i] = 0;
      gpioAcCandidateEdges[i] = 0;
      gpioAcCandidateActiveSamples[i] = 0;
      gpioAcCandidateFirstTimestampMs[i] = 0;
      if (gpioState[i] == 1 || gpioState[i] == 2) {
        gpioLastTimestampMs[i] = now;
      }
      continue;
    }

    if (gpioInputNoiseGuardEnabled[i] &&
        gpioInputNoiseGuardWasActive[i]) {
      gpioInputNoiseGuardWasActive[i] = 0;
      gpioRawActivitySeen[i] = 0;
      gpioLastRawTimestampMs[i] = 0;
      gpioAcCandidateEdges[i] = 0;
      gpioAcCandidateActiveSamples[i] = 0;
      gpioAcCandidateFirstTimestampMs[i] = 0;

      if (gpio_get_level(static_cast<gpio_num_t>(i)) == offStateLevel) {
        gpioLastTimestampMs[i] = gpioState[i] == 0 ? 0 : now;
      } else if (gpioState[i] == 1) {
        gpioLastTimestampMs[i] = 0;
      } else {
        gpioState[i] = 2;
        gpioLastTimestampMs[i] = now;
      }
      continue;
    }

    if (interruptCount > 0) {
//      SUPLA_LOG_DEBUG("GPIO %d INTR COUNT %d", i, interruptCount);
      gpioRawActivitySeen[i] = 1;
      gpioLastRawTimestampMs[i] = now;

      if (gpioState[i] == 1 || gpioState[i] == 2) {
        gpioLastTimestampMs[i] = now;
      }

      if (gpioState[i] == 0) {
        gpioState[i] = 2;
        gpioAcCandidateFirstTimestampMs[i] = now;
        gpioAcCandidateEdges[i] = interruptCount;
        gpioAcCandidateActiveSamples[i] = 1;
        gpioLastTimestampMs[i] = now;
      } else if (gpioState[i] == 2) {
        uint32_t candidateSpanMs = now - gpioAcCandidateFirstTimestampMs[i];
        if (gpioAcCandidateActiveSamples[i] == 0 ||
            candidateSpanMs > INTERRUPT_AC_TO_DC_IO_AC_ON_WINDOW_MS) {
          gpioAcCandidateFirstTimestampMs[i] = now;
          gpioAcCandidateEdges[i] = interruptCount;
          gpioAcCandidateActiveSamples[i] = 1;
          candidateSpanMs = 0;
        } else {
          uint32_t candidateEdges = gpioAcCandidateEdges[i] + interruptCount;
          gpioAcCandidateEdges[i] =
              candidateEdges > 65535 ? 65535 : candidateEdges;
          if (gpioAcCandidateActiveSamples[i] < 255) {
            gpioAcCandidateActiveSamples[i]++;
          }
        }

        if (gpioAcCandidateActiveSamples[i] >=
                INTERRUPT_AC_TO_DC_IO_AC_ON_MIN_ACTIVE_SAMPLES &&
            gpioAcCandidateEdges[i] >=
                INTERRUPT_AC_TO_DC_IO_AC_ON_MIN_EDGES &&
            candidateSpanMs >= INTERRUPT_AC_TO_DC_IO_AC_ON_MIN_SPAN_MS) {
          SUPLA_LOG_DEBUG(" *** GPIO %d is ON (AC) ***", i);
          gpioState[i] = 1;
          gpioAcCandidateEdges[i] = 0;
          gpioAcCandidateActiveSamples[i] = 0;
          gpioAcCandidateFirstTimestampMs[i] = 0;
        }
      }
      continue;
    }
    if (gpioLastTimestampMs[i] != 0 &&
        now - static_cast<uint32_t>(gpioLastTimestampMs[i]) >
            static_cast<uint32_t>(gpioMinOffTimeout[i])) {
      gpioLastTimestampMs[i] = 0;
      gpioAcCandidateEdges[i] = 0;
      gpioAcCandidateActiveSamples[i] = 0;
      gpioAcCandidateFirstTimestampMs[i] = 0;
      if (gpio_get_level(static_cast<gpio_num_t>(i)) == offStateLevel) {
        gpioState[i] = 0;
        SUPLA_LOG_DEBUG(" *** GPIO %d is OFF ***", i);
      } else {
        // for "DC" case we update to ON after filtering timeout
        gpioState[i] = 1;
        SUPLA_LOG_DEBUG(" *** GPIO %d is ON (DC)***", i);
      }
    }
  }
}

void InterruptAcToDcIo::setOffStateLevel(uint8_t level) {
  offStateLevel = level;
}

void InterruptAcToDcIo::customPinMode(int channelNumber,
                                      uint8_t pin,
                                      uint8_t mode) {
  (void)channelNumber;
  (void)pin;
  (void)mode;
  // do nothing
}
