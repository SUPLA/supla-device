// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <simple_time.h>
#include <storage_mock.h>
#include <supla/actions.h>
#include <supla/control/cmd_roller_shutter.h>
#include <supla/control/rgbcct_parsed.h>
#include <supla/control/rgbcct_remote.h>
#include <supla/parser/json.h>
#include <supla/parser/simple.h>
#include <supla/payload/json.h>
#include <supla/payload/simple.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

class CapturingOutput : public Supla::Output::Output {
 public:
  bool putContent(int) override { return false; }
  bool putContent(bool) override { return false; }
  bool putContent(const std::vector<int> &) override { return false; }
  bool putContent(const std::string &payload) override {
    attempts.push_back(payload);
    return succeed;
  }

  nlohmann::json lastState() const {
    return nlohmann::json::parse(attempts.back());
  }

  bool succeed = true;
  std::vector<std::string> attempts;
};

class TestShutter : public Supla::Control::CmdRollerShutter {
 public:
  TestShutter() : CmdRollerShutter(nullptr) {
    setCmdUpOn("up");
    setCmdUpOff("up off");
    setCmdDownOn("down");
    setCmdDownOff("down off");
    setOpenCloseTime(5000, 5000);
  }
  void relayUpOn() override {}
  void relayUpOff() override {}
  void relayDownOn() override {
    if (duringDownOn) {
      duringDownOn();
    }
  }
  void relayDownOff() override {}

  std::function<void()> duringDownOn;
};

class LightFeedbackSource : public Supla::Source::Source {
 public:
  std::string getContent() override { return content; }
  bool isConnected() override { return connected; }

  std::string content;
  bool connected = true;
};

class Sd4linuxStateOutputTests : public testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    time.advance(100);
  }

  void tick(Supla::Control::RgbCctParsed &light, int ms) {
    time.advance(ms);
    light.onFastTimer();
    light.iterateAlways();
  }

  SimpleTime time;
  CapturingOutput output;
  Supla::Payload::Json payload{&output};
};

TEST_F(Sd4linuxStateOutputTests, NativePwmOrderAndValuesForEachLayout) {
  struct TestCase {
    uint32_t function;
    int count;
    std::vector<int> expected;
  };
  const TestCase examples[] = {
      {SUPLA_CHANNELFNC_DIMMER, 1, {126, 0, 0, 0, 0}},
      {SUPLA_CHANNELFNC_DIMMER_CCT, 2, {0, 126, 0, 0, 0}},
      {SUPLA_CHANNELFNC_RGBLIGHTING, 3, {255, 128, 0, 0, 0}},
      {SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING, 4, {255, 128, 0, 126, 0}},
      {SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB, 5, {255, 128, 0, 0, 126}},
  };
  for (const auto &example : examples) {
    SCOPED_TRACE(example.function);
    output.attempts.clear();
    Supla::Control::RgbCctParsed light(nullptr);
    light.setLayout(example.function);
    light.setPwmResolutionBits(8);
    light.setPwmOutput(&payload, 20);
    light.onInit();
    light.setRGBCCT(255, 128, 0, 100, 50, 100, false, true);
    light.onFastTimer();
    time.advance(20);
    light.onFastTimer();
    EXPECT_TRUE(output.attempts.empty());
    light.iterateAlways();
    ASSERT_EQ(output.attempts.size(), 1);
    EXPECT_EQ(output.lastState().at("pwm"), example.expected);
    EXPECT_EQ(output.lastState().at("active_outputs"), example.count);
    EXPECT_EQ(output.lastState().at("max_pwm"), 255);
    tick(light, 30);
    EXPECT_EQ(output.attempts.size(), 1);
  }
}

TEST_F(Sd4linuxStateOutputTests, CoalescesFadeAndRetriesItsFinalFrame) {
  Supla::Control::RgbCctParsed light(nullptr);
  light.setLayout(SUPLA_CHANNELFNC_DIMMER);
  light.setPwmOutput(&payload, 250);
  light.setFadeEffectTime(100);
  light.onInit();
  light.onFastTimer();
  tick(light, 1);
  ASSERT_EQ(output.attempts.size(), 1);
  EXPECT_EQ(output.lastState()["pwm"][0], 0);

  light.setRGBW(0, 0, 0, 0, 100);
  for (int i = 0; i < 20; ++i) {
    tick(light, 10);
  }
  ASSERT_EQ(output.attempts.size(), 1);

  output.succeed = false;
  tick(light, 50);
  ASSERT_EQ(output.attempts.size(), 2);
  EXPECT_EQ(output.lastState()["pwm"][0], 1023);
  tick(light, 249);
  EXPECT_EQ(output.attempts.size(), 2);

  output.succeed = true;
  tick(light, 1);
  ASSERT_EQ(output.attempts.size(), 3);
  EXPECT_EQ(output.lastState()["pwm"][0], 1023);
  tick(light, 250);
  EXPECT_EQ(output.attempts.size(), 3);
}

