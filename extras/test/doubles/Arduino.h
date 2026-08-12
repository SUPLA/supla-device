// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ARDUINO_H_
#define EXTRAS_TEST_DOUBLES_ARDUINO_H_

#include <stdint.h>

#include <stdio.h>
#include <string>

typedef std::string String;

#define LSBFIRST 0
#define INPUT 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define HIGH 1
#define LOW 0

#ifndef PROGMEM
#define PROGMEM
#endif


#ifndef F
#define F(string_literal) string_literal
#endif

void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
void pinMode(uint8_t pin, uint8_t mode);
void analogWriteResolution(uint8_t pin, uint8_t bits);
void analogWriteFrequency(uint8_t pin, uint32_t frequencyHz);
void analogWriteFreq(uint32_t frequencyHz);
void analogWriteRange(uint32_t range);
unsigned int pulseIn(uint8_t pin, uint8_t val, uint64_t timeoutMicro);
void attachInterrupt(uint8_t pin, void (*func)(void), int mode);
void detachInterrupt(uint8_t pin);
uint8_t digitalPinToInterrupt(uint8_t pin);
uint32_t millis();
void delay(uint64_t ms);
long map(long, long, long, long, long);  // NOLINT

class SerialStub {
 public:
  SerialStub() {
  }

  virtual ~SerialStub() {
  }

  int printf(const char *, ...) {
    return 0;
  }
  int print(const String &) {
    return 0;
  }

  int print(const char[]) {
    return 0;
  }

  int print(char) {
    return 0;
  }

  int print(unsigned char) {
    return 0;
  }

  int print(int) {
    return 0;
  }

  int print(unsigned int) {
    return 0;
  }

  int print(long) {  // NOLINT
    return 0;
  }

  int print(unsigned long) {  // NOLINT
    return 0;
  }

  int print(double) {
    return 0;
  }


  int println(const String &) {
    return 0;
  }

  int println(const char[]) {
    return 0;
  }

  int println(char) {
    return 0;
  }

  int println(unsigned char) {
    return 0;
  }

  int println(int) {
    return 0;
  }

  int println(unsigned int) {
    return 0;
  }

  int println(long) {  // NOLINT
    return 0;
  }

  int println(unsigned long) {  // NOLINT
    return 0;
  }

  int println(double) {
    return 0;
  }

  int println(void) {
    return 0;
  }
};

extern SerialStub Serial;

#endif  // EXTRAS_TEST_DOUBLES_ARDUINO_H_
