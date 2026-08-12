// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_HVAC_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_HVAC_PARSED_H_

#include <supla/control/hvac_base.h>

#include <string>

namespace Supla {
namespace Control {

class CmdOutput;

class HvacParsed : public HvacBase {
 public:
  HvacParsed(std::string cmdOn,
             std::string cmdOff,
             std::string cmdOnSecondary,
             std::string cmdOffSecondary);
  virtual ~HvacParsed();

 protected:
  uint32_t lastReadTime = 0;
  CmdOutput *cmdOutput = nullptr;
  CmdOutput *cmdOutputSecondary = nullptr;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_HVAC_PARSED_H_
