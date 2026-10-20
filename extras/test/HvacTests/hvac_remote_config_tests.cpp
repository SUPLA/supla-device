// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <string.h>
#include <supla/control/hvac_base.h>
#include <supla/control/weekly_schedule_component.h>

namespace {
class RemoteConfigHvac : public Supla::Control::HvacBase {
 public:
  explicit RemoteConfigHvac(int number = -1)
      : HvacBase(nullptr, nullptr, number) {
    setHeatingAndCoolingSupported(true);
    setDefaultSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
    onInit();
  }
  using HvacBase::importRemoteConfig;
  Supla::ApplyConfigResult preparation = Supla::ApplyConfigResult::Success;
  int preparations = 0;
  bool invalidPreparedConfig = false;

 protected:
  Supla::ApplyConfigResult prepareChannelConfig(TChannelConfig_HVAC *config,
                                                bool, bool &) override {
    ++preparations;
    // Revalidation must not depend on the hook reporting its modification.
    if (invalidPreparedConfig) config->TemperatureControlType = 255;
    return preparation;
  }
};

class HvacRemoteConfigTests : public testing::Test {
 protected:
  SimpleTime time;
  void SetUp() override { Supla::Channel::resetToDefaults(); }
  void TearDown() override { Supla::Channel::resetToDefaults(); }
};
}  // namespace

TEST_F(HvacRemoteConfigTests,
       ExplicitChannelNumberIsReservedByBaseConstructor) {
  RemoteConfigHvac hvac(37);
  EXPECT_EQ(hvac.getChannelNumber(), 37);
}

TEST_F(HvacRemoteConfigTests, RejectedPreparationDoesNotApplyServerConfig) {
  testing::NiceMock<ConfigMock> storage;
  RemoteConfigHvac hvac;
  TChannelConfig_HVAC current = {};
  hvac.copyFullChannelConfigTo(&current);
  TSD_ChannelConfig request = {};
  request.Func = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
  request.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  request.ConfigSize = sizeof(current);
  auto proposed = current;
  proposed.TemperatureSetpointChangeSwitchesToManualMode =
      !current.TemperatureSetpointChangeSwitchesToManualMode;
  memcpy(request.Config, &proposed, sizeof(proposed));
  hvac.preparation = Supla::ApplyConfigResult::DataError;
  EXPECT_EQ(hvac.handleChannelConfig(&request), SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_EQ(hvac.preparations, 1);
  TChannelConfig_HVAC after = {};
  hvac.copyFullChannelConfigTo(&after);
  EXPECT_EQ(memcmp(&current, &after, sizeof(current)), 0);

  hvac.preparation = Supla::ApplyConfigResult::Success;
  hvac.invalidPreparedConfig = true;
  EXPECT_EQ(hvac.handleChannelConfig(&request), SUPLA_CONFIG_RESULT_DATA_ERROR);
  hvac.copyFullChannelConfigTo(&after);
  EXPECT_EQ(memcmp(&current, &after, sizeof(current)), 0);
  hvac.invalidPreparedConfig = false;
  memcpy(request.Config, &proposed, sizeof(proposed));
  EXPECT_EQ(hvac.handleChannelConfig(&request), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(hvac.preparations, 3);
  EXPECT_EQ(hvac.isTemperatureSetpointChangeSwitchesToManualMode(),
            proposed.TemperatureSetpointChangeSwitchesToManualMode);
}

TEST_F(HvacRemoteConfigTests, ObservationsUseNativeValidationAndStorage) {
  testing::NiceMock<ConfigMock> storage;
  RemoteConfigHvac hvac;
  TChannelConfig_HVAC observed = {};
  hvac.copyFullChannelConfigTo(&observed);
  observed.TemperatureSetpointChangeSwitchesToManualMode =
      !observed.TemperatureSetpointChangeSwitchesToManualMode;
  EXPECT_CALL(storage, setBlob(testing::StrEq("0_hvac_cfg2"), testing::_,
                               sizeof(Supla::Control::HvacStoredConfigV2)))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(hvac.importRemoteConfig(observed, true));
  EXPECT_EQ(hvac.preparations,
            0);  // Observations must not issue device writes.
  EXPECT_TRUE(hvac.importRemoteConfig(observed, true));  // No duplicate save.
  observed.ParameterFlags.UsedAlgorithmReadonly =
      !observed.ParameterFlags.UsedAlgorithmReadonly;
  observed.Reserved[0] = 123;
  EXPECT_TRUE(hvac.importRemoteConfig(observed, true));  // Ignored fields.
  observed.TemperatureControlType = 255;
  EXPECT_FALSE(hvac.importRemoteConfig(observed, true));
}
