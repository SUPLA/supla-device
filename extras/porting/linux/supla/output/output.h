// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_OUTPUT_H_
#define EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_OUTPUT_H_

#include <string>
#include <variant>
#include <vector>

namespace Supla {

namespace Output {
class Output {
 public:
  virtual ~Output() {
  }

  virtual bool putContent(int payload) = 0;
  virtual bool putContent(bool payload) = 0;
  virtual bool putContent(const std::string &payload) = 0;
  virtual bool putContent(const std::vector<int> &payload) = 0;
};
};  // namespace Output
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_OUTPUT_H_
