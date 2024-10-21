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

#include "blinking_led.h"

#include <supla/auto_lock.h>
#include <supla/io.h>
#include <supla/log_wrapper.h>
#include <supla/mutex.h>
#include <supla/time.h>

using Supla::Control::BlinkingLed;

BlinkingLed::BlinkingLed(Supla::Io *io, uint8_t outPin, bool invert)
    : BlinkingLed(outPin, invert) {
  this->io = io;
}

BlinkingLed::BlinkingLed(uint8_t outPin, bool invert)
    : outPin(outPin), invert(invert) {
  mutex = Supla::Mutex::Create();
}

void BlinkingLed::onInit() {
  Supla::AutoLock autoLock(mutex);
  updatePin();
  if (state == NOT_INITIALIZED) {
    turnOn();
  }
  Supla::Io::pinMode(outPin, OUTPUT, io);
}

void BlinkingLed::onTimer() {
  Supla::AutoLock autoLock(mutex);
  updatePin();
}

void BlinkingLed::setInvertedLogic(bool invertedLogic) {
  Supla::AutoLock autoLock(mutex);
  invert = invertedLogic;
  updatePin();
}

void BlinkingLed::turnOn() {
  lastUpdate = millis();
  state = ON;
  Supla::Io::digitalWrite(outPin, invert ? 0 : 1, io);
}

void BlinkingLed::turnOff() {
  lastUpdate = millis();
  state = OFF;
  Supla::Io::digitalWrite(outPin, invert ? 1 : 0, io);
}

void BlinkingLed::updatePin() {
  if (onDuration == 0 || offDuration == 0) {
    if ((state == ON || state == NOT_INITIALIZED) && onDuration == 0) {
      turnOff();
    }
    if ((state == OFF || state == NOT_INITIALIZED) && offDuration == 0) {
      turnOn();
    }
    return;
  }

  if (state == ON && millis() - lastUpdate > onDuration) {
    turnOff();
  } else if (state == OFF) {
    if (onLimit > 0 && pauseDuration > 0 && onLimitCounter == 0 &&
        millis() - lastUpdate < pauseDuration) {
      return;
    }
    if (millis() - lastUpdate > offDuration) {
      if (onLimit > 0 && pauseDuration > 0 && onLimitCounter == 0) {
        onLimitCounter = onLimit;
        if (repeatLimit > 0) {
          if (repeatLimit == 1) {
            onDuration = 0;
            offDuration = 1000;
            onLimit = 0;
            return;
          }
          repeatLimit--;
        }
      }
      turnOn();
      if (onLimitCounter > 0) {
        onLimitCounter--;
      }
    }
  }
}

void BlinkingLed::setCustomSequence(uint32_t onDurationMs,
                                    uint32_t offDurationMs,
                                    uint32_t pauseDurrationMs,
                                    uint8_t onLimit,
                                    uint8_t repeatLimit) {
  Supla::AutoLock autoLock(mutex);
  onDuration = onDurationMs;
  offDuration = offDurationMs;
  pauseDuration = pauseDurrationMs;
  this->onLimit = onLimit;
  onLimitCounter = onLimit;
  this->repeatLimit = repeatLimit;
  state = OFF;
  lastUpdate = 0;
  autoLock.unlock();
  updatePin();
}
