/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <supla/log_wrapper.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>

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

void Supla::Device::FileStateLogger::log(const char *logLine) {
  addToFile(logLine);
  Supla::Device::LastStateLogger::log(logLine);
}

void Supla::Device::FileStateLogger::addToFile(const char *line) {
  std::ofstream out(file, std::ios_base::app);

  time_t now = time(nullptr);
  out <<
    std::put_time(localtime(&now), "%F %T ")  // NOLINT(runtime/threadsafe_fn)
    << line << std::endl;
  out.close();
}
