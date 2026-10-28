// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/control/cmd_roller_shutter.h>
#include <string>

namespace {

class SimulatedCmdRollerShutter : public Supla::Control::CmdRollerShutter {
 public:
  SimulatedCmdRollerShutter() : CmdRollerShutter(nullptr) {}

  // Keep the real readiness check and movement engine; replace only outputs.
  void relayUpOn() override {
    EXPECT_FALSE(downOn);
    upOn = true;
    events += 'U';
  }
  void relayDownOn() override {
    EXPECT_FALSE(upOn);
    downOn = true;
    events += 'D';
  }
  void relayUpOff() override {
    upOn = false;
    events += 'u';
  }
  void relayDownOff() override {
    downOn = false;
    events += 'd';
  }

  std::string events;
  bool upOn = false;
  bool downOn = false;
};

class Sd4linuxCmdRollerShutterTests : public testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    time.advance(1000);
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }

  void configure(Supla::Control::CmdRollerShutter &rs, int missing = -1) {
    // Nonempty configuration tokens are never executed by simulated outputs.
    rs.setCmdUpOn(missing == 0 ? "" : "simulated-up-on");
    rs.setCmdUpOff(missing == 1 ? "" : "simulated-up-off");
    rs.setCmdDownOn(missing == 2 ? "" : "simulated-down-on");
    rs.setCmdDownOff(missing == 3 ? "" : "simulated-down-off");
    rs.setOpenCloseTime(5000, 5000);
  }

  void recalibrate(Supla::Control::RollerShutter &rs) {
    TSD_DeviceCalCfgRequest request = {};
    request.Command = SUPLA_CALCFG_CMD_RECALIBRATE;
    request.SuperUserAuthorized = 1;
    ASSERT_EQ(rs.handleCalcfgFromServer(&request), SUPLA_CALCFG_RESULT_DONE);
  }

  SimpleTime time;
};

TEST_F(Sd4linuxCmdRollerShutterTests, CalibratesUsingCommandsWithoutGpio) {
  SimulatedCmdRollerShutter rs;
  configure(rs);
  rs.onInit();
  recalibrate(rs);
  rs.onTimer();
  ASSERT_TRUE(rs.isCalibrationInProgress());
  EXPECT_EQ(rs.events, "dU");
  time.advance(5501);  // Calibration includes a 10% margin.
  rs.onTimer();
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_TRUE(rs.isCalibrated());
  EXPECT_EQ(rs.getCurrentPosition(), 0);
  EXPECT_EQ(rs.events, "dUud");
  EXPECT_FALSE(rs.upOn);
  EXPECT_FALSE(rs.downOn);
}

TEST_F(Sd4linuxCmdRollerShutterTests,
       MovesBothDirectionsAndStopsUsingCommands) {
  SimulatedCmdRollerShutter rs;
  configure(rs);
  rs.onInit();
  rs.setNotCalibrated();
  rs.setCurrentPosition(50);
  rs.setTargetPosition(0);
  rs.onTimer();
  EXPECT_EQ(rs.events, "dU");
  rs.setTargetPosition(STOP_REQUEST);
  rs.onTimer();
  EXPECT_EQ(rs.events, "dUud");
  time.advance(501);
  rs.setTargetPosition(100);
  rs.onTimer();
  EXPECT_EQ(rs.events, "dUuduD");
  rs.setTargetPosition(STOP_REQUEST);
  rs.onTimer();
  EXPECT_EQ(rs.events, "dUuduDud");
  EXPECT_FALSE(rs.upOn);
  EXPECT_FALSE(rs.downOn);
}

TEST_F(Sd4linuxCmdRollerShutterTests, MissingCommandBlocksMovement) {
  for (int missing = 0; missing < 4; ++missing) {
    SCOPED_TRACE(missing);
    SimulatedCmdRollerShutter rs;
    configure(rs, missing);
    recalibrate(rs);
    rs.onTimer();
    EXPECT_FALSE(rs.isCalibrationInProgress());
    EXPECT_EQ(rs.events, "ud");
    EXPECT_FALSE(rs.upOn);
    EXPECT_FALSE(rs.downOn);
  }
}

TEST_F(Sd4linuxCmdRollerShutterTests, PlainRollerShutterStillRequiresGpio) {
  Supla::Control::RollerShutter rs;
  rs.setOpenCloseTime(5000, 5000);
  recalibrate(rs);
  rs.onTimer();
  EXPECT_TRUE(rs.isCalibrationRequested());
  EXPECT_FALSE(rs.isCalibrationInProgress());
}

}  // namespace
