// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

int analogRead(uint8_t pin) {
  (void)(pin);
  return 0;
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

unsigned int pulseIn(uint8_t pin, uint8_t val, uint64_t timeoutMicro) {
  assert(DigitalInterface::instance);
  return DigitalInterface::instance->pulseIn(pin, val, timeoutMicro);
}

void attachInterrupt(uint8_t pin, void (*func)(void), int mode) {
  (void)(pin);
  (void)(func);
  (void)(mode);
}

void detachInterrupt(uint8_t pin) {
  (void)(pin);
}

uint8_t digitalPinToInterrupt(uint8_t pin) {
  return pin;
}

uint32_t millis() {
  assert(TimeInterface::instance);
  return TimeInterface::instance->millis();
}

void delay(uint64_t) {
}

void delayMicroseconds(uint64_t) {
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
