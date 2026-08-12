// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_LINUX_FILE_STATE_LOGGER_H_
#define EXTRAS_PORTING_LINUX_LINUX_FILE_STATE_LOGGER_H_

#include <supla/device/last_state_logger.h>

#include <string>

namespace Supla {
namespace Device {
class FileStateLogger : public LastStateLogger {
 public:
  explicit FileStateLogger(const std::string &);
  void log(const char *, int) override;
  void addToFile(const char *line);

 protected:
  std::string file;
};
};  // namespace Device
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_FILE_STATE_LOGGER_H_