TEST_F(Sd4linuxStateOutputTests, TwoCctPairsHaveIndependentNativeMixes) {
  CapturingOutput secondOutput;
  Supla::Payload::Json secondPayload(&secondOutput);
  Supla::Control::RgbCctParsed left(nullptr);
  Supla::Control::RgbCctParsed right(nullptr);
  left.setLayout(SUPLA_CHANNELFNC_DIMMER_CCT);
  right.setLayout(SUPLA_CHANNELFNC_DIMMER_CCT);
  left.setPwmOutput(&payload, 10);
  right.setPwmOutput(&secondPayload, 10);
  left.onInit();
  right.onInit();
  left.setRGBCCT(0, 0, 0, 0, 100, 50, false, true);
  right.setRGBCCT(0, 0, 0, 0, 100, 0, false, true);
  left.onFastTimer();
  right.onFastTimer();
  tick(left, 10);
  tick(right, 10);
  ASSERT_EQ(output.attempts.size(), 1);
  ASSERT_EQ(secondOutput.attempts.size(), 1);
  const auto mixed = output.lastState()["pwm"];
  EXPECT_NEAR(mixed[0].get<int>(), 512, 1);
  EXPECT_NEAR(mixed[1].get<int>(), 512, 1);
  EXPECT_EQ(secondOutput.lastState()["pwm"],
            nlohmann::json::array({1023, 0, 0, 0, 0}));
}

TEST_F(Sd4linuxStateOutputTests, RuntimeFunctionClearsUnusedPwmSlots) {
  Supla::Control::RgbCctParsed light(nullptr);
  light.setLayout(SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB);
  light.setPwmOutput(&payload, 10);
  light.onInit();
  light.onFastTimer();
  light.setRGBCCT(255, 255, 255, 100, 100, 100, false, true);
  tick(light, 10);
  ASSERT_EQ(output.attempts.size(), 1);
  EXPECT_EQ(output.lastState()["pwm"][4], 1023);

  light.setRGBW(0, 0, 0, 0, 70);
  ASSERT_TRUE(light.setRuntimeFunction(SUPLA_CHANNELFNC_DIMMER));
  time.advance(100);
  light.onFastTimer();
  light.iterateAlways();
  EXPECT_EQ(output.attempts.size(), 1);
  tick(light, 10);
  ASSERT_EQ(output.attempts.size(), 2);
  EXPECT_EQ(output.lastState()["active_outputs"], 1);
  EXPECT_EQ(output.lastState()["pwm"], nlohmann::json::array({0, 0, 0, 0, 0}));

  ASSERT_TRUE(light.setRuntimeFunction(SUPLA_CHANNELFNC_NONE));
  tick(light, 10);
  ASSERT_EQ(output.attempts.size(), 3);
  EXPECT_EQ(output.lastState()["active_outputs"], 0);
}

