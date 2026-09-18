// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <arduino_mock.h>
#include <config_simulator.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/at_channel.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <supla/control/relay.h>
#include <supla/control/roller_shutter.h>
#include <supla/events.h>
#include <supla/storage/config_tags.h>

#include <array>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace {

class ConfigEntryObserver : public SuplaDeviceClass {
 public:
  void handleAction(int event, int action) override {
    if (action == Supla::ENTER_CONFIG_MODE_OR_RESET_TO_FACTORY) {
      EXPECT_EQ(event, Supla::ON_CLICK_10);
      ++configRequests;
    }
    SuplaDeviceClass::handleAction(event, action);
  }

  int configRequests = 0;
};

// The same input/AT/CFG scenarios exercise a real Relay or RollerShutter.
// Buttons are driven only by GPIO; all timers run every simulated millisecond.
class BistableInputTest : public testing::TestWithParam<bool> {
 protected:
  static constexpr int kOutput = 1;
  static constexpr int kDownOutput = 2;
  static constexpr int kInput = 3;
  static constexpr int kDownInput = 4;

  testing::NiceMock<DigitalInterfaceMock> io;
  SimpleTime time;
  ConfigSimulator config;
  ConfigEntryObserver device;
  std::array<int, 8> gpio{};
  std::vector<std::pair<int, int>> outputChanges;
  std::array<std::vector<uint32_t>, 2> actions;
  std::unique_ptr<Supla::Control::Button> button;
  std::unique_ptr<Supla::Control::Button> downButton;
  std::unique_ptr<Supla::Control::Relay> relay;
  std::unique_ptr<Supla::Control::RollerShutter> shutter;
  std::array<std::unique_ptr<Supla::Control::ActionTrigger>, 2> triggers;

  void SetUp() override {
    Supla::Channel::resetToDefaults();
    time.advance(1000);
    ON_CALL(io, digitalRead(testing::_))
        .WillByDefault([this](uint8_t pin) { return gpio.at(pin); });
    ON_CALL(io, digitalWrite(testing::_, testing::_))
        .WillByDefault([this](uint8_t pin, uint8_t value) {
          if (gpio.at(pin) != value) {
            outputChanges.emplace_back(pin, value);
          }
          gpio.at(pin) = value;
          EXPECT_FALSE(gpio[kOutput] && gpio[kDownOutput]);
        });
  }

  void TearDown() override {
    triggers = {};
    shutter.reset();
    relay.reset();
    downButton.reset();
    button.reset();
    Supla::Channel::resetToDefaults();
  }

  void initialize(bool cfg = true, bool alwaysClick1 = false,
                  bool initiallyPressed = false) {
    config.setInt32(Supla::ConfigTag::BtnConfigTag, cfg ? 0 : 1);
    gpio[kInput] = initiallyPressed;
    button = std::make_unique<Supla::Control::Button>(kInput);
    downButton = std::make_unique<Supla::Control::Button>(kDownInput);
    int number = 0;
    for (auto *input : {button.get(), downButton.get()}) {
      input->setButtonNumber(number++);
      input->setMulticlickTime(300, true);
      input->setDebounceDelay(0);
      input->setSwNoiseFilterDelay(0);
      input->onLoadConfig(&device);
    }
    if (GetParam()) {
      shutter = std::make_unique<Supla::Control::RollerShutter>(
          kOutput, kDownOutput);
      shutter->attach(button.get(), downButton.get());
      shutter->onInit();
    } else {
      relay = std::make_unique<Supla::Control::Relay>(kOutput);
      relay->attach(button.get());
      relay->onInit();
      downButton->onInit();
    }
    for (int i = 0; i < 2; ++i) {
      triggers[i] = std::make_unique<Supla::Control::ActionTrigger>();
      triggers[i]->attach(i == 0 ? button.get() : downButton.get());
      if (alwaysClick1) {
        triggers[i]->setAlwaysUseOnClick1();
      }
      triggers[i]->onInit();
    }
    tick(10);
    outputChanges.clear();
  }

  void configure(uint32_t mask, int input = 0) {
    TSD_ChannelConfig message = {};
    message.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
    message.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
    TChannelConfig_ActionTrigger settings = {};
    settings.ActiveActions = mask;
    memcpy(message.Config, &settings, sizeof(settings));
    triggers[input]->handleChannelConfig(&message);
  }

