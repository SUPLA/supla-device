// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <interrupt_ac_to_dc_io.h>
#include <interrupt_ac_to_dc_io_esp_idf_stubs.h>
#include <simple_time.h>
#include <supla/input_noise_guard.h>

namespace {
constexpr int kGpio = 4;
constexpr int kMinOffTimeoutMs = 20;
}  // namespace

class InterruptAcToDcIoFixture : public testing::Test {
 protected:
  void SetUp() override {
    time.value = 1;
    InterruptAcToDcIoTestSupport::reset();
    Supla::InputNoiseGuard::Clear();
  }

  void TearDown() override {
    Supla::InputNoiseGuard::Clear();
  }

  void initialize(Supla::InterruptAcToDcIo *io, int initialLevel) {
    InterruptAcToDcIoTestSupport::setGpioLevel(kGpio, initialLevel);
    io->addGpio(kGpio, kMinOffTimeoutMs);
    io->enableInputNoiseGuardForGpio(kGpio);
    io->initialize();
  }

  void triggerAcSample(Supla::InterruptAcToDcIo *io) {
    InterruptAcToDcIoTestSupport::triggerInterrupt(kGpio, 3);
    io->onFastTimer();
  }

  SimpleTime time;
};

TEST_F(InterruptAcToDcIoFixture, DcOffToOnDuringGuardIsRecovered) {
  Supla::InterruptAcToDcIo io;
  initialize(&io, 0);
  Supla::InputNoiseGuard::IgnoreForMs(100);

  InterruptAcToDcIoTestSupport::setGpioLevel(kGpio, 1);
  InterruptAcToDcIoTestSupport::triggerInterrupt(kGpio);
  io.onFastTimer();

  time.advance(100);
  io.onFastTimer();
  EXPECT_EQ(0, io.customDigitalRead(0, kGpio));

  time.advance(kMinOffTimeoutMs + 1);
  io.onFastTimer();
  EXPECT_EQ(1, io.customDigitalRead(0, kGpio));
}

TEST_F(InterruptAcToDcIoFixture, DcOnToOffDuringGuardIsRecovered) {
  Supla::InterruptAcToDcIo io;
  initialize(&io, 1);
  Supla::InputNoiseGuard::IgnoreForMs(100);

  InterruptAcToDcIoTestSupport::setGpioLevel(kGpio, 0);
  InterruptAcToDcIoTestSupport::triggerInterrupt(kGpio);
  io.onFastTimer();

  time.advance(100);
  io.onFastTimer();
  EXPECT_EQ(1, io.customDigitalRead(0, kGpio));

  time.advance(kMinOffTimeoutMs + 1);
  io.onFastTimer();
  EXPECT_EQ(0, io.customDigitalRead(0, kGpio));
}

TEST_F(InterruptAcToDcIoFixture, GuardNoiseEndingOffDoesNotTurnInputOn) {
  Supla::InterruptAcToDcIo io;
  initialize(&io, 0);
  Supla::InputNoiseGuard::IgnoreForMs(100);

  InterruptAcToDcIoTestSupport::setGpioLevel(kGpio, 1);
  InterruptAcToDcIoTestSupport::triggerInterrupt(kGpio, 20);
  InterruptAcToDcIoTestSupport::setGpioLevel(kGpio, 0);
  io.onFastTimer();

  time.advance(100);
  io.onFastTimer();
  time.advance(kMinOffTimeoutMs + 1);
  io.onFastTimer();

  EXPECT_EQ(0, io.customDigitalRead(0, kGpio));
}

TEST_F(InterruptAcToDcIoFixture, AcDetectionStillWorksAfterGuard) {
  Supla::InterruptAcToDcIo io;
  initialize(&io, 0);
  Supla::InputNoiseGuard::IgnoreForMs(100);

  InterruptAcToDcIoTestSupport::triggerInterrupt(kGpio, 20);
  io.onFastTimer();
  time.advance(100);
  io.onFastTimer();

  InterruptAcToDcIoTestSupport::setGpioLevel(kGpio, 1);
  triggerAcSample(&io);
  time.advance(10);
  triggerAcSample(&io);
  time.advance(10);
  triggerAcSample(&io);
  time.advance(10);
  triggerAcSample(&io);

  EXPECT_EQ(1, io.customDigitalRead(0, kGpio));

  InterruptAcToDcIoTestSupport::setGpioLevel(kGpio, 0);
  time.advance(kMinOffTimeoutMs + 1);
  io.onFastTimer();
  EXPECT_EQ(0, io.customDigitalRead(0, kGpio));
}
