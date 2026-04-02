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

#include "impulse_counter.h"

#include <supla/actions.h>
#include <supla/log_wrapper.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

using Supla::Sensor::ImpulseCounter;

ImpulseCounter::ImpulseCounter(Supla::Io::IoPin impulsePin,
                               bool _detectLowToHigh,
                               bool _inputPullup,
                               uint16_t _debounceDelay,
                               uint16_t minSignalTimeToCountMs)
    : impulsePin(impulsePin),
      debounceDelayMs(_debounceDelay),
      minSignalTimeToCountMs(minSignalTimeToCountMs),
      detectLowToHigh(_detectLowToHigh) {
  this->impulsePin.setPullUp(_inputPullup);
  this->impulsePin.setMode(INPUT);

  prevState = (detectLowToHigh == true ? LOW : HIGH);
  newStateCandidate = prevState;

  SUPLA_LOG_DEBUG("IC[%d]: impulsePin(%d), delay(%d ms)",
                  getChannelNumber(),
                  this->impulsePin.getPin(),
                  debounceDelayMs);
  if (this->impulsePin.getPin() < 0) {
    SUPLA_LOG_ERROR("IC[%d]: incorrect impulse pin number", getChannelNumber());
  }
}

ImpulseCounter::ImpulseCounter(Supla::Io::Base *io,
                               int _impulsePin,
                               bool _detectLowToHigh,
                               bool _inputPullup,
                               uint16_t _debounceDelay,
                               uint16_t minSignalTimeToCountMs)
    : ImpulseCounter(Supla::Io::IoPin(_impulsePin, io),
                     _detectLowToHigh,
                     _inputPullup,
                     _debounceDelay,
                     minSignalTimeToCountMs) {
}

ImpulseCounter::ImpulseCounter(int _impulsePin,
                               bool _detectLowToHigh,
                               bool _inputPullup,
                               uint16_t _debounceDelay,
                               uint16_t minSignalTimeToCountMs)
    : ImpulseCounter(Supla::Io::IoPin(_impulsePin),
                     _detectLowToHigh,
                     _inputPullup,
                     _debounceDelay,
                     minSignalTimeToCountMs) {
}

void ImpulseCounter::onInit() {
  impulsePin.pinMode(channel.getChannelNumber());
  prevState = impulsePin.digitalRead(channel.getChannelNumber());
  newStateCandidate = prevState;
}

void ImpulseCounter::onFastTimer() {
  int currentState = impulsePin.digitalRead(channel.getChannelNumber());
  if (currentState != newStateCandidate) {
    newStateCandidate = currentState;
    lastChangeMs = millis();
  }
  if (millis() - lastChangeMs < minSignalTimeToCountMs) {
    return;
  }
  if (prevState == (detectLowToHigh == true ? LOW : HIGH)) {
    if (millis() - lastImpulseMillis > debounceDelayMs) {
      if (currentState == (detectLowToHigh == true ? HIGH : LOW)) {
        incCounter();
        lastImpulseMillis = millis();
      }
    }
  }
  prevState = currentState;
}
