// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <arduino_mock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <storage_mock.h>
#include <supla/actions.h>
#include <supla/channel.h>
#include <supla/control/button.h>
#include <supla/control/bistable_roller_shutter.h>
#include <supla/control/roller_shutter.h>
#include <supla/control/tripple_button_roller_shutter.h>
#include <supla/device/register_device.h>
#include <supla_io_mock.h>
#include <array>

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

class StatefulDigitalInterface : public DigitalInterface {
 public:
  void digitalWrite(uint8_t pin, uint8_t value) override {
    levels[pin] = value;
  }

  int digitalRead(uint8_t pin) override {
    return levels[pin];
  }

  void analogWrite(uint8_t, int) override {
  }

  void pinMode(uint8_t, uint8_t) override {
  }

  unsigned int pulseIn(uint8_t, uint8_t, uint64_t) override {
    return 0;
  }

  void setInput(uint8_t pin, uint8_t value) {
    levels[pin] = value;
  }

  uint8_t level(uint8_t pin) const {
    return levels[pin];
  }

 private:
  std::array<uint8_t, 256> levels{};
};

class BistableRollerShutterRig {
 public:
  static constexpr uint8_t kUpOutput = 1;
  static constexpr uint8_t kDownOutput = 2;
  static constexpr uint8_t kUpButtonInput = 3;
  static constexpr uint8_t kDownButtonInput = 4;

  StatefulDigitalInterface gpio;
  SimpleTime time;
  Supla::Control::RollerShutter rollerShutter;
  Supla::Control::Button upButton;
  Supla::Control::Button downButton;

  BistableRollerShutterRig()
      : rollerShutter(kUpOutput, kDownOutput),
        upButton(kUpButtonInput),
        downButton(kDownButtonInput) {
    upButton.setButtonType(Supla::Control::Button::ButtonType::BISTABLE);
    downButton.setButtonType(Supla::Control::Button::ButtonType::BISTABLE);
    upButton.setDebounceDelay(0);
    downButton.setDebounceDelay(0);
    upButton.setSwNoiseFilterDelay(0);
    downButton.setSwNoiseFilterDelay(0);

    gpio.setInput(kUpButtonInput, LOW);
    gpio.setInput(kDownButtonInput, LOW);
    rollerShutter.attach(&upButton, &downButton);
    rollerShutter.onInit();
  }

  void setButton(Supla::Control::Button &button,
                 uint8_t inputPin,
                 bool pressed) {
    gpio.setInput(inputPin, pressed ? HIGH : LOW);

    // Advance time so the button state machine observes the edge.
    time.advance(1);
    button.onTimer();
    time.advance(1);
    button.onTimer();
    rollerShutter.onTimer();
  }

  void advanceRollerShutter(uint32_t milliseconds) {
    time.advance(milliseconds);
    rollerShutter.onTimer();
  }

  void expectStopped() const {
    EXPECT_EQ(gpio.level(kUpOutput), LOW);
    EXPECT_EQ(gpio.level(kDownOutput), LOW);
  }

  void expectMovingUp() const {
    EXPECT_EQ(gpio.level(kUpOutput), HIGH);
    EXPECT_EQ(gpio.level(kDownOutput), LOW);
  }

  void expectMovingDown() const {
    EXPECT_EQ(gpio.level(kUpOutput), LOW);
    EXPECT_EQ(gpio.level(kDownOutput), HIGH);
  }
};

class BistableRollerShutterFixture : public testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }
};

TEST_F(BistableRollerShutterFixture,
       PressStartsMovementAndReleaseStopsMovement) {
  BistableRollerShutterRig rig;

  rig.expectStopped();

  rig.setButton(rig.downButton, BistableRollerShutterRig::kDownButtonInput,
                true);
  rig.expectMovingDown();

  rig.setButton(rig.downButton, BistableRollerShutterRig::kDownButtonInput,
                false);
  rig.expectStopped();
  rig.advanceRollerShutter(501);
  rig.expectStopped();
}

TEST_F(BistableRollerShutterFixture,
       PressingUpStartsMovementAndReleaseStopsMovement) {
  BistableRollerShutterRig rig;

  rig.expectStopped();

  rig.setButton(rig.upButton, BistableRollerShutterRig::kUpButtonInput, true);
  rig.expectMovingUp();

  rig.setButton(rig.upButton, BistableRollerShutterRig::kUpButtonInput, false);
  rig.expectStopped();
  rig.advanceRollerShutter(501);
  rig.expectStopped();
}

