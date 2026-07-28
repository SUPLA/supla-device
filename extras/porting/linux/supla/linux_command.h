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

#ifndef EXTRAS_PORTING_LINUX_SUPLA_LINUX_COMMAND_H_
#define EXTRAS_PORTING_LINUX_SUPLA_LINUX_COMMAND_H_

#include <cstdio>
#include <string>

namespace Supla {
namespace Linux {

using PopenFunction = FILE *(*)(const char *, const char *);
using PcloseFunction = int (*)(FILE *);

struct CommandPipeFunctions {
  PopenFunction popen = nullptr;
  PcloseFunction pclose = nullptr;
};

FILE *openCommandPipe(const std::string &command,
                      const char *context,
                      CommandPipeFunctions functions = {});

int closeCommandPipe(FILE *pipe,
                     const char *context,
                     CommandPipeFunctions functions = {});

void executeCommand(const std::string &command,
                    const char *context,
                    CommandPipeFunctions functions = {});

}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_LINUX_COMMAND_H_
