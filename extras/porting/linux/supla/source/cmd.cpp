// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cmd.h"

#include <supla/linux_command.h>

#include <cstdio>
#include <string>

Supla::Source::Cmd::Cmd(const char *cmd) : cmdLine(cmd) {
}

Supla::Source::Cmd::~Cmd() {
}

std::string Supla::Source::Cmd::getContent() {
  auto p = Supla::Linux::openCommandPipe(cmdLine, "Source::Cmd");
  if (p) {
    std::string content;
    int c = 0;
    while ((c = fgetc(p)) != EOF) {
      content.append(1, static_cast<char>(c));
    }
    Supla::Linux::closeCommandPipe(p, "Source::Cmd");
    return content;
  }
  return std::string("");
}
