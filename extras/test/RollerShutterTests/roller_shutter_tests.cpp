// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <arduino_mock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <storage_mock.h>
#include <supla/actions.h>
#include <supla/channel.h>
#include <supla/control/bistable_roller_shutter.h>
#include <supla/control/roller_shutter.h>
#include <supla/control/tripple_button_roller_shutter.h>
#include <supla/device/register_device.h>
#include <supla_io_mock.h>

#include "gmock/gmock.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

namespace {

Supla::Io::IoPin MakeOutputPin(Supla::Io::Base *io, int pin, bool highIsOn) {
  Supla::Io::IoPin result(pin, io);
  result.setActiveHigh(highIsOn);
  result.setMode(OUTPUT);
  return result;
}

}  // namespace

class RollerShutterFixture : public testing::Test {
 public:
  DigitalInterfaceMock ioMock;
  SimpleTime time;
  int gpioUp = 1;
  int gpioDown = 2;

  RollerShutterFixture() {
  }

  virtual ~RollerShutterFixture() {
  }

  void SetUp() {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() {
    Supla::Channel::resetToDefaults();
  }
};

class RollerShutterTestAccess : public Supla::Control::RollerShutter {
 public:
  using Supla::Control::RollerShutter::RollerShutter;

  uint32_t getOperationTimeoutMs() const {
    return operationTimeoutMs;
  }

  void injectTiltOnlyTarget(int tilt) {
    targetPosition = UNKNOWN_POSITION;
    targetTilt = tilt;
    newTargetPositionAvailable = true;
  }

  void activateUpRelay() {
    relayUpOn();
  }

  void activateDownRelay() {
    relayDownOn();
  }
};

TEST_F(RollerShutterFixture, basicTests) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

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
                SUPLA_CHANNEL_FLAG_RS_SBS_AND_STOP_ACTIONS |
                SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE |
                SUPLA_CHANNEL_FLAG_CALCFG_RECALIBRATE);
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(number),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));
}

TEST_F(RollerShutterFixture, onInitHighIsOn) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

  rs.onInit();
}

TEST_F(RollerShutterFixture, onInitLowIsOn) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown, false);

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));
  EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

  rs.onInit();
}

TEST_F(RollerShutterFixture, ioPinConstructorUsesSeparateIo) {
  SuplaIoMock ioMockUp;
  SuplaIoMock ioMockDown;

  Supla::Control::RollerShutter rs(MakeOutputPin(&ioMockUp, gpioUp, false),
                                   MakeOutputPin(&ioMockDown, gpioDown, true));

  ::testing::InSequence seq;

  EXPECT_CALL(ioMockUp, customDigitalWrite(0, gpioUp, HIGH));
  EXPECT_CALL(ioMockUp, customPinMode(0, gpioUp, OUTPUT));
  EXPECT_CALL(ioMockDown, customDigitalWrite(0, gpioDown, LOW));
  EXPECT_CALL(ioMockDown, customPinMode(0, gpioDown, OUTPUT));

  rs.onInit();
}

TEST_F(RollerShutterFixture, setPinUpDeactivatesActiveOldPinFirst) {
  SuplaIoMock ioUp;
  SuplaIoMock ioDown;
  RollerShutterTestAccess rs(MakeOutputPin(&ioUp, gpioUp, true),
                             MakeOutputPin(&ioDown, gpioDown, true));
  constexpr int newPin = 3;

  EXPECT_CALL(ioUp, customDigitalWrite(0, gpioUp, HIGH));
  rs.activateUpRelay();
  testing::Mock::VerifyAndClearExpectations(&ioUp);

  testing::InSequence sequence;
  EXPECT_CALL(ioUp, customDigitalWrite(0, gpioUp, LOW));
  EXPECT_CALL(ioUp, customDigitalWrite(0, newPin, LOW));
  EXPECT_CALL(ioUp, customPinMode(0, newPin, OUTPUT));

  rs.setPinUp(newPin);
}

TEST_F(RollerShutterFixture, setPinDownDeactivatesActiveLowOldPinFirst) {
  SuplaIoMock ioUp;
  SuplaIoMock ioDown;
  RollerShutterTestAccess rs(MakeOutputPin(&ioUp, gpioUp, true),
                             MakeOutputPin(&ioDown, gpioDown, false));
  constexpr int newPin = 3;

  EXPECT_CALL(ioDown, customDigitalWrite(0, gpioDown, LOW));
  rs.activateDownRelay();
  testing::Mock::VerifyAndClearExpectations(&ioDown);

  testing::InSequence sequence;
  EXPECT_CALL(ioDown, customDigitalWrite(0, gpioDown, HIGH));
  EXPECT_CALL(ioDown, customDigitalWrite(0, newPin, HIGH));
  EXPECT_CALL(ioDown, customPinMode(0, newPin, OUTPUT));

  rs.setPinDown(newPin);
}

