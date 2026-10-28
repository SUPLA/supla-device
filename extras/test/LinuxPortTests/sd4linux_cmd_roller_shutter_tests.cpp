// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/control/cmd_roller_shutter.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

namespace {

class Sd4linuxCmdRollerShutterTests : public testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    time.advance(1000);
    char pattern[] = "/tmp/codex/cmd-rs-test-XXXXXX";
    auto directory = mkdtemp(pattern);
    ASSERT_NE(directory, nullptr);
    tempDirectory = directory;
    commandLog = tempDirectory + "/commands";
  }

  void TearDown() override {
    if (!tempDirectory.empty()) {
      unlink(commandLog.c_str());
      EXPECT_EQ(rmdir(tempDirectory.c_str()), 0);
    }
    Supla::Channel::resetToDefaults();
  }

  void configure(Supla::Control::CmdRollerShutter &rs, int missing = -1) {
    rs.setCmdUpOn(missing == 0 ? "" : command("U"));
    rs.setCmdUpOff(missing == 1 ? "" : command("u"));
    rs.setCmdDownOn(missing == 2 ? "" : command("D"));
    rs.setCmdDownOff(missing == 3 ? "" : command("d"));
    rs.setOpenCloseTime(5000, 5000);
  }

  std::string command(const char *event) const {
    return std::string("printf '") + event + "' >> '" + commandLog + "'";
  }

  std::string events() const {
    std::ifstream file(commandLog);
    return {std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
  }

  void recalibrate(Supla::Control::RollerShutter &rs) {
    TSD_DeviceCalCfgRequest request = {};
    request.Command = SUPLA_CALCFG_CMD_RECALIBRATE;
    request.SuperUserAuthorized = 1;
    ASSERT_EQ(rs.handleCalcfgFromServer(&request), SUPLA_CALCFG_RESULT_DONE);
  }

  SimpleTime time;
  std::string tempDirectory;
  std::string commandLog;
};

TEST_F(Sd4linuxCmdRollerShutterTests, CalibratesUsingCommandsWithoutGpio) {
  Supla::Control::CmdRollerShutter rs(nullptr);
  configure(rs);
  rs.onInit();
  recalibrate(rs);
  rs.onTimer();
  ASSERT_TRUE(rs.isCalibrationInProgress());
  EXPECT_EQ(events(), "dU");
  time.advance(5501);  // Calibration includes a 10% margin.
  rs.onTimer();
  EXPECT_FALSE(rs.isCalibrationInProgress());
  EXPECT_TRUE(rs.isCalibrated());
  EXPECT_EQ(rs.getCurrentPosition(), 0);
  EXPECT_EQ(events(), "dUud");
}

TEST_F(Sd4linuxCmdRollerShutterTests,
       MovesBothDirectionsAndStopsUsingCommands) {
  Supla::Control::CmdRollerShutter rs(nullptr);
  configure(rs);
  rs.onInit();
  rs.setNotCalibrated();
  rs.setCurrentPosition(50);
  rs.setTargetPosition(0);
  rs.onTimer();
  EXPECT_EQ(events(), "dU");
  rs.setTargetPosition(STOP_REQUEST);
  rs.onTimer();
  EXPECT_EQ(events(), "dUud");
  time.advance(501);
  rs.setTargetPosition(100);
  rs.onTimer();
  EXPECT_EQ(events(), "dUuduD");
  rs.setTargetPosition(STOP_REQUEST);
  rs.onTimer();
  EXPECT_EQ(events(), "dUuduDud");
}

TEST_F(Sd4linuxCmdRollerShutterTests, MissingCommandBlocksMovement) {
  for (int missing = 0; missing < 4; ++missing) {
    SCOPED_TRACE(missing);
    unlink(commandLog.c_str());
    Supla::Control::CmdRollerShutter rs(nullptr);
    configure(rs, missing);
    recalibrate(rs);
    rs.onTimer();
    EXPECT_FALSE(rs.isCalibrationInProgress());
    EXPECT_EQ(events().find('U'), std::string::npos);
    EXPECT_EQ(events().find('D'), std::string::npos);
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
