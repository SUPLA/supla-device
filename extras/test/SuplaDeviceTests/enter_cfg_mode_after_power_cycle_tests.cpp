// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/clock/clock.h>
#include <supla/device/enter_cfg_mode_after_power_cycle.h>
#include <supla/storage/storage.h>

namespace {
using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

class EnterCfgModeAfterPowerCycleTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Storage::SetConfigInstance(&config);
  }

  void TearDown() override {
    Supla::Storage::SetConfigInstance(nullptr);
  }

  Supla::Clock clock;
  SimpleTime time;
  NiceMock<ConfigMock> config;
};
}  // namespace

TEST_F(EnterCfgModeAfterPowerCycleTests,
       ResetCounterClearsStoredCounterAndStopsCurrentSessionIncrement) {
  Supla::Device::EnterCfgModeAfterPowerCycle powerCycle(5000, 3, true);

  EXPECT_CALL(config, getUInt32(StrEq("power_cycle"), _))
      .WillOnce(DoAll(SetArgPointee<1>(2), Return(true)));
  powerCycle.onLoadConfig(nullptr);

  EXPECT_CALL(config, setUInt32(StrEq("power_cycle"), 0))
      .WillOnce(Return(true));
  EXPECT_CALL(config, commit()).Times(1);
  EXPECT_CALL(config, saveWithDelay(_)).Times(0);
  powerCycle.resetCounter();
  powerCycle.iterateAlways();
}