TEST_F(BistableRollerShutterFixture,
       PressingDownWhileMovingDownKeepsDirectionUntilRelease) {
  BistableRollerShutterRig rig;

  // Model an already running motor (for example after a remote command),
  // then operate the bistable DOWN input.
  rig.rollerShutter.moveDown();
  rig.rollerShutter.onTimer();
  rig.expectMovingDown();

  rig.setButton(rig.downButton, BistableRollerShutterRig::kDownButtonInput,
                true);
  rig.expectMovingDown();

  rig.setButton(rig.downButton, BistableRollerShutterRig::kDownButtonInput,
                false);
  rig.expectStopped();
  rig.advanceRollerShutter(501);
  rig.expectStopped();
}

TEST_F(BistableRollerShutterFixture,
       PressingUpWhileMovingDownReversesAndStaleDownReleaseIsIgnored) {
  BistableRollerShutterRig rig;

  // Model an already running motor and keep DOWN pressed before pressing UP.
  rig.rollerShutter.moveDown();
  rig.rollerShutter.onTimer();
  rig.expectMovingDown();

  rig.setButton(rig.downButton, BistableRollerShutterRig::kDownButtonInput,
                true);
  rig.expectMovingDown();

  rig.setButton(rig.upButton, BistableRollerShutterRig::kUpButtonInput, true);
  rig.expectStopped();

  // The reverse direction is protected by the motor interlock.
  rig.advanceRollerShutter(501);
  rig.expectMovingUp();

  // Releasing the old DOWN button must not stop the newer UP command.
  rig.setButton(rig.downButton, BistableRollerShutterRig::kDownButtonInput,
                false);
  rig.expectMovingUp();

  rig.setButton(rig.upButton, BistableRollerShutterRig::kUpButtonInput, false);
  rig.expectStopped();
  rig.advanceRollerShutter(501);
  rig.expectStopped();
}

TEST_F(BistableRollerShutterFixture,
       ReleasingReverseButtonBeforeInterlockCancelsPendingMovement) {
  BistableRollerShutterRig rig;

  rig.rollerShutter.moveDown();
  rig.rollerShutter.onTimer();
  rig.expectMovingDown();

  rig.setButton(rig.upButton, BistableRollerShutterRig::kUpButtonInput, true);
  rig.expectStopped();

  // Releasing UP before the interlock expires cancels the pending reverse.
  rig.setButton(rig.upButton, BistableRollerShutterRig::kUpButtonInput, false);
  rig.advanceRollerShutter(501);
  rig.expectStopped();
}

TEST_F(BistableRollerShutterFixture,
       InvertedButtonsStopTheMappedDirectionOnRelease) {
  BistableRollerShutterRig rig;

  rig.rollerShutter.setRsConfigButtonsUpsideDownValue(2);

  rig.setButton(rig.upButton, BistableRollerShutterRig::kUpButtonInput, true);
  rig.expectMovingDown();

  rig.setButton(rig.upButton, BistableRollerShutterRig::kUpButtonInput, false);
  rig.expectStopped();
}

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
       unknownStandardStateRestoresWithoutCalibrationRequestAndOpenCloseWork) {
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
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  rs.onTimer();
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::STOP_DIR));

  TSD_SuplaChannelNewValue newValue = {};
  newValue.DurationMS = (100 << 16) | 100;
  newValue.value[0] = 10;  // absolute OPEN target

  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 1));

  rs.handleNewValueFromServer(&newValue);
  rs.onTimer();

  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::UP_DIR));
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_FALSE(rs.isCalibrationInProgress());

  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 0));
  EXPECT_CALL(storage, scheduleSave(5000, 1000));
  rs.stop();
  rs.onTimer();
  time.advance(501);

  newValue.value[0] = 110;  // absolute CLOSE target
  EXPECT_CALL(ioMock, digitalWrite(gpioUp, 0));
  EXPECT_CALL(ioMock, digitalWrite(gpioDown, 1));
  rs.handleNewValueFromServer(&newValue);
  rs.onTimer();

  EXPECT_EQ(rs.getCurrentDirection(),
            static_cast<int>(Supla::Control::Directions::DOWN_DIR));
  EXPECT_FALSE(rs.isCalibrationRequested());
  EXPECT_FALSE(rs.isCalibrationInProgress());
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

