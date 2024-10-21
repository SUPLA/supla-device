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

#ifndef SRC_SUPLA_CONTROL_BLINKING_LED_H_
#define SRC_SUPLA_CONTROL_BLINKING_LED_H_

#include <supla/element.h>

namespace Supla {
class Io;
class Mutex;

namespace Control {

enum LedState { NOT_INITIALIZED, ON, OFF };
class BlinkingLed : public Supla::Element {
 public:
  explicit BlinkingLed(Supla::Io *io, uint8_t outPin, bool invert = false);
  explicit BlinkingLed(uint8_t outPin, bool invert = false);

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
                                 uint8_t repeatLimit = 0);

 protected:
  void updatePin();
  void turnOn();
  void turnOff();

  uint8_t outPin = 0;
  uint8_t onLimitCounter = 0;
  uint8_t onLimit = 0;
  uint8_t repeatLimit = 0;
  bool invert = false;
  uint32_t onDuration = 0;
  uint32_t offDuration = 1000;
  uint32_t pauseDuration = 0;
  uint32_t lastUpdate = 0;
  LedState state = NOT_INITIALIZED;
  Supla::Io *io = nullptr;
  Supla::Mutex *mutex = nullptr;
};

}  // namespace Control

}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_BLINKING_LED_H_
