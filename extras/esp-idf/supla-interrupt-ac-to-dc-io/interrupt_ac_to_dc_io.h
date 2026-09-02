// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_INTERRUPT_AC_TO_DC_IO_INTERRUPT_AC_TO_DC_IO_H_
#define EXTRAS_ESP_IDF_SUPLA_INTERRUPT_AC_TO_DC_IO_INTERRUPT_AC_TO_DC_IO_H_

#include <supla/io.h>
#include <supla/element.h>

namespace Supla {

#define INTERRUPT_AC_TO_DC_IO_MAX_GPIOS 50
#define INTERRUPT_AC_TO_DC_IO_DEFAULT_MIN_QUIET_MS 5
#define INTERRUPT_AC_TO_DC_IO_AC_ON_MIN_ACTIVE_SAMPLES 4
#define INTERRUPT_AC_TO_DC_IO_AC_ON_MIN_EDGES 12
#define INTERRUPT_AC_TO_DC_IO_AC_ON_MIN_SPAN_MS 25
#define INTERRUPT_AC_TO_DC_IO_AC_ON_WINDOW_MS 80

class InterruptAcToDcIo : public Io::Base, public Element {
 public:
  InterruptAcToDcIo();
  ~InterruptAcToDcIo();

  void initialize();
  bool isInitialized() const;

  bool isReady() const override;

  void addGpio(
      int gpio,
      int32_t minOffTimeoutMs,
      uint8_t minQuietBeforeNextActivityMs =
          INTERRUPT_AC_TO_DC_IO_DEFAULT_MIN_QUIET_MS);

  int customDigitalRead(int channelNumber, uint8_t pin) override;
  void customPinMode(int channelNumber, uint8_t pin, uint8_t mode) override;

  void onFastTimer() override;
  void setOffStateLevel(uint8_t level);
  void enableInputNoiseGuardForGpio(int gpio, bool enabled = true);
  void disableInputNoiseGuardForGpio(int gpio);

 protected:
//  bool gpioHiIsOn[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  int32_t gpioMinOffTimeout[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  int32_t gpioLastTimestampMs[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  uint32_t gpioLastRawTimestampMs[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  uint32_t gpioAcCandidateFirstTimestampMs[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] =
      {};
  uint16_t gpioAcCandidateEdges[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  uint8_t gpioAcCandidateActiveSamples[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  uint8_t gpioState[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  uint8_t gpioMinQuietBeforeNextActivityMs[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] =
      {};
  uint8_t gpioRawActivitySeen[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  uint8_t gpioInputNoiseGuardEnabled[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  uint8_t gpioInputNoiseGuardWasActive[INTERRUPT_AC_TO_DC_IO_MAX_GPIOS] = {};
  bool initialized = false;
  uint8_t offStateLevel = 0;
  uint32_t initCounter = 100;
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_INTERRUPT_AC_TO_DC_IO_INTERRUPT_AC_TO_DC_IO_H_