TEST_F(RollerShutterFixture, triggerCalibrationSetsCalibrationRequest) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);

  rs.setOpenCloseTime(10000, 10000);
  rs.setCalibrationFinished();
  ASSERT_FALSE(rs.isCalibrationRequested());

  rs.triggerCalibration();

  EXPECT_EQ(rs.getCurrentPosition(), UNKNOWN_POSITION);
  EXPECT_FALSE(rs.isCalibrated());
  EXPECT_TRUE(rs.isCalibrationRequested());
}

TEST_F(RollerShutterFixture,
       calcfgRecalibrateRequiresAuthorizationAndRequestsCalibration) {
  Supla::Control::RollerShutter rs(gpioUp, gpioDown);
  TSD_DeviceCalCfgRequest request = {};
  request.Command = SUPLA_CALCFG_CMD_RECALIBRATE;

  rs.setOpenCloseTime(10000, 10000);
  rs.setCalibrationFinished();
  ASSERT_FALSE(rs.isCalibrationRequested());

  request.SuperUserAuthorized = false;
  EXPECT_EQ(rs.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_UNAUTHORIZED);
  EXPECT_FALSE(rs.isCalibrationRequested());

  request.SuperUserAuthorized = true;
  EXPECT_EQ(rs.handleCalcfgFromServer(&request), SUPLA_CALCFG_RESULT_DONE);
  EXPECT_TRUE(rs.isCalibrationRequested());
}

TEST_F(RollerShutterFixture,
       unknownTiltStateRestoresWithoutRequestAndClearsRestoredTilt) {
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
  EXPECT_FALSE(rs.isCalibrationRequested());
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

TEST_F(RollerShutterFixture, directionalMovementAtEndpointUsesOnlyMargin) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  for (bool down : {false, true}) {
    // Config values encode 0% as 1 and 10% as 11; -1 is the default.
    for (int margin : {-1, 1, 11}) {
      SCOPED_TRACE(testing::Message()
                   << "down=" << down << " margin=" << margin);
      RollerShutterTestAccess rs(gpioUp, gpioDown);
      rs.setOpenCloseTime(20000, 10000);
      rs.setRsConfigTimeMarginValue(margin);
      rs.setCurrentPosition(down ? 100 : 0);
      rs.onInit();
      const uint32_t fullTime = down ? 20000 : 10000;
      const uint32_t expectedMargin =
          margin == -1 ? fullTime * 3 / 10 : margin == 1 ? 50 : fullTime / 10;
      const auto direction = down ? Supla::Control::Directions::DOWN_DIR
                                  : Supla::Control::Directions::UP_DIR;

      // Repeated clicks after stopping must each allow a corrective run.
      for (int repeat = 0; repeat < 2; ++repeat) {
        time.advance(501);
        rs.handleAction(0, down ? Supla::MOVE_DOWN_OR_STOP
                               : Supla::MOVE_UP_OR_STOP);
        rs.onTimer();
        rs.onTimer();  // Observe the endpoint and start the margin.
        ASSERT_EQ(rs.getOperationTimeoutMs(), expectedMargin);
        ASSERT_EQ(rs.getTargetPosition(), UNKNOWN_POSITION);
        time.advance(expectedMargin - 1);
        rs.onTimer();
        EXPECT_EQ(rs.getCurrentDirection(), static_cast<int>(direction));
        // The margin must not restart on every timer iteration.
        time.advance(2);
        rs.onTimer();
        rs.onTimer();
        EXPECT_EQ(rs.getCurrentDirection(),
                  static_cast<int>(Supla::Control::Directions::STOP_DIR));
      }
    }
  }
}

TEST_F(RollerShutterFixture, directionalMovementStartsMarginAtEndpoint) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  for (bool down : {false, true}) {
    RollerShutterTestAccess rs(gpioUp, gpioDown);
    rs.setOpenCloseTime(20000, 10000);
    rs.setCurrentPosition(50);
    rs.onInit();
    rs.handleAction(0, down ? Supla::MOVE_DOWN : Supla::MOVE_UP);
    rs.onTimer();
    const uint32_t fullTime = down ? 20000 : 10000;
    time.advance(fullTime / 2 - 1);
    rs.onTimer();
    EXPECT_EQ(rs.getTargetPosition(),
              down ? MOVE_DOWN_POSITION : MOVE_UP_POSITION);
    time.advance(1);
    rs.onTimer();
    EXPECT_EQ(rs.getCurrentPosition(), down ? 100 : 0);
    EXPECT_EQ(rs.getTargetPosition(), UNKNOWN_POSITION);
    EXPECT_EQ(rs.getOperationTimeoutMs(), fullTime * 3 / 10);
    time.advance(fullTime * 3 / 10 + 1);
    rs.onTimer();
    rs.onTimer();
    EXPECT_EQ(rs.getCurrentDirection(),
              static_cast<int>(Supla::Control::Directions::STOP_DIR));
  }
}

