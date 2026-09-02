// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_RGBWDIMMERTESTS_LEGACY_PWM_TEST_IO_H_
#define EXTRAS_TEST_RGBWDIMMERTESTS_LEGACY_PWM_TEST_IO_H_

#include <stdint.h>
#include <supla/io.h>

#include <vector>

class FixedEightBitPwmIo : public Supla::Io::Base {
 public:
  void customAnalogWrite(int, uint8_t, int value) override {
    values.push_back(value);
  }

  void customSetPwmResolutionBits(uint8_t pin,
                                  uint8_t resolutionBits) override {
    requestedResolutionPin = pin;
    requestedResolutionBits = resolutionBits;
    setResolutionCallCount++;
  }

  uint8_t customDefaultPwmResolutionBits(uint8_t) const override {
    return 8;
  }

  bool customCanSetPwmResolutionBits(uint8_t) const override {
    return false;
  }

  uint8_t customPwmResolutionBits(uint8_t) const override {
    return 8;
  }

  uint32_t customPwmMaxValue(uint8_t) const override {
    return 255;
  }

  void clearAnalogWrites() {
    values.clear();
  }

  std::vector<int> values;
  uint8_t requestedResolutionPin = 0;
  uint8_t requestedResolutionBits = 0;
  uint32_t setResolutionCallCount = 0;
};

class MutableTenBitPwmIo : public Supla::Io::Base {
 public:
  void customAnalogWrite(int, uint8_t, int value) override {
    values.push_back(value);
  }

  void customSetPwmResolutionBits(uint8_t pin,
                                  uint8_t resolutionBits) override {
    requestedResolutionPin = pin;
    requestedResolutionBits = resolutionBits;
    setResolutionCallCount++;
  }

  uint8_t customDefaultPwmResolutionBits(uint8_t) const override {
    return 10;
  }

  bool customCanSetPwmResolutionBits(uint8_t) const override {
    return true;
  }

  uint8_t customPwmResolutionBits(uint8_t) const override {
    return 10;
  }

  uint32_t customPwmMaxValue(uint8_t) const override {
    return 1023;
  }

  void clearAnalogWrites() {
    values.clear();
  }

  std::vector<int> values;
  uint8_t requestedResolutionPin = 0;
  uint8_t requestedResolutionBits = 0;
  uint32_t setResolutionCallCount = 0;
};

#endif  // EXTRAS_TEST_RGBWDIMMERTESTS_LEGACY_PWM_TEST_IO_H_
