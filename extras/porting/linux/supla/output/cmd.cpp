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

#include "cmd.h"

#include <supla/log_wrapper.h>

#include <sstream>
#include <string>
#include <vector>

Supla::Output::Cmd::Cmd(const char *cmd) : cmdLine(cmd) {
}

Supla::Output::Cmd::~Cmd() {
}

bool Supla::Output::Cmd::putContent(int payload) {
  if (!cmdLine.empty()) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), cmdLine.c_str(), payload);
    SUPLA_LOG_DEBUG("Command: %s", buffer);
    FILE* p = popen(buffer, "r");
    if (p == nullptr) {
      SUPLA_LOG_WARNING("Failed to execute command: %s", buffer);
      return false;
    }
    pclose(p);
    return true;
  }
  return false;
}

bool Supla::Output::Cmd::putContent(bool payload) {
  if (!cmdLine.empty()) {
    char buffer[256];
    snprintf(
        buffer, sizeof(buffer), cmdLine.c_str(), payload ? "true" : "false");
    SUPLA_LOG_DEBUG("Command: %s", buffer);
    FILE* p = popen(buffer, "r");
    if (p == nullptr) {
      SUPLA_LOG_WARNING("Failed to execute command: %s", buffer);
      return false;
    }
    pclose(p);
    return true;
  }
  return false;
}

bool Supla::Output::Cmd::putContent(const std::string &payload) {
  if (!cmdLine.empty()) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), cmdLine.c_str(), payload.c_str());
    SUPLA_LOG_DEBUG("Command: %s", buffer);
    FILE* p = popen(buffer, "r");
    if (p == nullptr) {
      SUPLA_LOG_WARNING("Failed to execute command: %s", buffer);
      return false;
    }
    pclose(p);
    return true;
  }
  return false;
}
bool Supla::Output::Cmd::putContent(const std::vector<int> &payload) {
  if (!cmdLine.empty()) {
    std::ostringstream oss;
    for (int i : payload) {
      oss << i << ' ';
    }
    std::string payloadStr = oss.str();
    char buffer[256];
    snprintf(buffer, sizeof(buffer), cmdLine.c_str(), payloadStr.c_str());
    SUPLA_LOG_DEBUG("Command: %s", buffer);
    FILE* p = popen(buffer, "r");
    if (p == nullptr) {
      SUPLA_LOG_WARNING("Failed to execute command: %s", buffer);
      return false;
    }
    pclose(p);
    return true;
  }
  return false;
}
