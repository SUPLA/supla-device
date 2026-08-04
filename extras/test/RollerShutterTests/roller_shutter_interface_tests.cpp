/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include <arduino_mock.h>
#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <storage_mock.h>
#include <supla/actions.h>
#include <supla/channel.h>
#include <supla/control/roller_shutter_interface.h>
#include <supla/device/register_device.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::StrEq;

class RollerShutterInterfaceFixture : public testing::Test {
 public:
  DigitalInterfaceMock ioMock;
  SimpleTime time;

  RollerShutterInterfaceFixture() {
  }

  virtual ~RollerShutterInterfaceFixture() {
  }

  void SetUp() {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() {
    Supla::Channel::resetToDefaults();
  }
};

TEST_F(RollerShutterInterfaceFixture, basicTests) {
  Supla::Control::RollerShutterInterface rs;

  int number = rs.getChannelNumber();
  ASSERT_EQ(number, 0);
  TDSC_RollerShutterValue value = {};
  EXPECT_EQ(rs.getChannel()->getChannelType(), SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(rs.getChannel()->getFuncList(),
            SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER |
                SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW |
                SUPLA_BIT_FUNC_TERRACE_AWNING |
                SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR | SUPLA_BIT_FUNC_CURTAIN |
                SUPLA_BIT_FUNC_PROJECTOR_SCREEN);

  EXPECT_EQ(rs.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  EXPECT_EQ(rs.getChannel()->getFlags(),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
                SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(
      rs.isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER));
  EXPECT_TRUE(
      rs.isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW));
  EXPECT_TRUE(rs.isFunctionSupported(SUPLA_CHANNELFNC_TERRACE_AWNING));
  EXPECT_TRUE(rs.isFunctionSupported(SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR));
  EXPECT_TRUE(rs.isFunctionSupported(SUPLA_CHANNELFNC_CURTAIN));
  EXPECT_TRUE(rs.isFunctionSupported(SUPLA_CHANNELFNC_PROJECTOR_SCREEN));

  EXPECT_FALSE(
      rs.isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND));
  EXPECT_FALSE(rs.isFunctionSupported(SUPLA_CHANNELFNC_VERTICAL_BLIND));
  EXPECT_FALSE(rs.isFunctionSupported(SUPLA_CHANNELFNC_LIGHTSWITCH));
  EXPECT_FALSE(rs.isFunctionSupported(0));

  EXPECT_FALSE(rs.isAutoCalibrationSupported());
}

TEST_F(RollerShutterInterfaceFixture, rsLocalMovement) {
  Supla::Control::RollerShutterInterface rs;
  rs.getChannel()->setFlag(SUPLA_CHANNEL_FLAG_TIME_SETTING_NOT_AVAILABLE);

  EXPECT_EQ(rs.getChannelNumber(), 0);

  TDSC_RollerShutterValue value = {};
  TDSC_RollerShutterValue *valuePtr =
      reinterpret_cast<TDSC_RollerShutterValue *>(
          Supla::RegisterDevice::getChannelValuePtr(0));
  EXPECT_EQ(0, memcmp(valuePtr, &value, SUPLA_CHANNELVALUE_SIZE));

  rs.onInit();

  value.position = -1;
  EXPECT_EQ(0, memcmp(valuePtr, &value, SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getTargetPosition(), STOP_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getClosingTimeMs(), 0);
  EXPECT_EQ(rs.getOpeningTimeMs(), 0);

  rs.iterateAlways();

  rs.setTargetPosition(50);
  rs.iterateAlways();
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getTargetPosition(), 50);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getClosingTimeMs(), 0);
  EXPECT_EQ(rs.getOpeningTimeMs(), 0);

  rs.stop();
  rs.iterateAlways();
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getTargetPosition(), STOP_REQUEST);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.moveUp();
  rs.iterateAlways();
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getTargetPosition(), MOVE_UP_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.moveDown();
  rs.iterateAlways();
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getTargetPosition(), MOVE_DOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.setCurrentPosition(55);
  rs.iterateAlways();
  value.position = 55;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), 55);
  EXPECT_EQ(rs.getTargetPosition(), MOVE_DOWN_POSITION);
  EXPECT_TRUE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
}

