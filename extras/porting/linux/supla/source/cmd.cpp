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

#include <cstdio>
#include <string>

Supla::Source::Cmd::Cmd(const char *cmd) : cmdLine(cmd) {
}

Supla::Source::Cmd::~Cmd() {
}

std::string Supla::Source::Cmd::getContent() {
  auto p = popen(cmdLine.c_str(), "r");
  if (p) {
    std::string content;
    int c = 0;
    while ((c = fgetc(p)) != EOF) {
      content.append(1, static_cast<char>(c));
    }
    pclose(p);
    return content;
  }
  return std::string("");
}
