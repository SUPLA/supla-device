/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "hvac_parsed.h"

#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/control/output_interface.h>

#include <cstdio>

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
      auto p = popen(cmdOn.c_str(), "r");
      pclose(p);
    }
  } else if (value == 0) {
    if (cmdOff.length() > 0) {
      auto p = popen(cmdOff.c_str(), "r");
      pclose(p);
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
