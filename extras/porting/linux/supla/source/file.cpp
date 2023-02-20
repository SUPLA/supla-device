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

#include <chrono>  // NOLINT(build/c++11)
#include <fstream>
#include <iostream>

#include "file.h"

Supla::Source::File::File(const char *filePath, int expirationSec)
    : filePath(filePath), fileExpirationSec(expirationSec) {
}

Supla::Source::File::~File() {
}

std::string Supla::Source::File::getContent() {
  std::string result;
  std::ifstream file;
  try {
    file.open(filePath);
    auto fileTime = std::filesystem::last_write_time(filePath);
    auto now = std::filesystem::file_time_type::clock::now();

    if (fileExpirationSec != 0
        && fileTime + std::chrono::seconds(fileExpirationSec) < now) {
      if (!fileIsTooOldLog) {
        fileIsTooOldLog = true;
        SUPLA_LOG_DEBUG("File: file \"%s\" is too old", filePath.c_str());
      }
      // file is too old
      return result;
    } else {
      fileIsTooOldLog = false;
      std::string line;
      while (std::getline(file, line)) {
        result.append(line).append("\n");
        if (result.length() > 1024 * 1024 * 10) {
          // file is too big - cut it at 10 MB
          break;
        }
      }
    }
  } catch (std::filesystem::filesystem_error) {
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
