// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/clock/clock.h>
#include <supla/storage/storage.h>

using ::testing::_;
using ::testing::StrEq;

class ClockConfigTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Storage::SetConfigInstance(&config);
  }

  void TearDown() override {
    Supla::Storage::SetConfigInstance(nullptr);
  }

  ConfigMock config;
  SimpleTime time;
};

TEST_F(ClockConfigTests,
       IgnoresCachedRemoteValueWhenRemoteConfigIsDisabled) {
  Supla::Clock clock;
  clock.setAutomaticTimeSync(true);
  clock.setUseAutomaticTimeSyncRemoteConfig(false);

  EXPECT_CALL(config, init()).Times(0);
  EXPECT_CALL(config,
              getUInt8(StrEq(Supla::AutomaticTimeSyncCfgTag), _))
      .Times(0);

  clock.onDeviceConfigChange(SUPLA_DEVICE_CONFIG_FIELD_AUTOMATIC_TIME_SYNC);

  EXPECT_FALSE(clock.iterateConnected());
}
