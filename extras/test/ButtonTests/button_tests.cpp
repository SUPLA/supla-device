// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <arduino_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/io.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <supla/control/sequence_button.h>
#include <supla/storage/config_tags.h>
#include <config_simulator.h>
#include "supla/events.h"

using ::testing::Return;

class ActionHandlerMock : public Supla::ActionHandler {
 public:
  MOCK_METHOD(void, handleAction, (int, int), (override));
};

class ButtonTestDouble : public Supla::Control::Button {
 public:
  using Supla::Control::Button::Button;

  void setActionTriggerModeLockedForTest(bool locked) {
    setActionTriggerModeLocked(locked);
  }

  void dispatchActionForTest(uint16_t event) {
    runActionWithActionTriggerPolicy(event);
  }

  void setConfigButtonForTest(bool enabled) {
    configButton = enabled;
  }

  uint16_t holdTimeMsForTest() const {
    return holdTimeMs;
  }

  uint16_t multiclickTimeMsForTest() const {
    return multiclickTimeMs;
  }
};

TEST(ButtonTests, GestureStartedWhileLockedIsIgnoredUntilRelease) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock;
  int pinState = 0;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillRepeatedly([&pinState](int) { return pinState; });
  EXPECT_CALL(mock, handleAction).Times(0);

  ButtonTestDouble button(5, false, false);
  button.onInit();
  button.setHoldTime(200);
  button.addAction(1, mock, Supla::ON_PRESS);
  button.addAction(2, mock, Supla::ON_CHANGE);
  button.addAction(3, mock, Supla::ON_RELEASE);
  button.addAction(4, mock, Supla::ON_HOLD);

  button.setActionTriggerModeLockedForTest(true);
  pinState = 1;
  time.advance(1000);
  button.onTimer();  // new state candidate while locked
  time.advance(30);
  button.onTimer();  // transition is debounced, but action is suppressed

  button.setActionTriggerModeLockedForTest(false);
  time.advance(300);
  button.onTimer();  // held button does not generate a delayed hold

  pinState = 0;
  time.advance(100);
  button.onTimer();
  time.advance(30);
  button.onTimer();  // release ending the locked gesture is also suppressed

  testing::Mock::VerifyAndClearExpectations(&mock);
  EXPECT_CALL(mock, handleAction(Supla::ON_PRESS, 1));
  EXPECT_CALL(mock, handleAction(Supla::ON_CHANGE, 2));

  pinState = 1;
  time.advance(100);
  button.onTimer();
  time.advance(30);
  button.onTimer();  // a new gesture after release works normally
}

TEST(ButtonTests, UnlockBeforeDebounceDoesNotReleasePendingPress) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock;
  int pinState = 0;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillRepeatedly([&pinState](int) { return pinState; });
  EXPECT_CALL(mock, handleAction).Times(0);

  ButtonTestDouble button(5, false, false);
  button.onInit();
  button.addAction(1, mock, Supla::ON_PRESS);
  button.addAction(2, mock, Supla::ON_CHANGE);
  button.addAction(3, mock, Supla::ON_RELEASE);

  button.setActionTriggerModeLockedForTest(true);
  pinState = 1;
  time.advance(1000);
  button.onTimer();  // pending press starts while locked
  button.setActionTriggerModeLockedForTest(false);
  time.advance(30);
  button.onTimer();  // debounce completes after unlock

  pinState = 0;
  time.advance(100);
  button.onTimer();
  time.advance(30);
  button.onTimer();
}

TEST(ButtonTests, LockedButtonAllowsConfiguredHoldToggleExactlyOnce) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock output;
  Supla::Control::ActionTrigger actionTrigger;
  int pinState = 0;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillRepeatedly([&pinState](int) { return pinState; });
  EXPECT_CALL(output, handleAction).Times(0);

  Supla::Control::Button button(5, false, false);
  actionTrigger.setLocalUnlockAllowed(true);
  actionTrigger.attach(button);
  button.setHoldTime(200);
  button.onInit();
  // Register the ordinary action before the lock action to verify that the
  // result does not depend on handler registration order.
  button.addAction(100, output, Supla::ON_HOLD, true);
  button.addAction(Supla::TOGGLE_LOCK, actionTrigger, Supla::ON_HOLD);
  actionTrigger.onInit();

  pinState = 1;
  time.advance(1000);
  button.onTimer();
  time.advance(30);
  button.onTimer();
  time.advance(300);
  button.onTimer();
  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);

  // Repeated HOLD and release of the locking gesture do not toggle again.
  time.advance(300);
  button.onTimer();
  pinState = 0;
  time.advance(100);
  button.onTimer();
  time.advance(30);
  button.onTimer();
  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);

  // A new HOLD unlocks once; its release and repeated HOLD remain inert.
  pinState = 1;
  time.advance(1000);
  button.onTimer();
  time.advance(30);
  button.onTimer();
  time.advance(300);
  button.onTimer();
  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_NOT_SET);
  time.advance(300);
  button.onTimer();
  pinState = 0;
  time.advance(100);
  button.onTimer();
  time.advance(30);
  button.onTimer();
  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_NOT_SET);
}