TEST_F(RollerShutterInterfaceFixture, setOpenCloseTimeClampsUnsafeValues) {
  Supla::Control::RollerShutterInterface rs;

  rs.setOpenCloseTime(RS_MAX_OPERATION_TIME_MS + 1,
                      RS_MAX_OPERATION_TIME_MS + 1234);

  EXPECT_EQ(rs.getClosingTimeMs(), RS_MAX_OPERATION_TIME_MS);
  EXPECT_EQ(rs.getOpeningTimeMs(), RS_MAX_OPERATION_TIME_MS);
}

TEST_F(RollerShutterInterfaceFixture, setTiltingTimeClampsUnsafeValues) {
  Supla::Control::RollerShutterInterface rs(true);

  rs.setTiltingTime(RS_MAX_OPERATION_TIME_MS + 1, false);

  EXPECT_EQ(rs.getTiltingTimeMs(), RS_MAX_OPERATION_TIME_MS);
}

TEST_F(RollerShutterInterfaceFixture, facadeBlindBasicTests) {
  Supla::Control::RollerShutterInterface rs(true);

  int number = rs.getChannelNumber();
  ASSERT_EQ(number, 0);
  TDSC_RollerShutterValue value = {};
  EXPECT_EQ(rs.getChannel()->getChannelType(), SUPLA_CHANNELTYPE_RELAY);
  EXPECT_EQ(rs.getChannel()->getFuncList(),
            SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER |
                SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW |
                SUPLA_BIT_FUNC_TERRACE_AWNING |
                SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR | SUPLA_BIT_FUNC_CURTAIN |
                SUPLA_BIT_FUNC_PROJECTOR_SCREEN |
                SUPLA_BIT_FUNC_CONTROLLINGTHEFACADEBLIND |
                SUPLA_BIT_FUNC_VERTICAL_BLIND);

  EXPECT_EQ(rs.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  EXPECT_EQ(rs.getChannel()->getFlags(),
            SUPLA_CHANNEL_FLAG_CHANNELSTATE |
                SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));
  EXPECT_TRUE(
      rs.isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER));
  EXPECT_TRUE(
      rs.isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW));
  EXPECT_TRUE(rs.isFunctionSupported(SUPLA_CHANNELFNC_TERRACE_AWNING));
  EXPECT_TRUE(rs.isFunctionSupported(SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR));
  EXPECT_TRUE(rs.isFunctionSupported(SUPLA_CHANNELFNC_CURTAIN));
  EXPECT_TRUE(rs.isFunctionSupported(SUPLA_CHANNELFNC_PROJECTOR_SCREEN));

  EXPECT_TRUE(
      rs.isFunctionSupported(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND));
  EXPECT_TRUE(rs.isFunctionSupported(SUPLA_CHANNELFNC_VERTICAL_BLIND));
  EXPECT_FALSE(rs.isFunctionSupported(SUPLA_CHANNELFNC_LIGHTSWITCH));
  EXPECT_FALSE(rs.isFunctionSupported(0));

  EXPECT_FALSE(rs.isAutoCalibrationSupported());
}

