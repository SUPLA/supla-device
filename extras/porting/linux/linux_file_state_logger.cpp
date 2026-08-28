// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include <ctime>
#include <filesystem>  // NOLINT(build/c++17)
#include <iomanip>
#include <sstream>
#include <string>

#include "linux_file_state_logger.h"
#include "linux_secure_file.h"
#include "supla/device/last_state_logger.h"

namespace Supla {
const char LastStateFile[] = "/last_state.txt";
};

Supla::Device::FileStateLogger::FileStateLogger(const std::string &path) {
  if (!std::filesystem::exists(path)) {
    std::error_code err;
    if (!std::filesystem::create_directories(path, err)) {
      SUPLA_LOG_WARNING(
                "Config: failed to create folder for last state file");
      return;
    }
  }

  file = path + Supla::LastStateFile;
  if (!Supla::Linux::writeSecureFile(file, nullptr, 0, false)) {
    SUPLA_LOG_ERROR("Config: failed to create last state file");
    return;
  }

  addToFile("Starting supla-device");
}

void Supla::Device::FileStateLogger::log(const char *logLine, int uptime) {
  addToFile(logLine);
  Supla::Device::LastStateLogger::log(logLine, uptime);
}

void Supla::Device::FileStateLogger::addToFile(const char *line) {
  time_t now = time(nullptr);
  std::ostringstream output;
  output <<
    std::put_time(localtime(&now), "%F %T ")  // NOLINT(runtime/threadsafe_fn)
    << line << std::endl;
  const std::string outputString = output.str();
  if (!Supla::Linux::writeSecureFile(
          file, outputString.data(), outputString.size(), true)) {
    SUPLA_LOG_ERROR("Config: failed to write last state file");
  }
}