TEST(ButtonTests, LockedButtonRejectsLocalUnlockWhenDisabled) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock output;
  Supla::Control::ActionTrigger actionTrigger;
  int pinState = 0;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillRepeatedly([&pinState](int) { return pinState; });
  EXPECT_CALL(output, handleAction).Times(0);

  Supla::Control::Button button(5, false, false);
  actionTrigger.attach(button);
  actionTrigger.handleAction(Supla::ON_HOLD, Supla::LOCK);
  button.setHoldTime(200);
  button.onInit();
  button.addAction(100, output, Supla::ON_HOLD, true);
  button.addAction(Supla::UNLOCK, actionTrigger, Supla::ON_HOLD);

  pinState = 1;
  time.advance(1000);
  button.onTimer();
  time.advance(30);
  button.onTimer();
  time.advance(300);
  button.onTimer();

  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);
}

TEST(ButtonTests, ConflictingLockActionsHaveDeterministicPriority) {
  SimpleTime time;
  ButtonTestDouble button(-1);
  Supla::Control::ActionTrigger actionTrigger;
  actionTrigger.setLocalUnlockAllowed(true);
  actionTrigger.attach(button);

  // Register in the reverse order of the priority used by Button.
  button.addAction(Supla::UNLOCK, actionTrigger, Supla::ON_CLICK_5);
  button.addAction(Supla::LOCK, actionTrigger, Supla::ON_CLICK_5);
  button.dispatchActionForTest(Supla::ON_CLICK_5);
  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);

  // Start a separate gesture before checking the locked-state priority.
  button.setActionTriggerModeLockedForTest(false);
  actionTrigger.handleAction(Supla::ON_HOLD, Supla::LOCK);
  button.dispatchActionForTest(Supla::ON_CLICK_5);
  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_NOT_SET);
}

TEST(ButtonTests, DuplicateToggleBindingsChangeLockOnce) {
  SimpleTime time;
  ButtonTestDouble button(-1);
  Supla::Control::ActionTrigger actionTrigger;
  actionTrigger.setLocalUnlockAllowed(true);
  actionTrigger.attach(button);

  button.addAction(Supla::TOGGLE_LOCK, actionTrigger, Supla::ON_CLICK_1);
  button.addAction(Supla::TOGGLE_LOCK, actionTrigger, Supla::ON_CLICK_1);
  button.dispatchActionForTest(Supla::ON_CLICK_1);

  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);
}

TEST(ButtonTests, ExplicitConfigButtonCapabilityKeepsOnlyConfigActions) {
  SimpleTime time;
  ButtonTestDouble button(-1);
  Supla::Control::ActionTrigger actionTrigger;
  ActionHandlerMock configAction;
  ActionHandlerMock ordinaryAction;
  actionTrigger.setKeepConfigButtonTriggerAlwaysAvailable(true);
  actionTrigger.attach(button);
  actionTrigger.handleAction(Supla::ON_HOLD, Supla::LOCK);
  button.setConfigButtonForTest(true);
  button.addAction(Supla::ENTER_CONFIG_MODE_OR_RESET_TO_FACTORY,
                   configAction,
                   Supla::ON_HOLD,
                   true);
  button.addAction(123, ordinaryAction, Supla::ON_HOLD, true);

  EXPECT_CALL(configAction,
              handleAction(Supla::ON_HOLD,
                           Supla::ENTER_CONFIG_MODE_OR_RESET_TO_FACTORY));
  EXPECT_CALL(ordinaryAction, handleAction).Times(0);
  button.dispatchActionForTest(Supla::ON_HOLD);
}

