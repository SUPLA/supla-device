// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_CMD_H_
#define EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_CMD_H_

#include <string>
#include <vector>

#include "output.h"

namespace Supla {
namespace Output {

class Cmd : public Output {
 public:
  explicit Cmd(std::string cmd);
  virtual ~Cmd();

 protected:
  std::string cmdLine;

 private:
  bool putContent(int payload) override;
  bool putContent(const std::string &payload) override;
  bool putContent(const std::vector<int> &payload) override;
  bool putContent(bool payload) override;
};

}  // namespace Output
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_CMD_H_