  void configurePublishing(int mode) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(
        key, triggers[0]->getChannelNumber(),
        Supla::ConfigTag::BtnActionTriggerCfgTagPrefix);
    config.setInt32(key, mode);
    triggers[0]->onLoadConfig(&device);
    triggers[0]->rebuildForAttachedButton();
  }

  void tick(int milliseconds) {
    for (int i = 0; i < milliseconds; ++i) {
      time.advance(1);
      button->onTimer();
      downButton->onTimer();
      if (shutter) {
        shutter->onTimer();
      }
      if (relay) {
        relay->iterateAlways();
      }
      for (int j = 0; j < 2; ++j) {
        auto *channel = static_cast<Supla::AtChannel *>(
            triggers[j]->getChannel());
        while (auto action = channel->popAction()) {
          actions[j].push_back(action);
        }
      }
    }
  }

  void edge(bool pressed, int pin = kInput) {
    gpio[pin] = pressed;
    tick(5);
  }

  void expectStopped() {
    EXPECT_EQ(gpio[kOutput], LOW);
    EXPECT_EQ(gpio[kDownOutput], LOW);
  }
};

TEST_P(BistableInputTest, CfgCountsTenEdgesWithoutDelayingFirstLocalActions) {
  initialize();
  ASSERT_EQ(button->getMaxMulticlickValue(), 10);
  edge(true);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  expectStopped();
  ASSERT_EQ(outputChanges.size(), 2);
  outputChanges.clear();
  for (int i = 2; i < 10; ++i) {
    EXPECT_EQ(device.configRequests, 0);
    edge(i % 2 == 0);
  }
  EXPECT_EQ(device.configRequests, 1);
  EXPECT_TRUE(outputChanges.empty());
  EXPECT_TRUE(actions[0].empty());
}

TEST_P(BistableInputTest, CfgSequenceCanStartWithRelease) {
  initialize(true, false, true);
  for (int i = 0; i < 10; ++i) {
    edge(i % 2 != 0);
    if (i == 1) {
      outputChanges.clear();
    }
  }
  EXPECT_EQ(device.configRequests, 1);
  EXPECT_TRUE(outputChanges.empty());
  EXPECT_TRUE(actions[0].empty());
}

TEST_P(BistableInputTest, ToggleTwoDoesNotCauseEvenTransientLocalMovement) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(true);
  tick(100);
  edge(false);
  EXPECT_TRUE(actions[0].empty());
  tick(301);
  EXPECT_THAT(actions[0], testing::ElementsAre(SUPLA_ACTION_CAP_TOGGLE_x2));
  EXPECT_TRUE(outputChanges.empty());
  expectStopped();
}

TEST_P(BistableInputTest, ToggleTwoRecognizesReleaseThenPress) {
  initialize(true, false, true);
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(false);
  tick(100);
  edge(true);
  tick(301);
  EXPECT_THAT(actions[0], testing::ElementsAre(SUPLA_ACTION_CAP_TOGGLE_x2));
  EXPECT_TRUE(outputChanges.empty());
}

TEST_P(BistableInputTest, ToggleTwoWithoutCfgDoesNotMoveLocally) {
  initialize(false);
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  ASSERT_EQ(button->getMaxMulticlickValue(), 2);
  edge(true);
  edge(false);
  tick(301);
  EXPECT_THAT(actions[0], testing::ElementsAre(SUPLA_ACTION_CAP_TOGGLE_x2));
  EXPECT_TRUE(outputChanges.empty());
}

TEST_P(BistableInputTest, TenEdgesEnterCfgWithoutSendingIntermediateToggleTwo) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x1 | SUPLA_ACTION_CAP_TOGGLE_x2);
  ASSERT_EQ(button->getMaxMulticlickValue(), 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(device.configRequests, 0);
    edge(i % 2 == 0);
    tick(50);
    EXPECT_TRUE(actions[0].empty());
    EXPECT_TRUE(outputChanges.empty());
  }
  EXPECT_EQ(device.configRequests, 1);
}

TEST_P(BistableInputTest, SeparateSingleEdgesOperateLocallyAfterTimeout) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(true);
  expectStopped();
  tick(290);
  expectStopped();
  tick(20);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  EXPECT_EQ(gpio[kOutput], HIGH);
  tick(301);
  expectStopped();
  EXPECT_TRUE(actions[0].empty());
}

TEST_P(BistableInputTest, CfgTenTakesPriorityOverToggleTwoAndLocalOperation) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  for (int i = 0; i < 10; ++i) {
    edge(i % 2 == 0);
    tick(50);
    EXPECT_TRUE(actions[0].empty());
    EXPECT_TRUE(outputChanges.empty());
  }
  EXPECT_EQ(device.configRequests, 1);
}

TEST_P(BistableInputTest, ToggleOneDisablesBothLocalEdges) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x1);
  edge(true);
  tick(301);
  edge(false);
  tick(301);
  EXPECT_THAT(actions[0], testing::ElementsAre(SUPLA_ACTION_CAP_TOGGLE_x1,
                                             SUPLA_ACTION_CAP_TOGGLE_x1));
  EXPECT_TRUE(outputChanges.empty());
}

