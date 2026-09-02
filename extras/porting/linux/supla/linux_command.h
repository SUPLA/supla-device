// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