TEST_F(RollerShutterFixture, setPinUpFromUnsetDoesNotWriteOldPin) {
  SuplaIoMock ioUp;
  SuplaIoMock ioDown;
  RollerShutterTestAccess rs(MakeOutputPin(&ioUp, -1, true),
                             MakeOutputPin(&ioDown, gpioDown, true));
  constexpr int newPin = 3;

  testing::InSequence sequence;
  EXPECT_CALL(ioUp, customDigitalWrite(0, newPin, LOW)).Times(1);
  EXPECT_CALL(ioUp, customPinMode(0, newPin, OUTPUT)).Times(1);

  rs.setPinUp(newPin);
}

TEST_F(RollerShutterFixture, setPinUpToSamePinDoesNotAddDeactivation) {
  SuplaIoMock ioUp;
  SuplaIoMock ioDown;
  RollerShutterTestAccess rs(MakeOutputPin(&ioUp, gpioUp, true),
                             MakeOutputPin(&ioDown, gpioDown, true));

  testing::InSequence sequence;
  EXPECT_CALL(ioUp, customDigitalWrite(0, gpioUp, LOW)).Times(1);
  EXPECT_CALL(ioUp, customPinMode(0, gpioUp, OUTPUT)).Times(1);

  rs.setPinUp(gpioUp);
}

TEST_F(RollerShutterFixture, setPinUpToUnsetDeactivatesOldPinFirst) {
  SuplaIoMock ioUp;
  SuplaIoMock ioDown;
  RollerShutterTestAccess rs(MakeOutputPin(&ioUp, gpioUp, true),
                             MakeOutputPin(&ioDown, gpioDown, true));

  EXPECT_CALL(ioUp, customDigitalWrite(0, gpioUp, HIGH));
  rs.activateUpRelay();
  testing::Mock::VerifyAndClearExpectations(&ioUp);

  EXPECT_CALL(ioUp, customDigitalWrite(0, gpioUp, LOW)).Times(1);
  EXPECT_CALL(ioUp, customPinMode(_, _, _)).Times(0);

  rs.setPinUp(-1);
}

TEST_F(RollerShutterFixture, unsetIoPinsDoNothing) {
  Supla::Control::RollerShutter rs;

  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(0);
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(0);

  rs.onInit();
}

TEST_F(RollerShutterFixture,
       incompleteConfigurationWithUpPinUnsetDoesNotStartMovementOrCalibration) {
  SuplaIoMock ioUp;
  SuplaIoMock ioDown;
  Supla::Control::RollerShutter rs(MakeOutputPin(&ioUp, -1, true),
                                   MakeOutputPin(&ioDown, gpioDown, true));

  EXPECT_CALL(ioUp, customDigitalWrite(_, _, _)).Times(0);
  EXPECT_CALL(ioUp, customPinMode(_, _, _)).Times(0);
  EXPECT_CALL(ioDown, customDigitalWrite(0, gpioDown, LOW)).Times(AtLeast(1));
  EXPECT_CALL(ioDown, customDigitalWrite(0, gpioDown, HIGH)).Times(0);
  EXPECT_CALL(ioDown, customPinMode(0, gpioDown, OUTPUT));

  rs.onInit();
  rs.setRsConfigMotorUpsideDownValue(2);
  EXPECT_EQ(rs.getMotorUpsideDown(), 2);

  rs.handleAction(0, Supla::MOVE_UP);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.handleAction(0, Supla::MOVE_DOWN);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.setOpenCloseTime(10000, 10000);
  rs.triggerCalibration();
  rs.onTimer();
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
}

TEST_F(
    RollerShutterFixture,
    incompleteConfigurationWithDownPinUnsetDoesNotStartMovementOrCalibration) {
  SuplaIoMock ioUp;
  SuplaIoMock ioDown;
  Supla::Control::RollerShutter rs(MakeOutputPin(&ioUp, gpioUp, true),
                                   MakeOutputPin(&ioDown, -1, true));

  EXPECT_CALL(ioDown, customDigitalWrite(_, _, _)).Times(0);
  EXPECT_CALL(ioDown, customPinMode(_, _, _)).Times(0);
  EXPECT_CALL(ioUp, customDigitalWrite(0, gpioUp, LOW)).Times(AtLeast(1));
  EXPECT_CALL(ioUp, customDigitalWrite(0, gpioUp, HIGH)).Times(0);
  EXPECT_CALL(ioUp, customPinMode(0, gpioUp, OUTPUT));

  rs.onInit();

  rs.handleAction(0, Supla::MOVE_DOWN);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.handleAction(0, Supla::MOVE_UP);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.setOpenCloseTime(10000, 10000);
  rs.triggerCalibration();
  rs.onTimer();
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
}

