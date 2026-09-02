// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SOURCE_SOURCE_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SOURCE_SOURCE_H_

#include <string>

namespace Supla {

namespace Source {
class Source {
 public:
  virtual ~Source() {}
  virtual std::string getContent() = 0;
  virtual bool isConnected() { return true; }
};
};  // namespace Source
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SOURCE_SOURCE_H_