TEST_F(Sd4linuxStateOutputTests, RestoresPwmAndFacadeBlindStateAtStartup) {
  auto storage = std::make_unique<StorageMockSimulator>(0, 4096);
  {
    Supla::Control::RgbCctParsed light(nullptr);
    light.setLayout(SUPLA_CHANNELFNC_DIMMER_CCT);
    TestShutter blind;
    blind.setFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
    ASSERT_TRUE(blind.applyFacadeBlindTimingConfig(5000, 5000, 500, 1, false));
    Supla::Storage::Init();
    light.onInit();
    blind.onInit();
    light.setRGBCCT(0, 0, 0, 0, 100, 100);
    blind.setCurrentPosition(60, 35);
    Supla::Storage::WriteStateStorage();
  }

  Supla::Channel::resetToDefaults();
  Supla::Control::RgbCctParsed light(nullptr);
  light.setLayout(SUPLA_CHANNELFNC_DIMMER_CCT);
  light.setPwmOutput(&payload, 250);
  light.setFadeEffectTime(0);
  TestShutter blind;
  blind.setFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND);
  ASSERT_TRUE(blind.applyFacadeBlindTimingConfig(5000, 5000, 500, 1, false));
  blind.setStateOutput(&payload, 250);
  Supla::Storage::LoadStateStorage();
  light.onInit();
  blind.onInit();

  light.onFastTimer();
  tick(light, 10);
  ASSERT_EQ(output.attempts.size(), 1);
  EXPECT_EQ(output.lastState()["pwm"],
            nlohmann::json::array({0, 1023, 0, 0, 0}));
  blind.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 2);
  const auto restored = output.lastState();
  EXPECT_EQ(restored["position"], 60);
  EXPECT_EQ(restored["tilt"], 35);
  EXPECT_EQ(restored["tilt_enabled"], true);
  EXPECT_EQ(restored["direction"], 0);
  EXPECT_EQ(restored["calibrated"], true);
  blind.setTargetPosition(80, 75);
  time.advance(250);
  blind.onTimer();
  blind.iterateAlways();
  EXPECT_EQ(output.lastState()["target_position"], 80);
  EXPECT_EQ(output.lastState()["target_tilt"], 75);
}

TEST_F(Sd4linuxStateOutputTests, PublishesUnknownCalibrationAndMovement) {
  TestShutter shutter;
  shutter.setStateOutput(&payload, 250);
  shutter.onInit();
  shutter.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 1);
  EXPECT_EQ(output.lastState()["position"], UNKNOWN_POSITION);
  EXPECT_EQ(output.lastState()["tilt"], UNKNOWN_POSITION);
  EXPECT_EQ(output.lastState()["tilt_enabled"], false);
  EXPECT_EQ(output.lastState()["calibrated"], false);
  shutter.setCalibrationOngoing();
  time.advance(250);
  shutter.onTimer();
  shutter.iterateAlways();
  EXPECT_EQ(output.lastState()["calibration_in_progress"], true);
  shutter.setCalibrationFinished();
  shutter.setCurrentPosition(50);
  time.advance(250);
  shutter.onTimer();
  shutter.iterateAlways();
  EXPECT_EQ(output.lastState()["calibrated"], true);

  shutter.setTargetPosition(100);
  shutter.onTimer();
  time.advance(250);
  shutter.onTimer();
  shutter.iterateAlways();
  EXPECT_GT(output.lastState()["position"].get<int>(), 50);
  EXPECT_EQ(output.lastState()["direction"], 1);
  EXPECT_EQ(output.lastState()["target_position"], 100);

  shutter.setTargetPosition(STOP_REQUEST);
  shutter.onTimer();
  shutter.iterateAlways();
  EXPECT_EQ(output.lastState()["direction"], 1);
  output.succeed = false;
  time.advance(250);
  shutter.iterateAlways();
  EXPECT_EQ(output.lastState()["direction"], 0);
  const auto attempted = output.attempts.size();
  output.succeed = true;
  time.advance(249);
  shutter.iterateAlways();
  EXPECT_EQ(output.attempts.size(), attempted);
  time.advance(1);
  shutter.iterateAlways();
  EXPECT_EQ(output.attempts.size(), attempted + 1);
  EXPECT_EQ(output.lastState()["direction"], 0);
  const auto published = output.attempts.size();
  time.advance(250);
  shutter.iterateAlways();
  EXPECT_EQ(output.attempts.size(), published);
}

TEST_F(Sd4linuxStateOutputTests, SimplePayloadUsesOneValuePerLine) {
  Supla::Payload::Simple simple(&output);
  TestShutter shutter;
  shutter.setStateOutput(&simple, 250);
  shutter.onInit();
  shutter.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 1);
  EXPECT_EQ(output.attempts[0], "-1\n-1\n0\n-2\n-1\n0\n0\n1\n0\n0\n0\n");
}

TEST_F(Sd4linuxStateOutputTests, PublishesOnlyAfterMotorCommandReturns) {
  TestShutter shutter;
  shutter.setStateOutput(&payload, 250);
  shutter.setCalibrationFinished();
  shutter.setCurrentPosition(50);
  shutter.onInit();
  shutter.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 1);
  EXPECT_EQ(output.lastState()["direction"], 0);

  shutter.duringDownOn = [&]() {
    shutter.iterateAlways();
    EXPECT_EQ(output.attempts.size(), 1);
  };
  shutter.setTargetPosition(100);
  time.advance(250);
  shutter.onTimer();
  shutter.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 2);
  EXPECT_EQ(output.lastState()["direction"], 1);
}

