// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_command.h"

#include <supla/log_wrapper.h>

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <string>

namespace {

const char *safeContext(const char *context) {
  return context != nullptr ? context : "Linux command";
}

}  // namespace

FILE *Supla::Linux::openCommandPipe(const std::string &command,
                                    const char *context,
                                    CommandPipeFunctions functions) {
  const auto open = functions.popen != nullptr ? functions.popen : ::popen;
  FILE *pipe = open(command.c_str(), "r");
  if (pipe == nullptr) {
    const int error = errno;
    SUPLA_LOG_ERROR(
        "%s: popen() failed: %s", safeContext(context), std::strerror(error));
  }
  return pipe;
}

int Supla::Linux::closeCommandPipe(FILE *pipe,
                                   const char *context,
                                   CommandPipeFunctions functions) {
  if (pipe == nullptr) {
    return -1;
  }

  const auto close = functions.pclose != nullptr ? functions.pclose : ::pclose;
  const int result = close(pipe);
  if (result == -1) {
    const int error = errno;
    SUPLA_LOG_ERROR(
        "%s: pclose() failed: %s", safeContext(context), std::strerror(error));
  }
  return result;
}

void Supla::Linux::executeCommand(const std::string &command,
                                  const char *context,
                                  CommandPipeFunctions functions) {
  FILE *pipe = openCommandPipe(command, context, functions);
  if (pipe != nullptr) {
    closeCommandPipe(pipe, context, functions);
  }
}
