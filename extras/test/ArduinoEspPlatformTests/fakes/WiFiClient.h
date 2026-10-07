// Copyright (C) AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_WIFICLIENT_H_
#define EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_WIFICLIENT_H_

#include <Arduino.h>

class WiFiClient {
 public:
  virtual ~WiFiClient() = default;
  virtual int connect(const char *, uint16_t) { return 1; }
  virtual void stop() {}
  virtual int available() { return 0; }
  virtual uint8_t connected() { return 1; }
  virtual int read(uint8_t *, size_t) { return 0; }
  virtual size_t write(const uint8_t *, size_t size) { return size; }
  virtual void setTimeout(uint16_t) {}
  virtual uint32_t localIP() { return 0; }
};

#endif  // EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_WIFICLIENT_H_