#pragma pack(push, 1)
struct RollerShutterStateDataTests {
  uint32_t closingTimeMs;
  uint32_t openingTimeMs;
  int8_t currentPosition;  // 0 - closed; 100 - opened
};

struct RollerShutterWithTiltStateDataTests {
  uint32_t closingTimeMs;
  uint32_t openingTimeMs;
  int8_t currentPosition;
  int8_t tiltPosition;
};
#pragma pack(pop)

TEST_F(RollerShutterFixture,
       isCalibratedRequiresKnownPositionWithConfiguredTimes) {
  Supla::Control::RollerShutterInterface rs;

  rs.setOpenCloseTime(10000, 10000);
  rs.setCalibrationFinished();

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_FALSE(rs.isCalibrated());
}

TEST_F(RollerShutterFixture,
       unknownStandardStateRequestsCalibrationAndAbsoluteOpenStartsOpening) {
  StorageMock storage;
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  RollerShutterStateDataTests state = {.closingTimeMs = 10000,
                                      .openingTimeMs = 10000,
                                      .currentPosition = UNKNOWN_POSITION};
  storage.defaultInitialization(sizeof(state));
  EXPECT_CALL(storage, readStorage(_, _, sizeof(state), _))
      .WillOnce([&state](uint32_t, unsigned char *data, int, bool) {
        memcpy(data, &state, sizeof(state));
        return sizeof(state);
      });
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

  Supla::Storage::LoadStateStorage();
  rs.onInit();

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_TRUE(rs.isCalibrationRequested());

  TSD_SuplaChannelNewValue newValue = {};
  newValue.DurationMS = (100 << 16) | 100;
  newValue.value[0] = 10;  // absolute OPEN target

  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

  rs.handleNewValueFromServer(&newValue);
  rs.onTimer();

  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::UP_DIR));
}

TEST_F(RollerShutterFixture,
       knownStandardStateRemainsCalibratedAndAbsoluteOpenStartsOpening) {
  StorageMock storage;
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  RollerShutterStateDataTests state = {
      .closingTimeMs = 10000, .openingTimeMs = 10000, .currentPosition = 50};
  storage.defaultInitialization(sizeof(state));
  EXPECT_CALL(storage, readStorage(_, _, sizeof(state), _))
      .WillOnce([&state](uint32_t, unsigned char *data, int, bool) {
        memcpy(data, &state, sizeof(state));
        return sizeof(state);
      });
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

  Supla::Storage::LoadStateStorage();
  rs.onInit();

  EXPECT_EQ(rs.getCurrentPosition(), 50);
  EXPECT_TRUE(rs.isCalibrated());
  EXPECT_FALSE(rs.isCalibrationRequested());

  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

  rs.open();
  rs.onTimer();

  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::UP_DIR));
}

TEST_F(RollerShutterFixture,
       unknownTiltStateRequestsCalibrationAndClearsRestoredTilt) {
  StorageMock storage;
  Supla::Control::RollerShutter rs(gpioUp, gpioDown, true, true);
  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  rs.setTiltingTime(1000, false);
  rs.setTiltControlType(SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
                        false);

  RollerShutterWithTiltStateDataTests state = {
      .closingTimeMs = 10000,
      .openingTimeMs = 10000,
      .currentPosition = UNKNOWN_POSITION,
      .tiltPosition = 40};
  storage.defaultInitialization(sizeof(state));
  EXPECT_CALL(storage, readStorage(_, _, sizeof(state), _))
      .WillOnce([&state](uint32_t, unsigned char *data, int, bool) {
        memcpy(data, &state, sizeof(state));
        return sizeof(state);
      });
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

  Supla::Storage::LoadStateStorage();
  rs.onInit();

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);
  EXPECT_EQ(rs.getCurrentTilt(), UNKNOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_TRUE(rs.isCalibrationRequested());
}

TEST_F(RollerShutterFixture, notCalibratedStartup) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  ::testing::InSequence seq;

  // init
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

  // move down
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

  // move up - it first call stop
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

  // then actual move up:
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

  // stop
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

  rs.onInit();

  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  TDSC_RollerShutterValue value = {};
  value.position = -1;
  TDSC_RollerShutterValue *valuePtr =
      reinterpret_cast<TDSC_RollerShutterValue *>(
          Supla::RegisterDevice::getChannelValuePtr(0));
  EXPECT_EQ(0, memcmp(valuePtr, &value, SUPLA_CHANNELVALUE_SIZE));

  rs.handleAction(0, Supla::MOVE_DOWN);
  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  rs.handleAction(0, Supla::MOVE_UP);
  for (int i = 0; i < 100; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  rs.handleAction(0, Supla::STOP);
  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }
}