TEST_F(Sd4linuxStateOutputTests, RemoteOutputUsesLogicalValuesForEachFunction) {
  using Supla::Control::RgbCctRemote;
  const uint32_t functions[] = {
      SUPLA_CHANNELFNC_DIMMER, SUPLA_CHANNELFNC_DIMMER_CCT,
      SUPLA_CHANNELFNC_RGBLIGHTING, SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING,
      SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB,
  };
  const nlohmann::json expected[] = {
      {{"state", true}, {"white_brightness", 40}},
      {{"state", true}, {"white_brightness", 40}, {"white_temperature", 76}},
      {{"state", true}, {"rgb_brightness", 55},
       {"red", 250}, {"green", 120}, {"blue", 15}},
      {{"state", true}, {"white_brightness", 40}, {"rgb_brightness", 55},
       {"red", 250}, {"green", 120}, {"blue", 15}},
      {{"state", true}, {"white_brightness", 40}, {"white_temperature", 76},
       {"rgb_brightness", 55}, {"red", 250}, {"green", 120}, {"blue", 15}},
  };
  for (size_t i = 0; i < std::size(functions); ++i) {
    RgbCctRemote light(nullptr, payload, {}, 20, 200);
    light.setLayout(functions[i]);
    light.onInit();
    output.attempts.clear();
    light.iterateAlways();
    EXPECT_TRUE(output.attempts.empty());
    TSD_SuplaChannelNewValue value = {};
    value.value[0] = 40;
    value.value[1] = 55;
    value.value[2] = 15;
    value.value[3] = 120;
    value.value[4] = static_cast<char>(250);
    value.value[7] = 76;
    light.handleNewValueFromServer(&value);
    light.onFastTimer();
    light.iterateAlways();
    ASSERT_EQ(output.attempts.size(), 1);
    EXPECT_EQ(output.lastState(), expected[i]);
    time.advance(100);
    light.iterateAlways();
    EXPECT_EQ(output.attempts.size(), 1);
  }
}

TEST_F(Sd4linuxStateOutputTests, RemoteFeedbackTransitionsAndRecovery) {
  using Supla::Control::RgbCctRemote;
  LightFeedbackSource source;
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(10);
  RgbCctRemote light(&parser, payload, {"state", "/light/brightness"},
                     50, 250);
  light.setLayout(SUPLA_CHANNELFNC_DIMMER);
  light.setDefaultStateOn();
  light.onInit();
  EXPECT_FALSE(light.getChannel()->isStateOnline());

  auto feedback = [&](const char *json, int delay = 20) {
    source.content = json;
    time.advance(delay);
    light.iterateAlways();
  };
  feedback(R"({"state":false,"light":{"brightness":80}})");
  EXPECT_TRUE(light.getChannel()->isStateOnline());
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 0);
  EXPECT_TRUE(output.attempts.empty());
  light.handleAction(0, Supla::TURN_ON);
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 1);
  EXPECT_EQ(output.lastState()["white_brightness"], 80);
  feedback(R"({"state":true,"light":{"brightness":25}})");
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 80);
  feedback(R"({"state":true,"light":{"brightness":80}})");
  feedback(R"({"state":true,"light":{"brightness":60}})");
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 60);
  EXPECT_EQ(output.attempts.size(), 1);

  feedback(R"({"state":true})");
  EXPECT_FALSE(light.getChannel()->isStateOnline());
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 60);
  feedback("");
  EXPECT_FALSE(light.getChannel()->isStateOnline());
  source.connected = false;
  light.setRGBW(-1, -1, -1, -1, 55);
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 2);
  source.connected = true;
  feedback(R"({"state":true,"light":{"brightness":32}})", 260);
  EXPECT_TRUE(light.getChannel()->isStateOnline());
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 32);
  EXPECT_EQ(output.attempts.size(), 2);
}

