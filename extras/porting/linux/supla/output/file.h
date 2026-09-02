// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_FILE_H_
#define EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_FILE_H_

#include <vector>
#include <string>

#include "output.h"

namespace Supla {
namespace Output {

class File : public Output {
 public:
  explicit File(const char *file);
  virtual ~File();

 protected:
  std::string fileName;

 private:
  bool putContent(int payload) override;
  bool putContent(const std::string &payload) override;
  bool putContent(const std::vector<int> &payload) override;
  bool putContent(bool payload) override;
};

}  // namespace Output
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_FILE_H_