TEST_F(RollerShutterFixture,
       notCalibratedServerOpenWithZeroTimesStartsOpening) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

  rs.onInit();

  TDSC_RollerShutterValue value = {};
  value.position = -1;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  TSD_SuplaChannelNewValue newValueFromServer = {};
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.DurationMS = 0;
  reinterpret_cast<TCSD_RollerShutterValue *>(newValueFromServer.value)
      ->position = 10;  // open

  rs.handleNewValueFromServer(&newValueFromServer);
  EXPECT_EQ(rs.getTargetPosition(), 0);
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getClosingTimeMs(), 0);
  EXPECT_EQ(rs.getOpeningTimeMs(), 0);

  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

  rs.onTimer();
  rs.iterateAlways();

  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::UP_DIR));
  EXPECT_EQ(rs.getTargetPosition(), 0);

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
}

TEST_F(RollerShutterFixture,
       notCalibratedServerCloseWithZeroTimesStartsClosing) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

  rs.onInit();

  TDSC_RollerShutterValue value = {};
  value.position = -1;
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  TSD_SuplaChannelNewValue newValueFromServer = {};
  newValueFromServer.ChannelNumber = 0;
  newValueFromServer.DurationMS = 0;
  reinterpret_cast<TCSD_RollerShutterValue *>(newValueFromServer.value)
      ->position = 110;  // close

  rs.handleNewValueFromServer(&newValueFromServer);
  EXPECT_EQ(rs.getTargetPosition(), 100);
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getClosingTimeMs(), 0);
  EXPECT_EQ(rs.getOpeningTimeMs(), 0);

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

  rs.onTimer();
  rs.iterateAlways();

  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::DOWN_DIR));
  EXPECT_EQ(rs.getTargetPosition(), 100);

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
}

TEST_F(RollerShutterFixture,
       facadeBlindServerTargetWithUnavailableMovementTimesPreservesRequest) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown, true, true);

  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  rs.setTiltingTime(1000, false);
  rs.setTiltControlType(SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
                        false);
  rs.getChannel()->setFlag(SUPLA_CHANNEL_FLAG_TIME_SETTING_NOT_AVAILABLE);
  rs.setCurrentPosition(50, 50);

  EXPECT_TRUE(rs.isCalibrated());
  EXPECT_EQ(rs.getOpeningTimeMs(), 0);
  EXPECT_EQ(rs.getClosingTimeMs(), 0);
  EXPECT_TRUE(rs.isTiltConfigured());

  TSD_SuplaChannelNewValue newValue = {};
  newValue.value[0] = 70;  // normalized position: 60
  newValue.value[1] = 70;  // normalized tilt: 60
  newValue.DurationMS = 0;

  rs.handleNewValueFromServer(&newValue);

  EXPECT_EQ(rs.getTargetPosition(), 60);
  EXPECT_EQ(rs.getTargetTilt(), 60);
}

TEST_F(RollerShutterFixture, movementTests) {
  StorageMock storage;
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  storage.defaultInitialization(9);
  EXPECT_CALL(storage, scheduleSave(_, 1000)).Times(AtLeast(0));

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  {
    ::testing::InSequence seq;

    EXPECT_CALL(storage,
                readStorage(_, _, /* sizeof(RollerShutterStateData) */ 9, _))
        .WillOnce([](uint32_t, unsigned char *data, int, bool) {
          RollerShutterStateDataTests rsData = {.closingTimeMs = 10000,
                                                .openingTimeMs = 10000,
                                                .currentPosition = 0};
          EXPECT_EQ(9, sizeof(rsData));
          memcpy(data, &rsData, sizeof(RollerShutterStateDataTests));
          return 9;
        });

    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

    // move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // move up - it first call stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // then actual move up:
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    // stop after reaching limit
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // sbs - stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move up
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  }

  Supla::Storage::LoadStateStorage();
  rs.onInit();

  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  TDSC_RollerShutterValue value = {};
  EXPECT_EQ(0,
            memcmp(Supla::RegisterDevice::getChannelValuePtr(0),
                   &value,
                   SUPLA_CHANNELVALUE_SIZE));

  rs.handleAction(0, Supla::MOVE_DOWN);
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 9);

  rs.handleAction(0, Supla::MOVE_UP);
  // relays are disabled after 60s timeout
  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 0);

  rs.handleAction(0, Supla::STEP_BY_STEP);  // sbs - move down
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 9);

  rs.handleAction(0, Supla::STEP_BY_STEP);  // sbs - stop
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 10);

  rs.handleAction(0, Supla::STEP_BY_STEP);  // sbs - move up
  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 0);
}

