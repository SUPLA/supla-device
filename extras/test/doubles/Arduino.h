#ifndef _test_double_arduino_h
#define _test_double_arduino_h

#include <stdint.h>

#include <string>

typedef std::string String;

#define LSBFIRST 0
#define INPUT 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define HIGH 1
#define LOW 0


#define F(string_literal) string_literal

void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
void pinMode(uint8_t pin, uint8_t mode);
uint32_t millis();
void delay(uint64_t ms);
long map(long, long, long, long, long);

class SerialStub {
 public:
  SerialStub() {
  }

  virtual ~SerialStub() {
  }

  int printf(const char *format, ...) {
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

  int print(long) {
    return 0;
  }

  int print(unsigned long) {
    return 0;
  }

  int print(double) {
    return 0;
  }


  int println(const String &s) {
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

  int println(long) {
    return 0;
  }

  int println(unsigned long) {
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

#endif