TEST(ButtonTests, ConfigCapabilityDoesNotPermitAnotherAtToUnlock) {
  SimpleTime time;
  ButtonTestDouble button(-1);
  Supla::Control::ActionTrigger lockedAt;
  Supla::Control::ActionTrigger otherAt;
  lockedAt.setKeepConfigButtonTriggerAlwaysAvailable(true);
  lockedAt.attach(button);
  otherAt.setLocalUnlockAllowed(true);
  otherAt.handleAction(Supla::ON_HOLD, Supla::LOCK);
  lockedAt.handleAction(Supla::ON_HOLD, Supla::LOCK);
  button.setConfigButtonForTest(true);
  button.addAction(Supla::UNLOCK, otherAt, Supla::ON_HOLD);

  button.dispatchActionForTest(Supla::ON_HOLD);

  EXPECT_EQ(static_cast<Supla::AtChannel *>(lockedAt.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);
  EXPECT_EQ(static_cast<Supla::AtChannel *>(otherAt.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);
}

TEST(ButtonTests, LockTransitionStopsRemainingEventsOfOnePress) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock output;
  Supla::Control::ActionTrigger actionTrigger;
  int pinState = 0;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillRepeatedly([&pinState](int) { return pinState; });
  EXPECT_CALL(output, handleAction).Times(0);

  Supla::Control::Button button(5, false, false);
  actionTrigger.setLocalUnlockAllowed(true);
  actionTrigger.attach(button);
  actionTrigger.handleAction(Supla::ON_HOLD, Supla::LOCK);
  button.onInit();
  button.addAction(Supla::UNLOCK, actionTrigger, Supla::ON_PRESS);
  button.addAction(123, output, Supla::ON_CHANGE, true);

  pinState = 1;
  time.advance(1000);
  button.onTimer();
  time.advance(30);
  button.onTimer();

  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_NOT_SET);
}

TEST(ButtonTests, UnlockPermissionChangeDuringPressWaitsForNewGesture) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  Supla::Control::ActionTrigger actionTrigger;
  int pinState = 0;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillRepeatedly([&pinState](int) { return pinState; });

  Supla::Control::Button button(5, false, false);
  actionTrigger.attach(button);
  actionTrigger.handleAction(Supla::ON_HOLD, Supla::LOCK);
  button.setHoldTime(200);
  button.onInit();
  button.addAction(Supla::UNLOCK, actionTrigger, Supla::ON_HOLD);

  pinState = 1;
  time.advance(1000);
  button.onTimer();
  time.advance(30);
  button.onTimer();
  actionTrigger.setLocalUnlockAllowed(true);
  time.advance(300);
  button.onTimer();
  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);

  pinState = 0;
  time.advance(100);
  button.onTimer();
  time.advance(30);
  button.onTimer();

  pinState = 1;
  time.advance(100);
  button.onTimer();
  time.advance(30);
  button.onTimer();
  time.advance(300);
  button.onTimer();
  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_NOT_SET);
}

TEST(ButtonTests, LockOnReleaseDoesNotLeakIntoFollowingClick) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock output;
  Supla::Control::ActionTrigger actionTrigger;
  int pinState = 0;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillRepeatedly([&pinState](int) { return pinState; });
  EXPECT_CALL(output, handleAction).Times(0);

  Supla::Control::Button button(5, false, false);
  actionTrigger.attach(button);
  button.setMulticlickTime(300);
  button.onInit();
  button.addAction(Supla::LOCK, actionTrigger, Supla::ON_RELEASE);
  button.addAction(200, output, Supla::ON_CLICK_1);

  pinState = 1;
  time.advance(1000);
  button.onTimer();
  time.advance(30);
  button.onTimer();
  pinState = 0;
  time.advance(100);
  button.onTimer();
  time.advance(30);
  button.onTimer();
  time.advance(500);
  button.onTimer();

  EXPECT_EQ(static_cast<Supla::AtChannel *>(actionTrigger.getChannel())
                ->getButtonMode(),
            SUPLA_BUTTON_MODE_LOCKED);
}

TEST(ButtonTests, SetMulticlickTimeClampsToPersistedConfigRange) {
  ButtonTestDouble button(-1);

  button.setMulticlickTime(0);
  EXPECT_EQ(button.multiclickTimeMsForTest(), 0);

  button.setMulticlickTime(100);
  EXPECT_EQ(button.multiclickTimeMsForTest(), 200);

  button.setMulticlickTime(200);
  EXPECT_EQ(button.multiclickTimeMsForTest(), 200);

  button.setMulticlickTime(10000);
  EXPECT_EQ(button.multiclickTimeMsForTest(), 10000);

  button.setMulticlickTime(20000);
  EXPECT_EQ(button.multiclickTimeMsForTest(), 10000);
}