TEST_F(RollerShutterFixture, bistableIoPinConstructorUsesSeparateIo) {
  SuplaIoMock ioMockUp;
  SuplaIoMock ioMockDown;

  Supla::Control::BistableRollerShutter rs(
      MakeOutputPin(&ioMockUp, gpioUp, true),
      MakeOutputPin(&ioMockDown, gpioDown, false));

  ::testing::InSequence seq;

  EXPECT_CALL(ioMockUp, customDigitalWrite(0, gpioUp, LOW));
  EXPECT_CALL(ioMockUp, customPinMode(0, gpioUp, OUTPUT));
  EXPECT_CALL(ioMockDown, customDigitalWrite(0, gpioDown, HIGH));
  EXPECT_CALL(ioMockDown, customPinMode(0, gpioDown, OUTPUT));

  rs.onInit();
}

TEST_F(RollerShutterFixture, trippleButtonIoPinConstructorUsesSeparateIo) {
  SuplaIoMock ioMockUp;
  SuplaIoMock ioMockDown;
  SuplaIoMock ioMockStop;

  Supla::Control::TrippleButtonRollerShutter rs(
      MakeOutputPin(&ioMockUp, gpioUp, true),
      MakeOutputPin(&ioMockDown, gpioDown, true),
      MakeOutputPin(&ioMockStop, 3, false));

  ::testing::InSequence seq;

  EXPECT_CALL(ioMockStop, customDigitalWrite(0, 3, HIGH));
  EXPECT_CALL(ioMockStop, customPinMode(0, 3, OUTPUT));
  EXPECT_CALL(ioMockUp, customDigitalWrite(0, gpioUp, LOW));
  EXPECT_CALL(ioMockUp, customPinMode(0, gpioUp, OUTPUT));
  EXPECT_CALL(ioMockDown, customDigitalWrite(0, gpioDown, LOW));
  EXPECT_CALL(ioMockDown, customPinMode(0, gpioDown, OUTPUT));

  rs.onInit();
}

TEST_F(RollerShutterFixture, trippleButtonIoPinConstructorSetsStopOutputMode) {
  SuplaIoMock ioMockUp;
  SuplaIoMock ioMockDown;
  SuplaIoMock ioMockStop;

  const int gpioStop = 3;
  Supla::Io::IoPin pinStop(gpioStop, &ioMockStop);
  pinStop.setActiveHigh(false);

  Supla::Control::TrippleButtonRollerShutter rs(
      Supla::Io::IoPin(gpioUp, &ioMockUp),
      Supla::Io::IoPin(gpioDown, &ioMockDown),
      pinStop);

  ::testing::InSequence seq;

  EXPECT_CALL(ioMockStop, customDigitalWrite(0, gpioStop, HIGH));
  EXPECT_CALL(ioMockStop, customPinMode(0, gpioStop, OUTPUT));
  EXPECT_CALL(ioMockUp, customDigitalWrite(0, gpioUp, LOW));
  EXPECT_CALL(ioMockUp, customPinMode(0, gpioUp, OUTPUT));
  EXPECT_CALL(ioMockDown, customDigitalWrite(0, gpioDown, LOW));
  EXPECT_CALL(ioMockDown, customPinMode(0, gpioDown, OUTPUT));

  rs.onInit();
}

