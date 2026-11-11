// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include <chrono>  // NOLINT(build/c++11)
#include <algorithm>
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
  constexpr size_t maxSize = 10 * 1024 * 1024;
  std::string result;
  std::ifstream file;
  bool readError = false;
  bool tooLarge = false;
  try {
    if (!isConnected()) {
      // file is too old
      return result;
    }

    file.open(filePath, std::ios::binary);
    readError = !file.is_open();
    char buffer[4096];
    while (!readError) {
      const size_t remaining = maxSize - result.size();
      // Probe one byte beyond the limit, including when the buffer is full.
      file.read(buffer, std::min(sizeof(buffer), remaining + 1));
      const size_t count = static_cast<size_t>(file.gcount());
      if (file.bad() || (file.fail() && !file.eof())) {
        readError = true;
        break;
      }
      if (count > remaining) {
        tooLarge = true;
        readError = true;
        break;
      }
      result.append(buffer, count);
      if (file.eof()) {
        if (!result.empty() && result.back() != '\n') {
          if (result.size() == maxSize) {
            tooLarge = true;
            readError = true;
          } else {
            result.push_back('\n');
          }
        }
        break;
      }
    }
  } catch (const std::filesystem::filesystem_error &) {
    readError = true;
    SUPLA_LOG_ERROR("File: file \"%s\" reading error", filePath.c_str());
  }

  file.close();
  if (readError) {
    result.clear();
  }
  if (tooLarge) {
    if (!fileIsTooLargeLog) {
      SUPLA_LOG_ERROR("File: file \"%s\" content exceeds 10 MiB",
                      filePath.c_str());
      fileIsTooLargeLog = true;
    }
    // Retain the valid cache, but stop returning it after the grace period.
    if (readFailures < 3) {
      readFailures++;
      return prevResult;
    }
    return result;
  } else if (!readError) {
    fileIsTooLargeLog = false;
  }

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
