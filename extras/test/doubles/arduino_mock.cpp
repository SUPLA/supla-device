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

#include "arduino_mock.h"

#include <gmock/gmock.h>

#include "Arduino.h"

SerialStub Serial;

DigitalInterface::DigitalInterface() {
  instance = this;
}

DigitalInterface::~DigitalInterface() {
  instance = nullptr;
}

DigitalInterface *DigitalInterface::instance = nullptr;

TimeInterface::TimeInterface() {
  instance = this;
}

TimeInterface::~TimeInterface() {
  instance = nullptr;
}

TimeInterface *TimeInterface::instance = nullptr;

void analogWrite(uint8_t pin, int val) {
  assert(DigitalInterface::instance);
  DigitalInterface::instance->analogWrite(pin, val);
}

void digitalWrite(uint8_t pin, uint8_t val) {
  assert(DigitalInterface::instance);
  DigitalInterface::instance->digitalWrite(pin, val);
}

int digitalRead(uint8_t pin) {
  assert(DigitalInterface::instance);
  return DigitalInterface::instance->digitalRead(pin);
}

void pinMode(uint8_t pin, uint8_t mode) {
  assert(DigitalInterface::instance);
  DigitalInterface::instance->pinMode(pin, mode);
}

void analogWriteResolution(uint8_t pin, uint8_t bits) {
  assert(DigitalInterface::instance);
  DigitalInterface::instance->analogWriteResolution(pin, bits);
}

void analogWriteFrequency(uint8_t pin, uint32_t frequencyHz) {
  assert(DigitalInterface::instance);
  DigitalInterface::instance->analogWriteFrequency(pin, frequencyHz);
}

void analogWriteFreq(uint32_t frequencyHz) {
  assert(DigitalInterface::instance);
  DigitalInterface::instance->analogWriteFreq(frequencyHz);
}

void analogWriteRange(uint32_t range) {
  assert(DigitalInterface::instance);
  DigitalInterface::instance->analogWriteRange(range);
}

uint32_t millis() {
  assert(TimeInterface::instance);
  return TimeInterface::instance->millis();
}

void delay(uint64_t) {
}

long map(  // NOLINT
    long input,  // NOLINT
    long inMin,  // NOLINT
    long inMax,  // NOLINT
    long outMin,  // NOLINT
    long outMax) {  // NOLINT
  long result =  // NOLINT
      (input - inMin) * (outMax - outMin) / (inMax - inMin);
  return result + outMin;
}

DigitalInterfaceMock::DigitalInterfaceMock() {
}
DigitalInterfaceMock::~DigitalInterfaceMock() {
}