TEST_F(RollerShutterFixture, movementByServerTests) {
  StorageMock storage;
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  storage.defaultInitialization(9);
  EXPECT_CALL(storage, scheduleSave(_, 1000)).Times(AtLeast(0));

  // updates of section preamble
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  {
    ::testing::InSequence seq;

    EXPECT_CALL(storage,
                readStorage(_, _, /* sizeof(RollerShutterStateData) */ 9, _))
        .WillOnce([](uint32_t, unsigned char *data, int, bool) {
          RollerShutterStateDataTests rsData = {.closingTimeMs = 10000,
                                                .openingTimeMs = 10000,
                                                .currentPosition = 0};
          EXPECT_EQ(9, sizeof(rsData));
          memcpy(data, &rsData, sizeof(RollerShutterStateDataTests));
          return 9;
        });

    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

    // move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // move up - it first call stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // then actual move up:
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    // stop after reaching limit
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // sbs - stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move up
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // sbs - move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // move down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // move up
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // down
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));

    // stop
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));

    // move up
    EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
    EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));
  }

  Supla::Storage::LoadStateStorage();
  rs.onInit();

  for (int i = 0; i < 10; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  TCSD_RollerShutterValue *value = nullptr;
  TSD_SuplaChannelNewValue newValueFromServer = {};

  value = reinterpret_cast<TCSD_RollerShutterValue *>(newValueFromServer.value);

  newValueFromServer.DurationMS = (100 << 16) | 100;
  newValueFromServer.ChannelNumber = 0;
  value->position = 1;  // move down

  rs.handleNewValueFromServer(&newValueFromServer);
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 9);

  value->position = 2;  // up
  rs.handleNewValueFromServer(&newValueFromServer);
  // relays are disabled after 60s timeout
  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 0);

  value->position = 5;  // step by step -> move down
  rs.handleNewValueFromServer(&newValueFromServer);
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 9);

  rs.handleNewValueFromServer(&newValueFromServer);  // sbs - stop
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 10);

  rs.handleNewValueFromServer(&newValueFromServer);  // sbs - move up
  for (int i = 0; i < 700; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 0);

  rs.handleNewValueFromServer(&newValueFromServer);  // sbs - move down
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 10);

  value->position = 0;
  rs.handleNewValueFromServer(&newValueFromServer);  // stop
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 10);

  value->position = 3;                               // down or stop
  rs.handleNewValueFromServer(&newValueFromServer);  // down
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 19);

  rs.handleNewValueFromServer(&newValueFromServer);  // stop (down or stop)
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 20);

  rs.handleNewValueFromServer(&newValueFromServer);  // down (down or stop)
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 28);

  value->position = 4;                               // move up or stop
  rs.handleNewValueFromServer(&newValueFromServer);  // stop (up or stop)
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 30);

  rs.handleNewValueFromServer(&newValueFromServer);  // up (up or stop)
  for (int i = 0; i < 11; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 20);

  value->position = 1;                               // down
  rs.handleNewValueFromServer(&newValueFromServer);  // down
  for (int i = 0; i < 16; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 29);

  value->position = 2;                               // up
  rs.handleNewValueFromServer(&newValueFromServer);  // up
  for (int i = 0; i < 16; i++) {
    rs.onTimer();
    rs.iterateAlways();
    time.advance(100);
  }

  EXPECT_EQ(Supla::RegisterDevice::getChannelValuePtr(0)[0], 22);
}

TEST_F(RollerShutterFixture, onLoadStateClampsUnsafeTimes) {
  StorageMock storage;
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  storage.defaultInitialization(9);
  EXPECT_CALL(storage, scheduleSave(_, 1000)).Times(AtLeast(0));
  EXPECT_CALL(storage, writeStorage(8, _, 7)).WillRepeatedly(Return(7));
  EXPECT_CALL(storage, commit()).WillRepeatedly(Return());

  EXPECT_CALL(storage,
              readStorage(_, _, /* sizeof(RollerShutterStateData) */ 9, _))
      .WillOnce([](uint32_t, unsigned char *data, int, bool) {
        RollerShutterStateDataTests rsData = {
            .closingTimeMs = RS_MAX_OPERATION_TIME_MS + 1,
            .openingTimeMs = RS_MAX_OPERATION_TIME_MS + 1234,
            .currentPosition = 0};
        EXPECT_EQ(9, sizeof(rsData));
        memcpy(data, &rsData, sizeof(RollerShutterStateDataTests));
        return 9;
      });

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, pinMode(gpioUp, OUTPUT));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, pinMode(gpioDown, OUTPUT));

  Supla::Storage::LoadStateStorage();
  rs.onInit();

  EXPECT_EQ(rs.getClosingTimeMs(), RS_MAX_OPERATION_TIME_MS);
  EXPECT_EQ(rs.getOpeningTimeMs(), RS_MAX_OPERATION_TIME_MS);
}

TEST_F(RollerShutterFixture, applyChannelConfigAcceptsMaximumSafeTimes) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);
  TSD_ChannelConfig channelConfig = {};
  channelConfig.Func = SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER;
  channelConfig.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  channelConfig.ConfigSize = sizeof(TChannelConfig_RollerShutter);

  auto config =
      reinterpret_cast<TChannelConfig_RollerShutter *>(channelConfig.Config);
  config->ClosingTimeMS = RS_MAX_OPERATION_TIME_MS;
  config->OpeningTimeMS = RS_MAX_OPERATION_TIME_MS;
  config->MotorUpsideDown = 1;
  config->ButtonsUpsideDown = 1;
  config->TimeMargin = -1;

  EXPECT_EQ(rs.applyChannelConfig(&channelConfig),
            Supla::ApplyConfigResult::Success);
  EXPECT_EQ(rs.getClosingTimeMs(), RS_MAX_OPERATION_TIME_MS);
  EXPECT_EQ(rs.getOpeningTimeMs(), RS_MAX_OPERATION_TIME_MS);
}