TEST(ButtonTests, SetHoldTimeClampsToPersistedConfigRange) {
  ButtonTestDouble button(-1);

  button.setHoldTime(0);
  EXPECT_EQ(button.holdTimeMsForTest(), 0);

  button.setHoldTime(100);
  EXPECT_EQ(button.holdTimeMsForTest(), 200);

  button.setHoldTime(200);
  EXPECT_EQ(button.holdTimeMsForTest(), 200);

  button.setHoldTime(10000);
  EXPECT_EQ(button.holdTimeMsForTest(), 10000);

  button.setHoldTime(20000);
  EXPECT_EQ(button.holdTimeMsForTest(), 10000);
}

TEST(ButtonTests, PersistsClampedMulticlickDefaultWhenConfigIsMissing) {
  ConfigSimulator config;
  SimpleTime time;
  ButtonTestDouble button(-1);
  button.setButtonNumber(0);
  button.setMulticlickTime(20000);

  uint32_t storedValue = 0;
  EXPECT_FALSE(config.getUInt32(Supla::ConfigTag::BtnMulticlickTag,
                                &storedValue));

  button.onLoadConfig(nullptr);

  ASSERT_TRUE(config.getUInt32(Supla::ConfigTag::BtnMulticlickTag,
                               &storedValue));
  EXPECT_EQ(storedValue, 10000U);

  ButtonTestDouble rebootedButton(-1);
  rebootedButton.setButtonNumber(0);
  rebootedButton.onLoadConfig(nullptr);
  EXPECT_EQ(rebootedButton.multiclickTimeMsForTest(), 10000);
}

TEST(ButtonTests, PersistsClampedHoldDefaultWhenConfigIsMissing) {
  ConfigSimulator config;
  SimpleTime time;
  ButtonTestDouble button(-1);
  button.setButtonNumber(0);
  button.setHoldTime(20000);

  uint32_t storedValue = 0;
  EXPECT_FALSE(config.getUInt32(Supla::ConfigTag::BtnHoldTag, &storedValue));

  button.onLoadConfig(nullptr);

  ASSERT_TRUE(config.getUInt32(Supla::ConfigTag::BtnHoldTag, &storedValue));
  EXPECT_EQ(storedValue, 10000U);

  ButtonTestDouble rebootedButton(-1);
  rebootedButton.setButtonNumber(0);
  rebootedButton.onLoadConfig(nullptr);
  EXPECT_EQ(rebootedButton.holdTimeMsForTest(), 10000);
}

TEST(ButtonTests, OnLoadConfigPersistsDefaultMulticlickTime) {
  ConfigSimulator config;
  SimpleTime time;
  Supla::Control::Button button(-1);
  button.setButtonNumber(0);
  constexpr uint32_t defaultMulticlickTime = 750;
  button.setMulticlickTime(defaultMulticlickTime);

  uint32_t storedMulticlickTime = 0;
  EXPECT_FALSE(config.getUInt32(Supla::ConfigTag::BtnMulticlickTag,
                                &storedMulticlickTime));

  button.onLoadConfig(nullptr);

  ASSERT_TRUE(config.getUInt32(Supla::ConfigTag::BtnMulticlickTag,
                               &storedMulticlickTime));
  EXPECT_EQ(storedMulticlickTime, defaultMulticlickTime);
}

TEST(ButtonTests, IoPinConstructorUsesConfiguredPolarity) {
  DigitalInterfaceMock ioMock;

  Supla::Io::IoPin inputPin(5);
  inputPin.setPullUp(true);
  inputPin.setActiveHigh(false);

  EXPECT_CALL(ioMock, pinMode(5, INPUT_PULLUP));
  EXPECT_CALL(ioMock, digitalRead(5)).WillOnce(Return(1));

  Supla::Control::Button button(inputPin);
  button.onInit();

  EXPECT_EQ(button.getLastState(), Supla::Control::RELEASED);
}

