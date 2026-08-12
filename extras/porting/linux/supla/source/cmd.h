// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SOURCE_CMD_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SOURCE_CMD_H_

#include <supla/parser/parser.h>

#include <string>

#include "source.h"

namespace Supla {

namespace Source {
class Cmd : public Source {
 public:
  explicit Cmd(const char *cmd);
  virtual ~Cmd();
  std::string getContent() override;

 protected:
  std::string cmdLine;
};
};  // namespace Source
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SOURCE_CMD_H_