TEST_F(RollerShutterInterfaceFixture,
       facadeBlindTimingValidationIsModeSpecific) {
  using Supla::Control::isValidFacadeBlindTiming;
  using Supla::Control::isValidTiltControlType;

  EXPECT_TRUE(isValidTiltControlType(SUPLA_TILT_CONTROL_TYPE_UNKNOWN));
  EXPECT_TRUE(isValidTiltControlType(
      SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING));
  EXPECT_TRUE(isValidTiltControlType(
      SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING));
  EXPECT_TRUE(isValidTiltControlType(
      SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED));
  EXPECT_FALSE(isValidTiltControlType(4));
  EXPECT_FALSE(isValidTiltControlType(255));
  EXPECT_FALSE(isValidTiltControlType(257));
  EXPECT_FALSE(isValidTiltControlType(UINT32_MAX));

  for (uint8_t type : {SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING,
                       SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED}) {
    EXPECT_FALSE(isValidFacadeBlindTiming(type, 5000, 5000, 5000));
    EXPECT_FALSE(isValidFacadeBlindTiming(type, 5000, 5001, 5000));
    EXPECT_FALSE(isValidFacadeBlindTiming(type, 5001, 5000, 5000));
    EXPECT_FALSE(isValidFacadeBlindTiming(type, 4999, 6000, 5000));
    EXPECT_FALSE(isValidFacadeBlindTiming(type, 6000, 4999, 5000));
    EXPECT_TRUE(isValidFacadeBlindTiming(type, 5001, 5001, 5000));
  }

  EXPECT_TRUE(isValidFacadeBlindTiming(
      SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
      5000,
      5000,
      10000));
  EXPECT_TRUE(isValidFacadeBlindTiming(
      SUPLA_TILT_CONTROL_TYPE_UNKNOWN, 1, 1, RS_MAX_OPERATION_TIME_MS));
}

