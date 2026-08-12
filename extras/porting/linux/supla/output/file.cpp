// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "file.h"

#include <supla/log_wrapper.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace Supla {
namespace Output {
File::File(const char *file) : fileName(file) {
}
File::~File() {
}
bool File::putContent(int payload) {
  std::ofstream outputFile(fileName);
  if (!outputFile) {
    SUPLA_LOG_WARNING("File %s cannot be opened", fileName.c_str());
    return false;
  }
  outputFile << payload;
  outputFile.close();
  SUPLA_LOG_DEBUG("%d saved to file %s", payload, fileName.c_str());
  return true;
}
bool File::putContent(const std::vector<int> &payload) {
  std::ofstream outputFile(fileName);
  if (!outputFile) {
    SUPLA_LOG_WARNING("File %s cannot be opened", fileName.c_str());
    return false;
  }
  for (int i : payload) {
    outputFile << i << "\n";
  }
  outputFile.close();

  std::ostringstream oss;
  for (int i : payload) {
    oss << i << ' ';
  }
  std::string payloadStr = oss.str();
  SUPLA_LOG_DEBUG("%s saved to file %s", payloadStr.c_str(), fileName.c_str());
  return true;
}
bool File::putContent(const std::string &payload) {
  std::ofstream outputFile(fileName);
  if (!outputFile) {
    SUPLA_LOG_WARNING("File %s cannot be opened", fileName.c_str());
    return false;
  }
  outputFile << payload;
  outputFile.close();
  SUPLA_LOG_DEBUG("%s saved to file %s", payload.c_str(), fileName.c_str());
  return true;
}
bool File::putContent(bool payload) {
  std::ofstream outputFile(fileName);
  if (!outputFile) {
    SUPLA_LOG_WARNING("File %s cannot be opened", fileName.c_str());
    return false;
  }
  outputFile << std::boolalpha << payload;
  outputFile.close();
  std::string payloadStr = payload ? "true" : "false";
  SUPLA_LOG_DEBUG("%s saved to file %s", payloadStr.c_str(), fileName.c_str());
  return true;
}
}  // namespace Output
}  // namespace Supla