TEST(ButtonTests, SequenceButtonIoPinConstructorUsesConfiguredPolarity) {
  DigitalInterfaceMock ioMock;

  Supla::Io::IoPin inputPin(6);
  inputPin.setPullUp(true);
  inputPin.setActiveHigh(false);

  EXPECT_CALL(ioMock, pinMode(6, INPUT_PULLUP));
  EXPECT_CALL(ioMock, digitalRead(6)).WillOnce(Return(1));

  Supla::Control::SequenceButton button(inputPin);
  button.onInit();

  EXPECT_EQ(button.getLastState(), Supla::Control::RELEASED);
}

TEST(ButtonTests, OnPressAndOnRelease) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 second read, should be ignored
      .WillOnce(Return(1))  // #4 third read, should trigger on_press
      .WillOnce(Return(1))  // #5 time 90
      .WillOnce(Return(0))  // #6 time 100
      .WillOnce(Return(0))  // #7 time 110
      .WillOnce(Return(0));  // #8 time 150

  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(1);

  // Conditional events are generated on each press/release beacuse button
  // is not configured to detect hold nor multiclick
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_CHANGE, 5)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 4)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 6)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.onInit();  // #1
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);
  button.addAction(4, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(5, mock1, Supla::CONDITIONAL_ON_CHANGE);
  button.addAction(6, mock1, Supla::CONDITIONAL_ON_RELEASE);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(10);
  button.onTimer();  // #3 within filtering time - nothing should happen
  time.advance(20);
  button.onTimer();  // #4 ON_PRESS
  time.advance(60);
  button.onTimer();  // #5
  time.advance(10);
  button.onTimer();  // #6 new state candidate
  time.advance(10);
  button.onTimer();  // #7
  time.advance(20);
  button.onTimer();  // #8 on release
  time.advance(30);
  button.onTimer();  // #
  time.advance(10);
  button.onTimer();
  time.advance(10);
  button.onTimer();
}

TEST(ButtonTests, OnHold) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3
      .WillOnce(Return(1))  // #4
      .WillOnce(Return(1))  // #5
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(0))  // #7
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0))  // #9
      // second round
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3
      .WillOnce(Return(1))  // #4
      .WillOnce(Return(1))  // #5
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(0))  // #7
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0));  // #9

  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(4);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_LONG_CLICK_0, 5)).Times(2);

  // Conditional on_press and on_change is executed twice, becuase we simulate
  // two independent button presses
  // Conditional on_release is not executed because on_hold was send
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_CHANGE, 10)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 9)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 11)).Times(0);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(500);
  button.setMulticlickTime(300);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(5, mock1, Supla::ON_LONG_CLICK_0);
  button.addAction(6, mock1, Supla::ON_LONG_CLICK_1);
  button.addAction(7, mock1, Supla::ON_LONG_CLICK_2);
  button.addAction(8, mock1, Supla::ON_LONG_CLICK_3);
  button.addAction(9, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(10, mock1, Supla::CONDITIONAL_ON_CHANGE);
  button.addAction(11, mock1, Supla::CONDITIONAL_ON_RELEASE);

  for (int i = 0; i < 2; i++) {
    time.advance(1000);
    button.onTimer();  // #2
    time.advance(30);
    button.onTimer();  // #3 ON_PRESS
    time.advance(60);
    button.onTimer();  // #4
    time.advance(450);
    button.onTimer();  // #5 ON_HOLD
    time.advance(10000);
    button.onTimer();  // #6
    time.advance(10);
    button.onTimer();  // #7 new state candidate
    time.advance(100);
    button.onTimer();  // #8 ON_RELEASE
    time.advance(500);
    button.onTimer();  // #9
  }
}

TEST(ButtonTests, OnHoldRepeated) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3
      .WillOnce(Return(1))  // #4
      .WillOnce(Return(1))  // #5
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7
      .WillOnce(Return(1))  // #8
      .WillOnce(Return(0))  // #9
      .WillOnce(Return(0));  // #10

  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(3);
  EXPECT_CALL(mock1, handleAction(Supla::ON_LONG_CLICK_0, 5)).Times(0);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(500);
  button.repeatOnHoldEvery(100);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(5, mock1, Supla::ON_LONG_CLICK_0);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);
  button.onTimer();  // #3 ON_PRESS
  time.advance(60);
  button.onTimer();  // #4
  time.advance(450);
  button.onTimer();  // #5 ON_HOLD
  time.advance(110);
  button.onTimer();  // #6 ON_HOLD
  time.advance(10);
  button.onTimer();  // #7
  time.advance(100);
  button.onTimer();  // #8 ON_HOLD
  time.advance(10);
  button.onTimer();  // #9 new state candidate
  time.advance(30);
  button.onTimer();  // #10 ON_RELEASE
}