TEST_P(BistableInputTest, RemovingConfigRestoresImmediateLocalOperation) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x1 | SUPLA_ACTION_CAP_TOGGLE_x2);
  configure(0);
  edge(true);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  expectStopped();
  tick(301);
  EXPECT_EQ(outputChanges.size(), 2);
  EXPECT_TRUE(actions[0].empty());
  EXPECT_EQ(button->getMaxMulticlickValue(), 10);
}

TEST_P(BistableInputTest, AlwaysClickOneDefersEvenWithoutServerActions) {
  initialize(true, true);
  edge(true);
  expectStopped();
  tick(301);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  tick(301);
  expectStopped();
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  configure(0);
  edge(true);
  expectStopped();
  tick(600);
  EXPECT_EQ(gpio[kOutput], HIGH);
  EXPECT_TRUE(actions[0].empty());
}

TEST_P(BistableInputTest, PublishAllDisableNoneKeepsSingleLocalOperation) {
  initialize();
  configurePublishing(1);
  edge(true);
  expectStopped();
  tick(301);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  tick(301);
  expectStopped();
  EXPECT_THAT(actions[0], testing::ElementsAre(
      SUPLA_ACTION_CAP_TURN_ON, SUPLA_ACTION_CAP_TOGGLE_x1,
      SUPLA_ACTION_CAP_TURN_OFF, SUPLA_ACTION_CAP_TOGGLE_x1));
}

TEST_P(BistableInputTest, PublishAllDisableAllDoesNotDisableCfg) {
  initialize();
  configurePublishing(2);
  configure(0);
  edge(true);
  tick(301);
  edge(false);
  tick(301);
  EXPECT_TRUE(outputChanges.empty());
  for (int i = 0; i < 10; ++i) {
    edge(i % 2 == 0);
  }
  EXPECT_EQ(device.configRequests, 1);
  EXPECT_TRUE(outputChanges.empty());
}

TEST_P(BistableInputTest, RepeatedServerConfigKeepsToggleTwoSequence) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(true);
  tick(100);
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(false);
  tick(301);
  EXPECT_THAT(actions[0], testing::ElementsAre(SUPLA_ACTION_CAP_TOGGLE_x2));
  EXPECT_TRUE(outputChanges.empty());
}

TEST_P(BistableInputTest, RepeatedEmptyConfigInPublishAllKeepsCfgSequence) {
  initialize();
  configurePublishing(2);
  configure(0);
  for (int i = 0; i < 10; ++i) {
    if (i == 4) {
      // PublishAllDisableAll uses an effective mask of 0xFFFFFFFF, but the
      // repeated server configuration is still the same empty mask.
      configure(0);
    }
    edge(i % 2 == 0);
    tick(50);
  }
  EXPECT_EQ(device.configRequests, 1);
  EXPECT_TRUE(outputChanges.empty());
}

class DirectionalInputTest : public BistableInputTest {};

TEST_P(DirectionalInputTest, RepeatedServerConfigPreservesPendingStop) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(true);
  tick(301);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  tick(100);
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  // The original release deadline must be kept, not restarted.
  tick(200);
  expectStopped();
  EXPECT_TRUE(actions[0].empty());
}

TEST_P(DirectionalInputTest, RepeatedEmptyConfigPreservesPendingStop) {
  initialize(true, true);
  configure(0);
  edge(true);
  tick(301);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  tick(100);
  configure(0);
  tick(200);
  expectStopped();
  EXPECT_TRUE(actions[0].empty());
}

TEST_P(DirectionalInputTest, ChangedConfigDiscardsPendingRelease) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(true);
  tick(301);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  tick(100);
  configure(SUPLA_ACTION_CAP_TOGGLE_x3);
  tick(400);
  EXPECT_EQ(gpio[kOutput], HIGH);
  EXPECT_TRUE(actions[0].empty());
}

TEST_P(DirectionalInputTest, ExplicitRebuildStillDiscardsPendingEdge) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(true);
  triggers[0]->rebuildForAttachedButton();
  tick(301);
  EXPECT_TRUE(outputChanges.empty());
  EXPECT_TRUE(actions[0].empty());
}

TEST_P(DirectionalInputTest, ChangedServerMaskInPublishAllDiscardsSequence) {
  initialize();
  configurePublishing(2);
  configure(0);
  for (int i = 0; i < 4; ++i) {
    edge(i % 2 == 0);
  }
  // The effective mask stays 0xFFFFFFFF, but the server mask has changed.
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  for (int i = 0; i < 6; ++i) {
    edge(i % 2 == 0);
  }
  EXPECT_EQ(device.configRequests, 0);
  EXPECT_TRUE(outputChanges.empty());
}

