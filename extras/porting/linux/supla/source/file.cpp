// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include <chrono>  // NOLINT(build/c++11)
#include <fstream>
#include <iostream>
#include <string>

#include "file.h"

Supla::Source::File::File(const char *filePath, int expirationSec)
    : filePath(filePath), fileExpirationSec(expirationSec) {
}

Supla::Source::File::~File() {
}

bool Supla::Source::File::isConnected() {
  if (fileExpirationSec == 0) {
    return true;
  }

  try {
    auto fileTime = std::filesystem::last_write_time(filePath);
    auto now = std::filesystem::file_time_type::clock::now();

    if (fileTime + std::chrono::seconds(fileExpirationSec) < now) {
      if (!fileIsTooOldLog) {
        fileIsTooOldLog = true;
        SUPLA_LOG_DEBUG("File: file \"%s\" is too old", filePath.c_str());
      }
      return false;
    }

    fileIsTooOldLog = false;
  } catch (const std::filesystem::filesystem_error &) {
    return false;
  }

  return true;
}

std::string Supla::Source::File::getContent() {
  std::string result;
  std::ifstream file;
  try {
    if (!isConnected()) {
      // file is too old
      return result;
    }

    file.open(filePath);
    std::string line;
    while (std::getline(file, line)) {
      result.append(line).append("\n");
      if (result.length() > 1024 * 1024 * 10) {
        // file is too big - cut it at 10 MB
        break;
      }
    }
  } catch (const std::filesystem::filesystem_error &) {
    SUPLA_LOG_ERROR("File: file \"%s\" reading error", filePath.c_str());
  }

  file.close();

  if (result.length() < 1 && readFailures < 3) {
    SUPLA_LOG_DEBUG("File: file \"%s\" reading error or empty",
                    filePath.c_str());
    readFailures++;
    return prevResult;
  }

  readFailures = 0;
  prevResult = result;
  return result;
}

void Supla::Source::File::setExpirationTime(int timeSec) {
  fileExpirationSec = timeSec;
}