TEST_F(RollerShutterInterfaceFixture,
       applyFacadeBlindConfigUsesEffectiveNegativeValuesTransactionally) {
  Supla::Control::RollerShutterInterface rs(true);
  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  rs.setOpenCloseTime(5000, 5000);
  rs.setTiltingTime(1000, false);
  rs.setTiltControlType(SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
                        false);

  TSD_ChannelConfig channelConfig = {};
  channelConfig.Func = SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND;
  channelConfig.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  channelConfig.ConfigSize = sizeof(TChannelConfig_FacadeBlind);
  auto config = reinterpret_cast<TChannelConfig_FacadeBlind *>(
      channelConfig.Config);
  config->OpeningTimeMS = -1;
  config->ClosingTimeMS = 6000;
  config->TiltingTimeMS = -1;
  config->ButtonsUpsideDown = 1;
  config->MotorUpsideDown = 1;
  config->TimeMargin = -1;
  config->TiltControlType =
      SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING;

  EXPECT_EQ(rs.applyChannelConfig(&channelConfig),
            Supla::ApplyConfigResult::Success);
  EXPECT_EQ(rs.getOpeningTimeMs(), 5000);
  EXPECT_EQ(rs.getClosingTimeMs(), 6000);
  EXPECT_EQ(rs.getTiltingTimeMs(), 1000);

  config->OpeningTimeMS = 5000;
  config->ClosingTimeMS = 5000;
  config->TiltingTimeMS = 10000;
  config->TiltControlType =
      SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING;

  EXPECT_EQ(rs.handleChannelConfig(&channelConfig, false),
            SUPLA_CONFIG_RESULT_DATA_ERROR);
  EXPECT_EQ(rs.getOpeningTimeMs(), 5000);
  EXPECT_EQ(rs.getClosingTimeMs(), 6000);
  EXPECT_EQ(rs.getTiltingTimeMs(), 1000);
  EXPECT_EQ(rs.getTiltControlType(),
            SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING);

  TChannelConfig_FacadeBlind reported = {};
  int size = 0;
  rs.fillChannelConfig(&reported, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(size, sizeof(reported));
  EXPECT_EQ(reported.OpeningTimeMS, 5000);
  EXPECT_EQ(reported.ClosingTimeMS, 6000);
  EXPECT_EQ(reported.TiltingTimeMS, 1000);
  EXPECT_EQ(reported.TiltControlType,
            SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING);

  config->OpeningTimeMS = 5000;
  config->ClosingTimeMS = 5000;
  config->TiltingTimeMS = 10000;
  config->TiltControlType =
      SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING;
  EXPECT_EQ(rs.applyChannelConfig(&channelConfig),
            Supla::ApplyConfigResult::Success);
  EXPECT_EQ(rs.getTiltingTimeMs(), 10000);
  EXPECT_TRUE(rs.isTiltConfigured());
}

TEST_F(RollerShutterInterfaceFixture,
       facadeBlindConfigValidatesEffectiveUnavailableTimes) {
  Supla::Control::RollerShutterInterface rs(true);
  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  rs.setOpenCloseTime(5000, 5000);
  rs.setTiltingTime(10000, false);
  rs.setTiltControlType(SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
                        false);
  rs.getChannel()->setFlag(SUPLA_CHANNEL_FLAG_TIME_SETTING_NOT_AVAILABLE);

  TSD_ChannelConfig channelConfig = {};
  channelConfig.Func = SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND;
  channelConfig.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  channelConfig.ConfigSize = sizeof(TChannelConfig_FacadeBlind);
  auto config = reinterpret_cast<TChannelConfig_FacadeBlind *>(
      channelConfig.Config);
  config->OpeningTimeMS = 20000;
  config->ClosingTimeMS = 20000;
  config->TiltingTimeMS = 10000;
  config->TiltControlType =
      SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING;

  EXPECT_EQ(rs.applyChannelConfig(&channelConfig),
            Supla::ApplyConfigResult::DataError);
  EXPECT_EQ(rs.getOpeningTimeMs(), 5000);
  EXPECT_EQ(rs.getClosingTimeMs(), 5000);
  EXPECT_EQ(rs.getTiltingTimeMs(), 10000);
  EXPECT_EQ(rs.getTiltControlType(),
            SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING);
  EXPECT_TRUE(rs.isTiltConfigured());
}

TEST_F(RollerShutterInterfaceFixture,
       invalidStoredTiltConfigIsDisabledAfterStateTimesAreLoaded) {
  ConfigMock config;
  StorageMock storage;
  Supla::Control::RollerShutterInterface rs(true);
  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  Supla::Control::TiltConfig storedTilt = {};
  storedTilt.tiltingTime = 5000;
  storedTilt.tiltControlType =
      SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING;

  EXPECT_CALL(config, init()).WillRepeatedly(Return(true));
  EXPECT_CALL(config, getInt32(StrEq("0_fnc"), _))
      .WillOnce([](const char*, int32_t* value) {
        *value = -1;
        return true;
      });
  EXPECT_CALL(config, getUInt8(StrEq("0_cfg_chng"), _))
      .WillOnce(Return(false));
  EXPECT_CALL(config,
              getBlob(StrEq("0_rs_cfg"), _,
                      sizeof(Supla::Control::RollerShutterConfig)))
      .WillOnce(Return(false));
  EXPECT_CALL(config,
              getBlob(StrEq("0_tilt_cfg"),
                      _,
                      sizeof(Supla::Control::TiltConfig)))
      .WillOnce([&storedTilt](const char*, char* value, size_t size) {
        memcpy(value, &storedTilt, size);
        return true;
      });
  EXPECT_CALL(config,
              setBlob(StrEq("0_rs_cfg"), _,
                      sizeof(Supla::Control::RollerShutterConfig)))
      .WillOnce(Return(false));
  EXPECT_CALL(config,
              setBlob(StrEq("0_tilt_cfg"),
                      _,
                      sizeof(Supla::Control::TiltConfig)))
      .WillOnce(Return(false));

  rs.onLoadConfig(nullptr);
  EXPECT_EQ(rs.getTiltingTimeMs(), 5000);

#pragma pack(push, 1)
  struct StoredState {
    uint32_t closingTimeMs;
    uint32_t openingTimeMs;
    int8_t currentPosition;
    int8_t tiltPosition;
  } state = {5000, 5000, 0, 0};
#pragma pack(pop)
  storage.defaultInitialization(sizeof(state));
  EXPECT_CALL(storage, readStorage(_, _, sizeof(state), _))
      .WillOnce([&state](uint32_t, unsigned char* value, int, bool) {
        memcpy(value, &state, sizeof(state));
        return sizeof(state);
      });
  Supla::Storage::LoadStateStorage();
  rs.onLoadState();

  EXPECT_EQ(rs.getTiltingTimeMs(), 0);
  EXPECT_EQ(rs.getTiltControlType(), SUPLA_TILT_CONTROL_TYPE_UNKNOWN);
  EXPECT_FALSE(rs.isTiltConfigured());
  TChannelConfig_FacadeBlind reported = {};
  int size = 0;
  rs.fillChannelConfig(&reported, &size, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(reported.TiltingTimeMS, 0);
  EXPECT_EQ(reported.TiltControlType, SUPLA_TILT_CONTROL_TYPE_UNKNOWN);
}

TEST_F(RollerShutterInterfaceFixture, facadeBlindLocalMovement) {
  Supla::Control::RollerShutterInterface rs(true);
  rs.getChannel()->setFlag(SUPLA_CHANNEL_FLAG_TIME_SETTING_NOT_AVAILABLE);

  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);

  EXPECT_EQ(rs.getChannelNumber(), 0);

  TDSC_FacadeBlindValue value = {};
  TDSC_FacadeBlindValue *valuePtr = reinterpret_cast<TDSC_FacadeBlindValue *>(
      Supla::RegisterDevice::getChannelValuePtr(0));
  EXPECT_EQ(0, memcmp(valuePtr, &value, SUPLA_CHANNELVALUE_SIZE));

  rs.onInit();

  value.position = -1;
  value.tilt = -1;
  value.flags = RS_VALUE_FLAG_TILT_IS_SET;
  EXPECT_EQ(0, memcmp(valuePtr, &value, SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getCurrentTilt(), UNKNOWN_POSITION);      // not calibrated
  EXPECT_EQ(rs.getTargetPosition(), STOP_POSITION);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getClosingTimeMs(), 0);
  EXPECT_EQ(rs.getOpeningTimeMs(), 0);

  rs.iterateAlways();

  rs.setTargetPosition(50);
  rs.iterateAlways();
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getCurrentTilt(), UNKNOWN_POSITION);
  EXPECT_EQ(rs.getTargetPosition(), 50);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getClosingTimeMs(), 0);
  EXPECT_EQ(rs.getOpeningTimeMs(), 0);

  rs.stop();
  rs.iterateAlways();
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getCurrentTilt(), UNKNOWN_POSITION);
  EXPECT_EQ(rs.getTargetPosition(), STOP_REQUEST);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.moveUp();
  rs.iterateAlways();
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getCurrentTilt(), UNKNOWN_POSITION);
  EXPECT_EQ(rs.getTargetPosition(), MOVE_UP_POSITION);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.moveDown();
  rs.iterateAlways();
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);  // not calibrated
  EXPECT_EQ(rs.getCurrentTilt(), UNKNOWN_POSITION);
  EXPECT_EQ(rs.getTargetPosition(), MOVE_DOWN_POSITION);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.setCurrentPosition(55);
  rs.iterateAlways();
  value.position = 55;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), 55);
  EXPECT_EQ(rs.getCurrentTilt(), UNKNOWN_POSITION);
  EXPECT_EQ(rs.getTargetPosition(), MOVE_DOWN_POSITION);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
  EXPECT_TRUE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.setCurrentPosition(55, 98);
  rs.iterateAlways();
  value.position = 55;
  value.tilt = 98;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), 55);
  EXPECT_EQ(rs.getCurrentTilt(), 98);
  EXPECT_EQ(rs.getTargetPosition(), MOVE_DOWN_POSITION);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
  EXPECT_TRUE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.setTargetPosition(42, 13);
  rs.iterateAlways();
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  EXPECT_EQ(rs.getCurrentPosition(), 55);
  EXPECT_EQ(rs.getCurrentTilt(), 98);
  EXPECT_EQ(rs.getTargetPosition(), 42);
  EXPECT_EQ(rs.getTargetTilt(), 13);
  EXPECT_TRUE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getClosingTimeMs(), 0);
  EXPECT_EQ(rs.getOpeningTimeMs(), 0);

  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  rs.iterateAlways();
  value.tilt = 0;
  value.flags = 0;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));
}