TEST_F(RollerShutterFixture, applyChannelConfigClampsUnsafeTimes) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);
  rs.setOpenCloseTime(10000, 11000);

  TSD_ChannelConfig channelConfig = {};
  channelConfig.Func = SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER;
  channelConfig.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  channelConfig.ConfigSize = sizeof(TChannelConfig_RollerShutter);

  auto config =
      reinterpret_cast<TChannelConfig_RollerShutter *>(channelConfig.Config);
  config->ClosingTimeMS = RS_MAX_OPERATION_TIME_MS + 1;
  config->OpeningTimeMS = RS_MAX_OPERATION_TIME_MS + 1234;
  config->MotorUpsideDown = 1;
  config->ButtonsUpsideDown = 1;
  config->TimeMargin = -1;

  EXPECT_EQ(rs.applyChannelConfig(&channelConfig),
            Supla::ApplyConfigResult::Success);
  EXPECT_EQ(rs.getClosingTimeMs(), RS_MAX_OPERATION_TIME_MS);
  EXPECT_EQ(rs.getOpeningTimeMs(), RS_MAX_OPERATION_TIME_MS);
}

TEST_F(RollerShutterFixture,
       invalidFacadeBlindTiltTimingFallsBackToPlainFiniteMovement) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  Supla::Control::RollerShutter rs(gpioUp, gpioDown, true, true);
  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  rs.setOpenCloseTime(5000, 5000);
  rs.setTiltingTime(10000, false);
  rs.setTiltControlType(
      SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING, false);
  rs.setCurrentPosition(0, 0);

  EXPECT_FALSE(rs.isTiltConfigured());
  rs.onInit();
  rs.setTargetPosition(100, 100);

  for (int i = 0; i < 80; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(rs.getCurrentPosition(), 100);
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
}

TEST_F(RollerShutterFixture,
       invalidFacadeBlindTiltOnlyDirectTargetIsIgnored) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  Supla::Control::RollerShutter rs(gpioUp, gpioDown, true, true);
  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  rs.setOpenCloseTime(5000, 5000);
  rs.setTiltingTime(10000, false);
  rs.setTiltControlType(
      SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING, false);
  rs.setCurrentPosition(0, 0);
  rs.onInit();

  rs.setTargetPosition(UNKNOWN_POSITION, 100);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getTargetPosition(), STOP_POSITION);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);

  rs.setTiltingTime(1000, false);
  rs.setTiltControlType(SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
                        false);
  rs.setTargetPosition(UNKNOWN_POSITION, 101);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getTargetPosition(), STOP_POSITION);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
}

TEST_F(RollerShutterFixture,
       invalidInjectedTiltOnlyTargetIsCancelledBeforeRelayStarts) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  RollerShutterTestAccess rs(gpioUp, gpioDown, true, true);
  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  rs.setOpenCloseTime(5000, 5000);
  rs.setTiltingTime(1000, false);
  rs.setTiltControlType(SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
                        false);
  rs.setCurrentPosition(50, UNKNOWN_POSITION);
  rs.onInit();
  testing::Mock::VerifyAndClearExpectations(&ioMock);

  EXPECT_CALL(ioMock, digitalWrite(_, 1)).Times(0);
  EXPECT_CALL(ioMock, digitalWrite(_, 0)).Times(AtLeast(2));
  rs.injectTiltOnlyTarget(50);
  rs.onTimer();

  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getTargetPosition(), STOP_POSITION);
  EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
  EXPECT_EQ(rs.getOperationTimeoutMs(), 0);
}

TEST_F(RollerShutterFixture,
       standardRollerShutterIgnoresTiltOnlyTargetWhileStoppedAndMoving) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  RollerShutterTestAccess rs(gpioUp, gpioDown);
  rs.setOpenCloseTime(5000, 5000);
  rs.setCurrentPosition(50);
  rs.onInit();

  TSD_SuplaChannelNewValue newValue = {};
  newValue.value[0] = UNKNOWN_POSITION;
  newValue.value[1] = 60;
  rs.handleNewValueFromServer(&newValue);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getTargetPosition(), STOP_POSITION);

  rs.moveDown();
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::DOWN_DIR));
  const int targetBeforeInvalidCommand = rs.getTargetPosition();
  rs.handleNewValueFromServer(&newValue);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::DOWN_DIR));
  EXPECT_EQ(rs.getTargetPosition(), targetBeforeInvalidCommand);
  EXPECT_GT(rs.getOperationTimeoutMs(), 0);
}

