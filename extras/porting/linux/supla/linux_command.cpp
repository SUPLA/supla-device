// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_command.h"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <supla/log_wrapper.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <string>
#include <thread>  // NOLINT(build/c++11)

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

bool Supla::Linux::executeCommandWithTimeout(
    const std::string &command, const char *context,
    std::chrono::milliseconds timeout) {
  using Clock = std::chrono::steady_clock;
  const auto deadline = Clock::now() + timeout;
  posix_spawnattr_t attributes;
  int error = posix_spawnattr_init(&attributes);
  if (error != 0) {
    SUPLA_LOG_ERROR("%s: spawn attributes failed: %s",
                    safeContext(context), std::strerror(error));
    return false;
  }
  error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
  if (error == 0) {
    error = posix_spawnattr_setpgroup(&attributes, 0);
  }

  posix_spawn_file_actions_t actions;
  const int actionsError = posix_spawn_file_actions_init(&actions);
  if (error == 0) {
    error = actionsError;
  }
  if (error == 0) {
    error = posix_spawn_file_actions_addopen(
        &actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  }
  char shell[] = "sh";
  char option[] = "-c";
  char *const args[] = {shell, option, const_cast<char *>(command.c_str()),
                        nullptr};
  pid_t child = -1;
  if (error == 0) {
    error = posix_spawn(
        &child, "/bin/sh", &actions, &attributes, args, environ);
  }
  if (actionsError == 0) {
    posix_spawn_file_actions_destroy(&actions);
  }
  posix_spawnattr_destroy(&attributes);
  if (error != 0) {
    SUPLA_LOG_ERROR("%s: command start failed: %s",
                    safeContext(context), std::strerror(error));
    return false;
  }

  int status = 0;
  while (true) {
    const auto result = waitpid(child, &status, WNOHANG);
    if (result == child) {
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
      }
      SUPLA_LOG_ERROR("%s: command failed (wait status %d)",
                      safeContext(context), status);
      return false;
    }
    if (result == -1 && errno != EINTR) {
      SUPLA_LOG_ERROR("%s: waitpid failed: %s",
                      safeContext(context), std::strerror(errno));
      return false;
    }
    if (Clock::now() >= deadline) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  SUPLA_LOG_ERROR("%s: command timed out", safeContext(context));
  // The shell is still our unreaped child, so its process group ID is safe.
  if (kill(-child, SIGKILL) == -1 && errno != ESRCH) {
    SUPLA_LOG_ERROR("%s: command termination failed: %s",
                    safeContext(context), std::strerror(errno));
    return false;
  }
  while (waitpid(child, &status, 0) == -1 && errno == EINTR) {
  }
  return false;
}
