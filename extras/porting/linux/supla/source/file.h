// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SOURCE_FILE_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SOURCE_FILE_H_

#include <supla/parser/parser.h>

#include <filesystem>  // NOLINT
#include <string>

#include "source.h"

namespace Supla {

namespace Source {
class File : public Source {
 public:
  explicit File(const char *filePath, int expirationSec = 10 * 60);
  virtual ~File();
  std::string getContent() override;
  bool isConnected() override;

  void setExpirationTime(int timeSec);

 protected:
  std::filesystem::path filePath;
  int fileExpirationSec = 10 * 60;
  bool fileIsTooOldLog = false;
  std::string prevResult;
  int readFailures = 0;
};
};  // namespace Source
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SOURCE_FILE_H_
