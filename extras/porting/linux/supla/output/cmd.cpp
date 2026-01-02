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

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

static bool replaceFirst(std::string& s,
                         std::string_view what,
                         std::string_view with) {
  const auto pos = s.find(what);
  if (pos == std::string::npos) {
    return false;
  }
  s.replace(pos, what.size(), with);
  return true;
}

static std::string buildCmd(std::string_view cmdLine, std::string_view arg) {
  std::string cmd(cmdLine);

  if (replaceFirst(cmd, "{}", arg)) {
    return cmd;
  }
  if (replaceFirst(cmd, "%s", arg)) {
    return cmd;
  }
  if (replaceFirst(cmd, "%d", arg)) {
    return cmd;
  }

  if (!cmd.empty() && cmd.back() != ' ') {
    cmd.push_back(' ');
  }
  cmd.append(arg);
  return cmd;
}

static bool execCmd(const std::string& cmd) {
  SUPLA_LOG_DEBUG("Command: %s", cmd.c_str());
  FILE* p = popen(cmd.c_str(), "r");
  if (!p) {
    SUPLA_LOG_WARNING("Failed to execute command: %s", cmd.c_str());
    return false;
  }
  pclose(p);
  return true;
}

Supla::Output::Cmd::Cmd(std::string cmd) : cmdLine(cmd) {
}

Supla::Output::Cmd::~Cmd() {
}

bool Supla::Output::Cmd::putContent(int payload) {
  if (cmdLine.empty()) return false;
  return execCmd(buildCmd(cmdLine, std::to_string(payload)));
}

bool Supla::Output::Cmd::putContent(bool payload) {
  if (cmdLine.empty()) return false;

  const char* val = payload ? "true" : "false";
  return execCmd(buildCmd(cmdLine, val));
}

bool Supla::Output::Cmd::putContent(const std::string& payload) {
  if (cmdLine.empty()) return false;

  return execCmd(buildCmd(cmdLine, payload));
}

bool Supla::Output::Cmd::putContent(const std::vector<int>& payload) {
  if (cmdLine.empty()) return false;

  std::string payloadStr;
  payloadStr.reserve(payload.size() * 12);

  for (size_t i = 0; i < payload.size(); ++i) {
    if (i) {
      payloadStr.push_back(' ');
    }
    payloadStr += std::to_string(payload[i]);
  }

  return execCmd(buildCmd(cmdLine, payloadStr));
}