TEST(ButtonTests, OnHoldNotRepeated) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3
      .WillOnce(Return(1))  // #4
      .WillOnce(Return(1))  // #5
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7
      .WillOnce(Return(1))  // #8
      .WillOnce(Return(0))  // #9
      .WillOnce(Return(0));  // #10

  EXPECT_CALL(mock1, handleAction(Supla::ON_PRESS, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CHANGE, 2)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_RELEASE, 3)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_LONG_CLICK_0, 5)).Times(0);

  Supla::Control::Button button(5, false, false);

  button.setHoldTime(500);
//  button.repeatOnHoldEvery(100);
  button.onInit();
  button.addAction(1, mock1, Supla::ON_PRESS);
  button.addAction(2, mock1, Supla::ON_CHANGE);
  button.addAction(3, mock1, Supla::ON_RELEASE);
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(5, mock1, Supla::ON_LONG_CLICK_0);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);
  button.onTimer();  // #3 ON_PRESS
  time.advance(60);
  button.onTimer();  // #4
  time.advance(450);
  button.onTimer();  // #5 ON_HOLD
  time.advance(110);
  button.onTimer();  // #6 ON_HOLD
  time.advance(10);
  button.onTimer();  // #7
  time.advance(100);
  button.onTimer();  // #8 ON_HOLD
  time.advance(10);
  button.onTimer();  // #9 new state candidate
  time.advance(30);
  button.onTimer();  // #10 ON_RELEASE
}

TEST(ButtonTests, Multiclick) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 click 1
      .WillOnce(Return(0))  // #4
      .WillOnce(Return(0))  // #5 release
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7 click 2
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0))  // #9 release
      .WillOnce(Return(0));  // #10 some time -> ON_CLICK_2

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_2, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(0);

  // Conditional on_press and on_change are send only on first button press.
  // Conditional on_release is send, because there was multiclick detected.
  // Conditional on_change is send twice (on_press + on_release)
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_CHANGE, 10)).Times(3);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 9)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 11)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);
  button.setMulticlickTime(300);
  button.onInit();
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(1, mock1, Supla::ON_CLICK_3);
  button.addAction(1, mock1, Supla::ON_CLICK_4);
  button.addAction(1, mock1, Supla::ON_CLICK_5);
  button.addAction(1, mock1, Supla::ON_CLICK_6);
  button.addAction(1, mock1, Supla::ON_CLICK_7);
  button.addAction(9, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(10, mock1, Supla::CONDITIONAL_ON_CHANGE);
  button.addAction(11, mock1, Supla::CONDITIONAL_ON_RELEASE);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);  // filtering
  button.onTimer();  // #3 click 1
  time.advance(60);  // debounce
  button.onTimer();  // #4
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(500);
  button.onTimer();  // #10 ON_CLICK_2
}

TEST(ButtonTests, OnHoldSendsLongMulticlick) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 click 1
      .WillOnce(Return(1))  // #4 ON_HOLD
      .WillOnce(Return(0))  // #5
      .WillOnce(Return(0))  // #6 release
      .WillOnce(Return(1))  // #7
      .WillOnce(Return(1))  // #8 click 1
      .WillOnce(Return(0))  // #9
      .WillOnce(Return(0))  // #10 release
      .WillOnce(Return(1))  // #11
      .WillOnce(Return(1))  // #12 click 2
      .WillOnce(Return(0))  // #13
      .WillOnce(Return(0))  // #14 release
      .WillOnce(Return(0));  // #15 some time -> ON_LONG_CLICK_2

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_2, 1)).Times(0);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_LONG_CLICK_2, 5)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);
  button.setMulticlickTime(300);
  button.onInit();  // #1
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(1, mock1, Supla::ON_CLICK_3);
  button.addAction(1, mock1, Supla::ON_CLICK_4);
  button.addAction(1, mock1, Supla::ON_CLICK_5);
  button.addAction(1, mock1, Supla::ON_CLICK_6);
  button.addAction(1, mock1, Supla::ON_CLICK_7);
  button.addAction(6, mock1, Supla::ON_LONG_CLICK_1);
  button.addAction(5, mock1, Supla::ON_LONG_CLICK_2);
  button.addAction(7, mock1, Supla::ON_LONG_CLICK_3);

  time.advance(1000);
  button.onTimer();   // #2
  time.advance(30);   // filtering
  button.onTimer();   // #3 click 1
  time.advance(800);  //
  button.onTimer();   // #4 ON_HOLD
  time.advance(60);
  button.onTimer();
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(60);
  button.onTimer();  // #10
  time.advance(30);  // filtering
  button.onTimer();  // #11 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #12
  time.advance(30);  // filtering
  button.onTimer();  // #13 release
  time.advance(500);
  button.onTimer();  // #14 ON_LONG_CLICK_2
}