TEST_F(Sd4linuxStateOutputTests, RemoteToggleWorksAfterBrightnessOnlyOff) {
  using Supla::Control::RgbCctRemote;
  LightFeedbackSource source;
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(10);
  RgbCctRemote light(&parser, payload, {"", "brightness"}, 50, 250);
  light.setLayout(SUPLA_CHANNELFNC_DIMMER);
  light.onInit();
  auto feedback = [&](const std::string &json) {
    source.content = json;
    time.advance(20);
    light.iterateAlways();
  };
  feedback(R"({"brightness":0})");
  light.handleAction(0, Supla::TURN_ON);
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 1);
  const int on = output.lastState()["white_brightness"];
  feedback(R"({"brightness":)" + std::to_string(on) + "}");
  feedback(R"({"brightness":0})");
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 0);

  // The hidden RGB brightness must not make the first toggle turn it off.
  light.handleAction(0, Supla::TOGGLE);
  time.advance(60);
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 2);
  EXPECT_EQ(output.lastState()["white_brightness"], on);
}

TEST_F(Sd4linuxStateOutputTests, RemoteSimpleCoalescesAndRetriesLatestSetting) {
  using Supla::Control::RgbCctRemote;
  LightFeedbackSource source;
  Supla::Parser::Simple parser(&source);
  parser.setRefreshTime(10);
  parser.addOptionalKey("switch", 0);
  parser.addOptionalKey("brightness", 1);
  parser.addOptionalKey("temperature", 2);
  Supla::Payload::Simple simple(&output);
  RgbCctRemote light(&parser, simple, {"switch", "brightness", "temperature"},
                     50, 220);
  light.setLayout(SUPLA_CHANNELFNC_DIMMER_CCT);
  light.onInit();
  source.content = "1\n30\n45\n";
  light.iterateAlways();
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 30);

  light.setRGBCCT(-1, -1, -1, -1, 90, 22);
  output.succeed = false;
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 1);
  light.setRGBCCT(-1, -1, -1, -1, 58, 75);
  time.advance(10);
  light.iterateAlways();
  EXPECT_EQ(output.attempts.size(), 1);
  output.succeed = true;
  time.advance(40);
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 2);
  EXPECT_EQ(output.attempts.back(), "1\n58\n75\n");
  source.content = "true\n58\n75\n";
  time.advance(50);
  light.iterateAlways();
  EXPECT_EQ(output.attempts.size(), 2);

  EXPECT_TRUE(light.setRuntimeFunction(SUPLA_CHANNELFNC_DIMMER));
  source.content = "true\n42\n";
  time.advance(50);
  light.iterateAlways();
  EXPECT_TRUE(light.getChannel()->isStateOnline());
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 42);
  EXPECT_EQ(output.attempts.size(), 2);
  light.handleAction(0, Supla::TURN_OFF);
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 3);
  EXPECT_EQ(output.attempts.back(), "0\n0\n");
}

TEST_F(Sd4linuxStateOutputTests, RemoteDropsUnsentRequestOnDisconnect) {
  using Supla::Control::RgbCctRemote;
  LightFeedbackSource source;
  Supla::Parser::Json parser(&source);
  parser.setRefreshTime(10);
  RgbCctRemote light(&parser, payload, {"state", "brightness"}, 30, 120);
  light.setLayout(SUPLA_CHANNELFNC_DIMMER);
  light.onInit();
  source.content = R"({"state":true,"brightness":25})";
  light.iterateAlways();
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 25);

  output.succeed = false;
  light.setRGBW(-1, -1, -1, -1, 80);
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 1);
  source.connected = false;
  time.advance(40);
  light.iterateAlways();
  EXPECT_FALSE(light.getChannel()->isStateOnline());
  source.connected = true;
  output.succeed = true;
  light.iterateAlways();
  EXPECT_EQ(light.getCurrentDimmerBrightness(), 25);
  EXPECT_EQ(output.attempts.size(), 1);
}

TEST_F(Sd4linuxStateOutputTests, RemoteGetsFirstAttemptAfterThrottle) {
  Supla::Control::RgbCctRemote light(nullptr, payload, {}, 100, 100);
  light.setLayout(SUPLA_CHANNELFNC_DIMMER);
  light.onInit();
  light.setRGBW(-1, -1, -1, -1, 30);
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 1);
  light.setRGBW(-1, -1, -1, -1, 70);
  light.iterateAlways();
  EXPECT_EQ(output.attempts.size(), 1);
  time.advance(150);
  light.iterateAlways();
  ASSERT_EQ(output.attempts.size(), 2);
  EXPECT_EQ(output.lastState()["white_brightness"], 70);
}

}  // namespace
