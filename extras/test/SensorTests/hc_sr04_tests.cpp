// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <arduino_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/sensor/HC_SR04.h>
#include <supla_io_mock.h>

using ::testing::InSequence;
using ::testing::Return;

namespace {

constexpr uint8_t kTriggerPin = 12;
constexpr uint8_t kEchoPin = 13;

TEST(HcSr04Tests, UsesCustomIoBackendForAllOperations) {
  DigitalInterfaceMock nativeIo;
  SuplaIoMock customIo;

  EXPECT_CALL(nativeIo, pinMode).Times(0);
  EXPECT_CALL(nativeIo, digitalWrite).Times(0);
  EXPECT_CALL(nativeIo, pulseIn).Times(0);

  InSequence sequence;
  EXPECT_CALL(customIo, customPinMode(-1, kTriggerPin, OUTPUT));
  EXPECT_CALL(customIo, customPinMode(-1, kEchoPin, INPUT));
  EXPECT_CALL(customIo, customDigitalWrite(-1, kTriggerPin, LOW));
  EXPECT_CALL(customIo, customDigitalWrite(-1, kTriggerPin, HIGH));
  EXPECT_CALL(customIo, customDigitalWrite(-1, kTriggerPin, LOW));
  EXPECT_CALL(customIo, customPulseIn(-1, kEchoPin, HIGH, 60000))
      .WillOnce(Return(100));
  EXPECT_CALL(customIo, customDigitalWrite(-1, kTriggerPin, HIGH));
  EXPECT_CALL(customIo, customDigitalWrite(-1, kTriggerPin, LOW));
  EXPECT_CALL(customIo, customPulseIn(-1, kEchoPin, HIGH, 60000))
      .WillOnce(Return(100));

  Supla::Sensor::HC_SR04 sensor(
      kTriggerPin, kEchoPin, 0, 500, 0, 500, &customIo);
  sensor.onInit();
}

TEST(HcSr04Tests, UsesNativeGpioWithoutCustomBackend) {
  DigitalInterfaceMock nativeIo;

  InSequence sequence;
  EXPECT_CALL(nativeIo, pinMode(kTriggerPin, OUTPUT));
  EXPECT_CALL(nativeIo, pinMode(kEchoPin, INPUT));
  EXPECT_CALL(nativeIo, digitalWrite(kTriggerPin, LOW));
  EXPECT_CALL(nativeIo, digitalWrite(kTriggerPin, HIGH));
  EXPECT_CALL(nativeIo, digitalWrite(kTriggerPin, LOW));
  EXPECT_CALL(nativeIo, pulseIn(kEchoPin, HIGH, 60000))
      .WillOnce(Return(100));
  EXPECT_CALL(nativeIo, digitalWrite(kTriggerPin, HIGH));
  EXPECT_CALL(nativeIo, digitalWrite(kTriggerPin, LOW));
  EXPECT_CALL(nativeIo, pulseIn(kEchoPin, HIGH, 60000))
      .WillOnce(Return(100));

  Supla::Sensor::HC_SR04 sensor(kTriggerPin, kEchoPin);
  sensor.onInit();
}

}  // namespace