TEST_P(DirectionalInputTest, DirectionalStopIsNotSwallowedWithOnlyClickOne) {
  initialize(false, true);
  ASSERT_EQ(button->getMaxMulticlickValue(), 1);
  edge(true);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  expectStopped();
  tick(600);
  expectStopped();
}

TEST_P(DirectionalInputTest, TurnOnAndOffKeepIndependentDirectionalMasks) {
  initialize();
  configure(SUPLA_ACTION_CAP_TURN_ON);
  edge(true);
  EXPECT_THAT(actions[0], testing::ElementsAre(SUPLA_ACTION_CAP_TURN_ON));
  tick(301);
  expectStopped();
  // A remote command starts the motor. The unmasked release must stop it.
  shutter->moveUp();
  tick(10);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  tick(301);
  expectStopped();
  configure(SUPLA_ACTION_CAP_TURN_OFF);
  tick(600);
  edge(true);
  tick(301);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  tick(301);
  EXPECT_EQ(gpio[kOutput], HIGH);
  EXPECT_THAT(actions[0], testing::ElementsAre(SUPLA_ACTION_CAP_TURN_ON,
                                             SUPLA_ACTION_CAP_TURN_OFF));
}

TEST_P(DirectionalInputTest, TurnOnAndOffTogetherDisableBothLocalEdges) {
  initialize();
  configure(SUPLA_ACTION_CAP_TURN_ON | SUPLA_ACTION_CAP_TURN_OFF);
  edge(true);
  tick(301);
  edge(false);
  tick(301);
  EXPECT_TRUE(outputChanges.empty());
  EXPECT_THAT(actions[0], testing::ElementsAre(SUPLA_ACTION_CAP_TURN_ON,
                                             SUPLA_ACTION_CAP_TURN_OFF));
  configure(0);
  edge(true);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  expectStopped();
}

TEST_P(DirectionalInputTest, UnmaskedReleaseWorksWithoutAnyClickHandler) {
  initialize(false);
  configure(SUPLA_ACTION_CAP_TURN_ON);
  // Only raw TURN_ON is subscribed, but the local release still needs x1.
  ASSERT_EQ(button->getMaxMulticlickValue(), 1);
  edge(true);
  expectStopped();
  shutter->moveUp();
  tick(10);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  expectStopped();
  EXPECT_THAT(actions[0], testing::ElementsAre(SUPLA_ACTION_CAP_TURN_ON));
}

TEST_P(DirectionalInputTest, ZeroMulticlickTimeKeepsDirectionalEdgesImmediate) {
  initialize(false, true);
  button->setMulticlickTime(0);
  edge(true);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  expectStopped();
}

TEST_P(DirectionalInputTest, ReconfigurationDiscardsPendingDirectionalEdge) {
  initialize();
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(true);
  configure(SUPLA_ACTION_CAP_TOGGLE_x3);
  tick(301);
  EXPECT_TRUE(outputChanges.empty());
  EXPECT_TRUE(actions[0].empty());
  edge(false);
  configure(0);
  tick(301);
  EXPECT_TRUE(outputChanges.empty());
  edge(true);
  EXPECT_EQ(gpio[kOutput], HIGH);
}

TEST_P(DirectionalInputTest, LaterDirectionWinsAndOldReleaseDoesNotStopIt) {
  initialize(true, true);
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  configure(SUPLA_ACTION_CAP_TOGGLE_x2, 1);
  edge(true, kDownInput);
  tick(301);
  EXPECT_EQ(gpio[kDownOutput], HIGH);
  edge(true);
  tick(301);
  expectStopped();
  tick(501);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false, kDownInput);
  tick(301);
  EXPECT_EQ(gpio[kOutput], HIGH);
  edge(false);
  tick(301);
  expectStopped();
  EXPECT_TRUE(actions[0].empty());
  EXPECT_TRUE(actions[1].empty());
}

TEST_P(DirectionalInputTest, UpsideDownMapsDeferredPressAndReleaseTogether) {
  initialize(true, true);
  shutter->setRsConfigButtonsUpsideDownValue(2);
  configure(SUPLA_ACTION_CAP_TOGGLE_x2);
  edge(true);
  tick(301);
  EXPECT_EQ(gpio[kDownOutput], HIGH);
  edge(false);
  tick(301);
  expectStopped();
}

INSTANTIATE_TEST_SUITE_P(RelayAndRollerShutter, BistableInputTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(RollerShutter, DirectionalInputTest,
                         testing::Values(true));

}  // namespace
