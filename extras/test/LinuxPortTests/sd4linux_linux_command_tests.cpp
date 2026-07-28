/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include <gtest/gtest.h>
#include <supla/linux_command.h>

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <string>

extern "C" const char *supla_test_get_last_log();

namespace {

FILE *pipeReturnedByPopen = nullptr;
FILE *pipePassedToPclose = nullptr;
int pcloseCallCount = 0;

FILE *fakePopen(const char *, const char *) {
  errno = 0;
  return pipeReturnedByPopen;
}

int fakePclose(FILE *pipe) {
  pipePassedToPclose = pipe;
  pcloseCallCount++;
  return 0;
}

int failingPclose(FILE *pipe) {
  pipePassedToPclose = pipe;
  pcloseCallCount++;
  errno = ECHILD;
  return -1;
}

FILE *failingPopen(const char *, const char *) {
  errno = EMFILE;
  return nullptr;
}

void resetFakePipeState() {
  pipeReturnedByPopen = reinterpret_cast<FILE *>(0x1234);
  pipePassedToPclose = nullptr;
  pcloseCallCount = 0;
}

}  // namespace

TEST(Sd4linuxLinuxCommandTests, PopenFailureDoesNotCallPcloseWithNull) {
  Supla::Linux::CommandPipeFunctions functions = {&failingPopen, &fakePclose};
  pcloseCallCount = 0;

  Supla::Linux::executeCommand("test command", "CmdRelay[7].cmd_on", functions);

  EXPECT_EQ(pcloseCallCount, 0);
  EXPECT_NE(std::string(supla_test_get_last_log()).find("CmdRelay[7].cmd_on"),
            std::string::npos);
  EXPECT_NE(std::string(supla_test_get_last_log()).find(std::strerror(EMFILE)),
            std::string::npos);
}

TEST(Sd4linuxLinuxCommandTests, ValidPipeIsPassedToPclose) {
  resetFakePipeState();
  Supla::Linux::CommandPipeFunctions functions = {&fakePopen, &fakePclose};

  Supla::Linux::executeCommand(
      "test command", "CmdValve[3].cmd_open", functions);

  EXPECT_EQ(pcloseCallCount, 1);
  EXPECT_EQ(pipePassedToPclose, pipeReturnedByPopen);
}

TEST(Sd4linuxLinuxCommandTests, PcloseFailureIsLogged) {
  resetFakePipeState();
  Supla::Linux::CommandPipeFunctions functions = {&fakePopen, &failingPclose};

  Supla::Linux::executeCommand("test command", "Hvac.cmd_off", functions);

  EXPECT_EQ(pcloseCallCount, 1);
  EXPECT_NE(std::string(supla_test_get_last_log()).find("Hvac.cmd_off"),
            std::string::npos);
  EXPECT_NE(std::string(supla_test_get_last_log()).find(std::strerror(ECHILD)),
            std::string::npos);
}
