// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/control/custom_hvac.h>

#include <string>
#include <variant>

namespace {

class FakePayload : public Supla::Payload::Payload {
 public:
  FakePayload() : Supla::Payload::Payload(nullptr) {
  }

  bool isBasedOnIndex() override {
    return false;
  }

  void turnOn(const std::string &,
              std::variant<int, bool, std::string>) override {
    turnOnCount++;
  }

  void turnOff(const std::string &,
               std::variant<int, bool, std::string>) override {
    turnOffCount++;
  }

  int turnOnCount = 0;
  int turnOffCount = 0;
};

class CustomHvacForTests : public Supla::Control::CustomHvac {
 public:
  explicit CustomHvacForTests(Supla::Payload::Payload *payload)
      : CustomHvac(payload) {
  }

  using Supla::Control::HvacBase::setOutput;
};

class Sd4linuxCustomHvacTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }

  static void configureSubfunction(CustomHvacForTests *hvac,
                                   uint8_t subfunction) {
    hvac->getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
    hvac->setSubfunction(subfunction);
  }

  SimpleTime time;
};

}  // namespace

TEST_F(Sd4linuxCustomHvacTests, ValueOneExecutesOnPayloadCommand) {
  FakePayload payload;
  CustomHvacForTests hvac(&payload);
  configureSubfunction(&hvac, SUPLA_HVAC_SUBFUNCTION_HEAT);

  hvac.setOutput(1, true);

  EXPECT_EQ(payload.turnOnCount, 1);
  EXPECT_EQ(payload.turnOffCount, 0);
}

TEST_F(Sd4linuxCustomHvacTests,
       NegativeOneExecutesOnPayloadCommandForCooling) {
  FakePayload payload;
  CustomHvacForTests hvac(&payload);
  configureSubfunction(&hvac, SUPLA_HVAC_SUBFUNCTION_COOL);

  hvac.setOutput(-1, true);

  EXPECT_EQ(payload.turnOnCount, 1);
  EXPECT_EQ(payload.turnOffCount, 0);
}

TEST_F(Sd4linuxCustomHvacTests, ZeroExecutesOffPayloadCommand) {
  FakePayload payload;
  CustomHvacForTests hvac(&payload);
  configureSubfunction(&hvac, SUPLA_HVAC_SUBFUNCTION_HEAT);

  hvac.setOutput(0, true);

  EXPECT_EQ(payload.turnOnCount, 0);
  EXPECT_EQ(payload.turnOffCount, 1);
}

