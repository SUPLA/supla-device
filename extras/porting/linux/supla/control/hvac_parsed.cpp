// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hvac_parsed.h"

#include <supla/linux_command.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/control/output_interface.h>

#include <string>

using Supla::Control::HvacParsed;

namespace Supla {
namespace Control {
class CmdOutput : public OutputInterface {
 public:
  CmdOutput(std::string cmdOn, std::string cmdOff);

  int getOutputValue() const override;
  void setOutputValue(int value) override;
  bool isOnOffOnly() const override;

 private:
  std::string cmdOn;
  std::string cmdOff;
  int lastState = 0;
};
}  // namespace Control
}  // namespace Supla

using Supla::Control::CmdOutput;

CmdOutput::CmdOutput(std::string cmdOn, std::string cmdOff) :
  cmdOn(cmdOn), cmdOff(cmdOff) {
}

int CmdOutput::getOutputValue() const {
  return lastState;
}

void CmdOutput::setOutputValue(int value) {
  lastState = value;
  if (value == 1) {
    if (cmdOn.length() > 0) {
      Supla::Linux::executeCommand(cmdOn, "Hvac.cmd_on");
    }
  } else if (value == 0) {
    if (cmdOff.length() > 0) {
      Supla::Linux::executeCommand(cmdOff, "Hvac.cmd_off");
    }
  }
}

bool CmdOutput::isOnOffOnly() const {
  return true;
}

HvacParsed::HvacParsed(std::string cmdOn,
                       std::string cmdOff,
                       std::string cmdOnSecondary,
                       std::string cmdOffSecondary) {
  cmdOutput = new CmdOutput(cmdOn, cmdOff);
  addPrimaryOutput(cmdOutput);
  if (cmdOnSecondary.length() > 0 && cmdOffSecondary.length() > 0) {
    SUPLA_LOG_DEBUG("Hvac: adding secondary output");
    cmdOutputSecondary = new CmdOutput(cmdOnSecondary, cmdOffSecondary);
    addSecondaryOutput(cmdOutputSecondary);
  }
}

HvacParsed::~HvacParsed() {
  delete cmdOutput;
  cmdOutput = nullptr;
  delete cmdOutputSecondary;
  cmdOutputSecondary = nullptr;
}