TEST(ButtonTests, BistableMulticlick) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 click 1
      .WillOnce(Return(0))  // #4
      .WillOnce(Return(0))  // #5 release
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7 click 2
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0))  // #9 release
      .WillOnce(Return(0));  // #10 some time -> ON_CLICK_2

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_4, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(0);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);  // should be ignored
  button.setMulticlickTime(300, true);

  button.onInit();
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(1, mock1, Supla::ON_CLICK_3);
  button.addAction(1, mock1, Supla::ON_CLICK_4);
  button.addAction(1, mock1, Supla::ON_CLICK_5);
  button.addAction(1, mock1, Supla::ON_CLICK_6);
  button.addAction(1, mock1, Supla::ON_CLICK_7);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);  // filtering
  button.onTimer();  // #3 click 1
  time.advance(60);  // debounce
  button.onTimer();  // #4
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(500);
  button.onTimer();  // #10 ON_CLICK_4
}

TEST(ButtonTests, OnHoldDoesntWorkOnBistableButton) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))   // #1 onInit
      .WillOnce(Return(1))   // #2 time 0 - first read
      .WillOnce(Return(1))   // #3 click 1
      .WillOnce(Return(1))   // #4 ON_HOLD shouldn't happen -
                             // there will be ON_CLICK_1 instead
      .WillOnce(Return(0))   // #5
      .WillOnce(Return(0))   // #6
      .WillOnce(Return(1))   // #7
      .WillOnce(Return(1))   // #8
      .WillOnce(Return(0))   // #9
      .WillOnce(Return(0))   // #10 release
      .WillOnce(Return(0));  // #11 some time -> no ON_CLICK_3

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_1, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_3, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(0);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);  // should be ignored
  button.setMulticlickTime(300, true);
  button.onInit();
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(1, mock1, Supla::ON_CLICK_3);
  button.addAction(1, mock1, Supla::ON_CLICK_4);
  button.addAction(1, mock1, Supla::ON_CLICK_5);
  button.addAction(1, mock1, Supla::ON_CLICK_6);
  button.addAction(1, mock1, Supla::ON_CLICK_7);

  time.advance(1000);
  button.onTimer();   // #2
  time.advance(30);   // filtering
  button.onTimer();   // #3 click 1
  time.advance(800);  //
  button.onTimer();   // #4 ON_HOLD
  time.advance(60);
  button.onTimer();
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(500);
  button.onTimer();  // #10 ON_CLICK_2
}

