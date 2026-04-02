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


#ifndef F
#define F(string_literal) string_literal
#endif

void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
void pinMode(uint8_t pin, uint8_t mode);
void analogWriteResolution(uint8_t pin, uint8_t bits);
void analogWriteFrequency(uint8_t pin, uint32_t frequencyHz);
void analogWriteFreq(uint32_t frequencyHz);
void analogWriteRange(uint32_t range);
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