TEST_F(RollerShutterFixture,
       directionalMovementWaitsForEndpointTiltBeforeMargin) {
  EXPECT_CALL(ioMock, digitalWrite(_, _)).Times(AtLeast(0));
  EXPECT_CALL(ioMock, pinMode(_, _)).Times(AtLeast(0));

  for (uint8_t type : {SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING,
                       SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING,
                       SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED}) {
    for (bool down : {false, true}) {
      SCOPED_TRACE(testing::Message() << "type=" << +type << " down=" << down);
      RollerShutterTestAccess rs(gpioUp, gpioDown, true, true);
      rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
      rs.setOpenCloseTime(20000, 10000);
      rs.setTiltingTime(1000, false);
      rs.setTiltControlType(type, false);
      // Closing must still finish tilting when position already reads 100%.
      // For opening, tilt only at fully closed requires starting at 100%.
      const bool tiltOnlyAtBottom =
          type == SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED;
      rs.setCurrentPosition(down || tiltOnlyAtBottom ? 100 : 0, 50);
      rs.onInit();
      rs.handleAction(0, down ? Supla::MOVE_DOWN : Supla::MOVE_UP);
      rs.onTimer();
      time.advance(499);
      rs.onTimer();
      EXPECT_EQ(rs.getTargetPosition(),
                down ? MOVE_DOWN_POSITION : MOVE_UP_POSITION);
      for (int i = 0; i < 2100 &&
                          rs.getTargetPosition() != UNKNOWN_POSITION; ++i) {
        time.advance(10);
        rs.onTimer();
      }
      ASSERT_EQ(rs.getTargetPosition(), UNKNOWN_POSITION);
      EXPECT_EQ(rs.getCurrentPosition(), down ? 100 : 0);
      EXPECT_EQ(rs.getCurrentTilt(), down ? 100 : 0);
      const uint32_t expectedMargin = down ? 6000 : 3000;
      EXPECT_EQ(rs.getOperationTimeoutMs(), expectedMargin);
      time.advance(expectedMargin + 1);
      rs.onTimer();
      rs.onTimer();
      EXPECT_EQ(rs.getCurrentDirection(),
                static_cast<int>(Supla::Control::Directions::STOP_DIR));
    }
  }
}

TEST_F(RollerShutterFixture,
       attachedMonostableButtonsKeepClickAndHoldBehavior) {
  StatefulDigitalInterface gpio;
  for (bool down : {false, true}) {
    gpio.setInput(3, LOW);
    Supla::Control::Button button(3);
    button.setDebounceDelay(0);
    button.setSwNoiseFilterDelay(0);
    button.setHoldTime(500);
    button.setMulticlickTime(100);
    RollerShutterTestAccess rs(gpioUp, gpioDown, true, true);
    rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
    rs.setOpenCloseTime(20000, 10000);
    rs.setTiltingTime(1000, false);
    rs.setTiltControlType(
        SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING, false);
    rs.setCurrentPosition(down ? 100 : 0, down ? 100 : 0);
    rs.attach(&button, !down, true);
    rs.onInit();
    const int output = down ? gpioDown : gpioUp;
    auto advance = [&](int duration) {
      for (int elapsed = 0; elapsed < duration; ++elapsed) {
        time.advance(1);
        button.onTimer();
        rs.onTimer();
      }
    };
    advance(100);
    gpio.setInput(3, HIGH);
    advance(100);
    EXPECT_EQ(gpio.level(output), LOW);
    gpio.setInput(3, LOW);
    advance(250);
    EXPECT_EQ(gpio.level(output), HIGH);
    EXPECT_EQ(rs.getOperationTimeoutMs(), down ? 6000 : 3000);

    // A second short click stops the corrective movement.
    gpio.setInput(3, HIGH);
    advance(100);
    gpio.setInput(3, LOW);
    advance(250);
    EXPECT_EQ(gpio.level(output), LOW);

    advance(501);
    rs.setCurrentPosition(50, 50);
    gpio.setInput(3, HIGH);
    advance(600);
    EXPECT_EQ(gpio.level(output), HIGH);
    EXPECT_NE(rs.getCurrentTilt(), 50);
    gpio.setInput(3, LOW);
    advance(10);
    EXPECT_EQ(gpio.level(output), LOW);
    advance(250);
    EXPECT_EQ(gpio.level(output), LOW);  // No click after releasing a hold.
  }
}