TEST(ButtonTests, MulticlickShouldSendEventAsap) {
  // we have configured on_click_2, so it should send that event on second
  // detected click and should not wait longer. Following clicks should be
  // ignored.
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 click 1 (ignored), conditional_on_press send
      .WillOnce(Return(0))  // #4
      .WillOnce(Return(0))  // #5 release, contional_on_release send
      .WillOnce(Return(1))  // #6
      .WillOnce(Return(1))  // #7 click 2 (send)
      .WillOnce(Return(0))  // #8
      .WillOnce(Return(0))  // #9 release
      .WillOnce(Return(1))  // #10
      .WillOnce(Return(1))  // #11 click 3 (ignored)
      .WillOnce(Return(0))  // #12
      .WillOnce(Return(0))  // #13 release
      .WillOnce(Return(1))  // #14
      .WillOnce(Return(1))  // #15
      .WillOnce(Return(0))  // #16
      .WillOnce(Return(0))  // #17 release
      .WillOnce(Return(0));  // #18 some time -> ON_CLICK_2

  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_2, 1)).Times(1);
  EXPECT_CALL(mock1, handleAction(Supla::ON_HOLD, 4)).Times(0);

  // Conditional on_press and on_change are send only on first button press.
  // Conditional on_release is send, because there was multiclick detected.
  // Conditional on_change is send twice (on_press + on_release)
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_CHANGE, 10)).Times(3);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 9)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 11)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.setHoldTime(700);
  button.setMulticlickTime(300);
  button.onInit();
  button.addAction(4, mock1, Supla::ON_HOLD);
  button.addAction(1, mock1, Supla::ON_CLICK_1);
  button.addAction(1, mock1, Supla::ON_CLICK_2);
  button.addAction(9, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(10, mock1, Supla::CONDITIONAL_ON_CHANGE);
  button.addAction(11, mock1, Supla::CONDITIONAL_ON_RELEASE);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);  // filtering
  button.onTimer();  // #3 click 1
  time.advance(60);  // debounce
  button.onTimer();  // #4
  time.advance(30);  // filtering
  button.onTimer();  // #5 release
  time.advance(60);  // debounce
  button.onTimer();  // #6
  time.advance(30);  // filtering
  button.onTimer();  // #7 click 2
  time.advance(60);  // debounce
  button.onTimer();  // #8
  time.advance(30);  // filtering
  button.onTimer();  // #9 release
  time.advance(100);
  button.onTimer();  // #10 ON_CLICK_2
  time.advance(100);
  button.onTimer();  // #11
  time.advance(100);
  button.onTimer();  // #12
  time.advance(100);
  button.onTimer();  // #13
  time.advance(100);
  button.onTimer();  // #14
  time.advance(100);
  button.onTimer();  // #15
  time.advance(100);
  button.onTimer();  // #16
  time.advance(100);
  button.onTimer();  // #17
  time.advance(100);
  button.onTimer();  // #18
}

TEST(ButtonTests, MotionSensorClicks) {
  SimpleTime time;
  DigitalInterfaceMock ioMock;
  ActionHandlerMock mock1;

  EXPECT_CALL(ioMock, pinMode(5, INPUT));
  EXPECT_CALL(ioMock, digitalRead(5))
      .WillOnce(Return(0))  // #1 onInit
      .WillOnce(Return(1))  // #2 time 0 - first read
      .WillOnce(Return(1))  // #3 on press
      .WillOnce(Return(0))  // #4
      .WillOnce(Return(0))  // #5 release
      .WillOnce(Return(0))  // #6
      .WillOnce(Return(0))  // #7
      .WillOnce(Return(1))  // #8
      .WillOnce(Return(1))  // #9 on press
      .WillOnce(Return(0))  // #10
      .WillOnce(Return(0))  // #11 release
      .WillOnce(Return(1))  // #12
      .WillOnce(Return(1))  // #13 on press
      .WillOnce(Return(0))  // #14
      .WillOnce(Return(0))  // #15 release
      .WillOnce(Return(0));  // #16 some time -> ON_CLICK_4 - motion sensor
                             // counts in the same way as bistable input

  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_PRESS, 1)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::CONDITIONAL_ON_RELEASE, 1)).Times(2);
  EXPECT_CALL(mock1, handleAction(Supla::ON_CLICK_4, 1)).Times(1);

  Supla::Control::Button button(5, false, false);
  button.setButtonType(Supla::Control::Button::ButtonType::MOTION_SENSOR);
  button.setHoldTime(700);  // should be ignored
  button.setMulticlickTime(300);

  button.onInit();
  button.addAction(1, mock1, Supla::CONDITIONAL_ON_PRESS);
  button.addAction(1, mock1, Supla::CONDITIONAL_ON_RELEASE);
  button.addAction(1, mock1, Supla::ON_CLICK_4);

  time.advance(1000);
  button.onTimer();  // #2
  time.advance(30);  // filtering
  button.onTimer();  // #3 on press
  time.advance(60);  // debounce
  button.onTimer();  // #4
  time.advance(30);  // filtering
  button.onTimer();  // #5 on release
  time.advance(60);  // debounce
  button.onTimer();  // #6

  time.advance(1000);
  button.onTimer();  // #7
  time.advance(30);  //
  button.onTimer();  // #8
  time.advance(60);  // filtering
  button.onTimer();  // #9 on press
  time.advance(60);  // debounce
  button.onTimer();  // #10 release
  time.advance(60);  //
  button.onTimer();  // #11
  time.advance(60);  // filtering
  button.onTimer();  // #12   !!!!
  time.advance(60);  // debounce
  button.onTimer();  // #13   !!!!
  time.advance(60);  //
  button.onTimer();  // #14
  time.advance(60);  // filtering
  button.onTimer();  // #15
  time.advance(500);  // debounce
  button.onTimer();  // #16 on click 4
}
