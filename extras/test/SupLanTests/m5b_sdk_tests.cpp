// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <math.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <output_mock.h>
#include <supla/actions.h>
#include <supla/channels/channel_state.h>
#include <supla/control/hvac_base.h>
#include <supla/control/lighting_pwm_base.h>
#include <supla/control/roller_shutter.h>
#include <supla/control/virtual_relay.h>
#include <supla/control/virtual_valve.h>
#include <supla/events.h>
#include <supla/local_action.h>

namespace {
class RemoteRelay : public Supla::Control::VirtualRelay {
 public:
  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *command) override {
    ++legacyCalls;
    lastDuration = command->DurationMS;
    return VirtualRelay::handleNewValueFromServer(command);
  }
  int legacyCalls = 0;
  uint32_t lastDuration = 0;
};
TEST(M5Sdk, LegacyVirtualForwardingAndTimedDurationCodec) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  RemoteRelay relay;
  relay.setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  relay.onInit();
  EXPECT_EQ(relay.handleRemoteAction(ACTION_TURN_ON, nullptr, 0), 3);
  EXPECT_TRUE(relay.isOn());
  EXPECT_EQ(relay.legacyCalls, 1);
  const uint8_t duration[] = {0xe8, 3, 0, 0};
  EXPECT_EQ(relay.handleRemoteAction(ACTION_TURN_ON_WITH_DURATION,
                                    duration, sizeof(duration)), 3);
  EXPECT_EQ(relay.lastDuration, 1000U);
  EXPECT_EQ(relay.handleRemoteAction(ACTION_TOGGLE, nullptr, 0), 3);
  EXPECT_FALSE(relay.isOn());
  EXPECT_EQ(relay.handleRemoteAction(ACTION_TOGGLE, duration, 4), 35);
  EXPECT_FALSE(relay.isOn());
  EXPECT_EQ(relay.handleRemoteAction(ACTION_OPEN, nullptr, 0), 33);
}
TEST(M5Sdk, RelayModeParamsReservedBytesAndFunctionAwareActions) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  RemoteRelay relay;
  relay.setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  relay.onInit();
  uint8_t params[16] = {SUPLA_RELAY_MODE_CMD_SWITCH_TO_MANUAL};
  EXPECT_EQ(relay.handleRemoteAction(ACTION_SET_RELAY_PARAMETERS,
            params, 16), 3);
  params[15] = 1;
  EXPECT_EQ(relay.handleRemoteAction(ACTION_SET_RELAY_PARAMETERS,
            params, 16), 35);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE);
  EXPECT_EQ(relay.handleRemoteAction(ACTION_OPEN, nullptr, 0), 33);
  EXPECT_EQ(relay.handleRemoteAction(ACTION_CLOSE, nullptr, 0), 33);
  EXPECT_EQ(relay.handleRemoteAction(ACTION_OPEN_CLOSE, nullptr, 0), 3);
  relay.setDefaultFunction(SUPLA_CHANNELFNC_PUMPSWITCH);
  EXPECT_EQ(relay.handleRemoteAction(ACTION_TURN_ON, nullptr, 0), 33);
}
TEST(M5Sdk, ValveActionsAreSeparateFromDirectionalGateAutomation) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Control::VirtualValve valve;
  EXPECT_EQ(valve.handleRemoteAction(ACTION_OPEN, nullptr, 0), 3);
  EXPECT_EQ(valve.getValueOpenStateFromDevice(), 100);
  EXPECT_EQ(valve.handleRemoteAction(ACTION_CLOSE, nullptr, 0), 3);
  EXPECT_EQ(valve.getValueOpenStateFromDevice(), 0);
  EXPECT_EQ(valve.handleRemoteAction(ACTION_SHUT, nullptr, 0), 33);
  uint8_t params = 0;
  EXPECT_EQ(valve.handleRemoteAction(ACTION_OPEN, &params, 1), 35);
}
TEST(M5Sdk, ShadingUsesSourceSignedTiltDeltaAndRejectsUnknownFlags) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Control::RollerShutter shutter(1, 2, true, true);
  shutter.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  shutter.setOpenCloseTime(10000, 10000);
  shutter.setTiltingTime(1000);
  shutter.setTiltControlType(
      SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING);
  shutter.setCurrentPosition(40, 40);
  uint8_t params[16] = {0xff, SSP_FLAG_TILT_AS_DELTA, 15};
  EXPECT_EQ(shutter.handleRemoteAction(ACTION_SHUT_PARTIALLY, params, 16), 3);
  EXPECT_EQ(shutter.getTargetTilt(), 55);
  params[2] = static_cast<uint8_t>(-15);
  EXPECT_EQ(shutter.handleRemoteAction(ACTION_SHUT_PARTIALLY, params, 16), 3);
  EXPECT_EQ(shutter.getTargetTilt(), 25);
  params[1] = 0x80;
  EXPECT_EQ(shutter.handleRemoteAction(ACTION_SHUT_PARTIALLY, params, 16), 35);
  EXPECT_EQ(shutter.getTargetTilt(), 25);
  EXPECT_EQ(shutter.handleRemoteAction(ACTION_OPEN, nullptr, 0), 33);
}
TEST(M5Sdk, ShadingDirectionDeltaAndAbsentFieldsMatchSourceSemantics) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  Supla::Control::RollerShutter shutter(1, 2, true, true);
  shutter.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  shutter.setOpenCloseTime(10000, 10000);
  shutter.setTiltingTime(1000);
  shutter.setTiltControlType(
      SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING);
  shutter.setCurrentPosition(40, 40);
  for (uint32_t action : {ACTION_SHUT_PARTIALLY, ACTION_REVEAL_PARTIALLY}) {
    for (uint8_t flags = 0; flags < 4; ++flags) {
      for (int delta : {-15, 15}) {
        const bool relativePosition = flags & SSP_FLAG_PERCENTAGE_AS_DELTA;
        const bool relativeTilt = flags & SSP_FLAG_TILT_AS_DELTA;
        const int position = relativePosition ? delta : 25;
        const int tilt = relativeTilt ? delta : 30;
        uint8_t params[16] = {static_cast<uint8_t>(position), flags,
                              static_cast<uint8_t>(tilt)};
        int target = position;
        // Preserve the existing Server conversion before Source delta math.
        if (action == ACTION_REVEAL_PARTIALLY && target >= 0)
          target = 100 - target;
        if (relativePosition) target += 40;
        target = target < 0 ? 0 : target > 100 ? 100 : target;
        ASSERT_EQ(shutter.handleRemoteAction(action, params, 16), 3);
        EXPECT_EQ(shutter.getTargetPosition(), target);
        EXPECT_EQ(shutter.getTargetTilt(), relativeTilt ? 40 + delta : 30);
      }
    }
    uint8_t absentPosition[16] = {0xff, 0, 25};
    ASSERT_EQ(shutter.handleRemoteAction(action, absentPosition, 16), 3);
    EXPECT_EQ(shutter.getTargetPosition(), -1);
    EXPECT_EQ(shutter.getTargetTilt(), 25);
    uint8_t absentTilt[16] = {25, 0, 0xff};
    ASSERT_EQ(shutter.handleRemoteAction(action, absentTilt, 16), 3);
    EXPECT_EQ(shutter.getTargetPosition(),
              action == ACTION_REVEAL_PARTIALLY ? 75 : 25);
    EXPECT_EQ(shutter.getTargetTilt(), -1);
  }
}
class NativeLight : public Supla::Control::LightingPwmBase {
 public:
  void setRGBCCTValueOnDevice(uint32_t[5], int) override {}
};
TEST(M5Sdk, RgbwPackedParametersValidateBeforeMutation) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  time.advance(500);
  NativeLight light;
  light.onInit();
  uint8_t params[16] = {40, 60, 0x33, 0x22, 0x11, 0, 0, 0xff, 0, 0xff};
  EXPECT_EQ(light.handleRemoteAction(ACTION_SET_RGBW_PARAMETERS,
            params, 16), 3);
  time.advance(500);
  light.iterateAlways();
  EXPECT_EQ(light.getChannel()->getValueBrightness(), 40);
  EXPECT_EQ(light.getChannel()->getValueColorBrightness(), 60);
  EXPECT_EQ(light.getChannel()->getValueRed(), 0x11);
  params[15] = 1;
  EXPECT_EQ(light.handleRemoteAction(ACTION_SET_RGBW_PARAMETERS,
            params, 16), 35);
  EXPECT_EQ(light.getChannel()->getValueBrightness(), 40);
  light.setDefaultFunction(SUPLA_CHANNELFNC_NONE);
  EXPECT_EQ(light.handleRemoteAction(ACTION_SET_RGBW_PARAMETERS,
            params, 16), 33);
}
TEST(M5Sdk, HvacPackedNativeParametersUseExistingModeHandler) {
  Supla::Channel::resetToDefaults();
  SimpleTime time;
  OutputSimulator output;
  Supla::Control::HvacBase hvac(&output);
  hvac.getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  hvac.initDefaultConfig();
  uint8_t params[11] = {};
  params[4] = SUPLA_HVAC_MODE_HEAT;
  params[5] = 0x34;
  params[6] = 0x08;
  params[9] = SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
  EXPECT_EQ(hvac.handleRemoteAction(ACTION_HVAC_SET_PARAMETERS, params, 11), 3);
  EXPECT_EQ(hvac.getChannel()->getHvacMode(), SUPLA_HVAC_MODE_HEAT);
  params[4] = 255;
  EXPECT_EQ(hvac.handleRemoteAction(ACTION_HVAC_SET_PARAMETERS,
            params, 11), 35);
  EXPECT_EQ(hvac.handleRemoteAction(ACTION_HVAC_SET_PARAMETERS,
            params, 10), 35);
  EXPECT_EQ(hvac.handleRemoteAction(ACTION_OPEN, nullptr, 0), 33);
}
class DeletingHandler : public Supla::ActionHandler {
 public:
  Supla::LocalAction **trigger = nullptr;
  void handleAction(int, int) override {
    delete *trigger;
    *trigger = nullptr;
  }
};
TEST(M5Sdk, LocalActionRetiresExecutingRegistrationUntilDispatchReturns) {
  auto trigger = new Supla::LocalAction;
  DeletingHandler handler;
  handler.trigger = &trigger;
  trigger->addAction(1, handler, Supla::ON_CHANGE);
  trigger->addAction(2, handler, Supla::ON_CHANGE);
  trigger->runAction(Supla::ON_CHANGE);
  EXPECT_EQ(trigger, nullptr);
  EXPECT_EQ(Supla::ActionHandlerClient::begin, nullptr);
}
TEST(M5Sdk, UnavailableAndUnsupportedNumericStateAreNeverZero) {
  Supla::ChannelState unavailable;
  EXPECT_TRUE(std::isnan(unavailable.getValueDouble()));
  TDS_SuplaDeviceChannel_E snapshot = {};
  snapshot.Type = SUPLA_CHANNELTYPE_CONTAINER;
  snapshot.Default = SUPLA_CHANNELFNC_CONTAINER;
  snapshot.value[0] = 51;
  Supla::ChannelState container(&snapshot, true);
  EXPECT_DOUBLE_EQ(container.getValueDouble(), 50);
  snapshot.Type = SUPLA_CHANNELTYPE_ELECTRICITY_METER;
  EXPECT_TRUE(std::isnan(container.getValueDouble()));
}
}  // namespace