TEST_F(RollerShutterFixture,
       invalidTiltDirectionalMovementKeepsMarginAndCorrectiveRuns) {
  StatefulDigitalInterface gpio;
  for (bool down : {false, true}) {
    for (int margin : {-1, 1, 11}) {
      SCOPED_TRACE(testing::Message()
                   << "down=" << down << " margin=" << margin);
      RollerShutterTestAccess rs(gpioUp, gpioDown, true, true);
      rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
      rs.setOpenCloseTime(20000, 10000);
      rs.setTiltingTime(20000, false);
      rs.setTiltControlType(
          SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING, false);
      rs.setRsConfigTimeMarginValue(margin);
      rs.setCurrentPosition(50, 50);
      rs.onInit();
      ASSERT_FALSE(rs.isTiltConfigured());
      const int output = down ? gpioDown : gpioUp;
      const uint32_t fullTime = down ? 20000 : 10000;
      const uint32_t expectedMargin =
          margin == -1 ? fullTime * 3 / 10 : margin == 1 ? 50 : fullTime / 10;

      // First establish fallback during travel, then reuse its retained flag
      // for a corrective run starting at the endpoint.
      for (int run = 0; run < 2; ++run) {
        time.advance(501);
        rs.handleAction(0, down ? Supla::MOVE_DOWN : Supla::MOVE_UP);
        rs.onTimer();
        if (run == 0) {
          time.advance(fullTime / 2 - 1);
          rs.onTimer();
          EXPECT_EQ(rs.getTargetPosition(),
                    down ? MOVE_DOWN_POSITION : MOVE_UP_POSITION);
          time.advance(1);
        }
        rs.onTimer();
        EXPECT_EQ(rs.getCurrentPosition(), down ? 100 : 0);
        ASSERT_EQ(gpio.level(output), HIGH);
        ASSERT_EQ(rs.getOperationTimeoutMs(), expectedMargin);
        EXPECT_EQ(rs.getTargetPosition(), UNKNOWN_POSITION);
        EXPECT_EQ(rs.getTargetTilt(), UNKNOWN_POSITION);
        time.advance(expectedMargin - 1);
        rs.onTimer();
        EXPECT_EQ(gpio.level(output), HIGH);
        time.advance(2);
        rs.onTimer();
        rs.onTimer();
        EXPECT_EQ(gpio.level(gpioUp), LOW);
        EXPECT_EQ(gpio.level(gpioDown), LOW);
      }
    }
  }
}

TEST_F(RollerShutterFixture,
       invalidTiltAbsoluteMovementStillStopsWithoutMargin) {
  StatefulDigitalInterface gpio;
  for (bool down : {false, true}) {
    RollerShutterTestAccess rs(gpioUp, gpioDown, true, true);
    rs.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
    rs.setOpenCloseTime(20000, 10000);
    rs.setTiltingTime(20000, false);
    rs.setTiltControlType(
        SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING, false);
    rs.setCurrentPosition(50, 50);
    rs.onInit();
    ASSERT_FALSE(rs.isTiltConfigured());
    rs.handleAction(0, down ? Supla::CLOSE : Supla::OPEN);
    rs.onTimer();
    time.advance((down ? 20000 : 10000) / 2 - 1);
    rs.onTimer();
    EXPECT_EQ(gpio.level(down ? gpioDown : gpioUp), HIGH);
    time.advance(1);
    rs.onTimer();
    EXPECT_EQ(rs.getCurrentPosition(), down ? 100 : 0);
    EXPECT_EQ(rs.getOperationTimeoutMs(), 0);
    EXPECT_EQ(gpio.level(gpioUp), LOW);
    EXPECT_EQ(gpio.level(gpioDown), LOW);
  }
}
