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