TEST_F(RollerShutterFixture,
       tiltOnlyInputRequiresConfiguredTiltAndKnownState) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  TSD_SuplaChannelNewValue newValue = {};
  newValue.value[0] = UNKNOWN_POSITION;
  newValue.value[1] = 60;

  RollerShutterTestAccess rs(gpioUp, gpioDown, true, true);
  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  rs.setOpenCloseTime(5000, 5000);
  rs.setCurrentPosition(50, 50);
  rs.onInit();
  rs.handleNewValueFromServer(&newValue);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getTargetPosition(), STOP_POSITION);

  rs.moveDown();
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::DOWN_DIR));
  const int targetBeforeInvalidCommand = rs.getTargetPosition();
  rs.handleNewValueFromServer(&newValue);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::DOWN_DIR));
  EXPECT_EQ(rs.getTargetPosition(), targetBeforeInvalidCommand);
  rs.stop();
  rs.onTimer();

  rs.setTiltingTime(1000, false);
  rs.setTiltControlType(SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
                        false);
  rs.setCurrentPosition(50, UNKNOWN_POSITION);
  rs.handleNewValueFromServer(&newValue);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getTargetPosition(), STOP_POSITION);

  rs.setCurrentPosition(UNKNOWN_POSITION, 50);
  rs.handleNewValueFromServer(&newValue);
  rs.onTimer();
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
  EXPECT_EQ(rs.getTargetPosition(), STOP_POSITION);
}

TEST_F(RollerShutterFixture,
       validTiltOnlyMovementHasFiniteTimeoutForAllModesAndDirections) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  for (uint8_t type : {SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING,
                       SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
                       SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED}) {
    for (bool moveDown : {true, false}) {
      RollerShutterTestAccess rs(gpioUp, gpioDown, true, true);
      rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
      rs.setOpenCloseTime(5000, 5000);
      rs.setTiltingTime(
          type == SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING
              ? 10000
              : 1000,
          false);
      rs.setTiltControlType(type, false);
      const int startPosition = 50;
      const int startTilt = moveDown ? 0 : 100;
      const int targetTilt = moveDown ? 100 : 0;
      rs.setCurrentPosition(startPosition, startTilt);
      rs.onInit();

      TSD_SuplaChannelNewValue newValue = {};
      newValue.value[0] = UNKNOWN_POSITION;
      newValue.value[1] = targetTilt + 10;
      rs.handleNewValueFromServer(&newValue);
      rs.onTimer();

      EXPECT_EQ(rs.getCurrentDirection(),
                static_cast<int>(moveDown
                                     ? Supla::Control::Directions::DOWN_DIR
                                     : Supla::Control::Directions::UP_DIR));
      const uint32_t expectedBaseTimeout =
          type == SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING
              ? 10000
              : 5000;
      EXPECT_EQ(rs.getOperationTimeoutMs(),
                expectedBaseTimeout + expectedBaseTimeout * 0.3);

      for (int i = 0; i < 150 &&
                          rs.getCurrentDirection() !=
                              static_cast<int>(
                                  Supla::Control::Directions::STOP_DIR);
           i++) {
        time.advance(100);
        rs.onTimer();
      }

      EXPECT_EQ(rs.getCurrentTilt(), targetTilt);
      EXPECT_EQ(rs.getCurrentDirection(),
                static_cast<int>(Supla::Control::Directions::STOP_DIR));
      EXPECT_EQ(rs.getOperationTimeoutMs(), 0);
      if (type ==
          SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED) {
        EXPECT_EQ(rs.getCurrentPosition(), moveDown ? 100 : 0);
      }
    }
  }
}

TEST_F(RollerShutterFixture,
       disabledFacadeBlindTiltKeepsPositionAtEndpointsDuringMargin) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  RollerShutterTestAccess rs(gpioUp, gpioDown, true, true);
  rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  rs.setOpenCloseTime(5000, 5000);
  rs.setTiltingTime(0, false);
  rs.setTiltControlType(SUPLA_TILT_CONTROL_TYPE_UNKNOWN, false);
  rs.setCurrentPosition(100, UNKNOWN_POSITION);
  rs.onInit();
  rs.open();

  for (int i = 0; i < 80; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(rs.getCurrentPosition(), 0);
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.close();
  for (int i = 0; i < 80; i++) {
    rs.onTimer();
    time.advance(100);
  }

  EXPECT_EQ(rs.getCurrentPosition(), 100);
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));
}
