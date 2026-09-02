// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_BLINKING_LED_H_
#define SRC_SUPLA_CONTROL_BLINKING_LED_H_

#include <supla/element.h>
#include <supla/io.h>

namespace Supla {
class Mutex;

namespace Control {

enum LedState { NOT_INITIALIZED, ON, OFF };
class BlinkingLed : public Supla::Element {
 public:
  explicit BlinkingLed(Supla::Io::IoPin outPin);
  explicit BlinkingLed(Supla::Io::IoPin outPin, bool invert);
  explicit BlinkingLed(Supla::Io::Base *io,
                       uint8_t outPin,
                       bool invert = false);
  explicit BlinkingLed(uint8_t outPin, bool invert = false);
  ~BlinkingLed() override;

  void onInit() override;
  void onTimer() override;

  // Use inverted logic for GPIO output, when:
  // false -> HIGH=ON,  LOW=OFF
  // true  -> HIGH=OFF, LOW=ON
  void setInvertedLogic(bool invertedLogic);

  // Enables custom LED sequence based on given durations.
  // Automatic sequence change will be disabled.
  virtual void setCustomSequence(uint32_t onDurationMs,
                                 uint32_t offDurationMs,
                                 uint32_t pauseDurrationMs = 0,
                                 uint8_t onLimit = 0,
                                 uint8_t repeatLimit = 0,
                                 bool startWithOff = true);

  void setAlwaysOffSequence();
  void setAlwaysOnSequence();

  void setCopyStateTo(BlinkingLed *led);
  void disable();
  void enable();
  void setInvert(bool newInvert);

 protected:
  void updatePin();
  void turnOn();
  void turnOff();

  Supla::Io::IoPin outPin;
  uint8_t onLimitCounter = 0;
  uint8_t onLimit = 0;
  uint8_t repeatLimit = 0;
  uint32_t onDuration = 0;
  uint32_t offDuration = 100;
  uint32_t pauseDuration = 0;
  uint32_t lastUpdate = 0;
  LedState state = NOT_INITIALIZED;
  Supla::Mutex *mutex = nullptr;
  BlinkingLed *copyStateTo = nullptr;
  bool enabled = true;
};

}  // namespace Control

}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_BLINKING_LED_H_
