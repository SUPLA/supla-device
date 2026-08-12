// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include <ctime>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <iomanip>
#include <string>

#include "linux_file_state_logger.h"
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
  std::ofstream out(file);
  out.close();

  addToFile("Starting supla-device");
}

void Supla::Device::FileStateLogger::log(const char *logLine, int uptime) {
  addToFile(logLine);
  Supla::Device::LastStateLogger::log(logLine, uptime);
}

void Supla::Device::FileStateLogger::addToFile(const char *line) {
  std::ofstream out(file, std::ios_base::app);

  time_t now = time(nullptr);
  out <<
    std::put_time(localtime(&now), "%F %T ")  // NOLINT(runtime/threadsafe_fn)
    << line << std::endl;
  out.close();
}
