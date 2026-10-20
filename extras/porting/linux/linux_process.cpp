// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_process.h"

#include <signal.h>
#include <stdlib.h>
#include <supla/log_wrapper.h>
#include <unistd.h>

std::atomic<unsigned char> st_app_terminate{0};
static_assert(std::atomic<unsigned char>::is_always_lock_free,
              "Signal termination flag must be lock-free");

namespace {
void signalHandler(int signal) {
  (void)signal;
  st_app_terminate.store(1, std::memory_order_relaxed);
}
}  // namespace

void st_hook_signals(void) {
  struct sigaction action = {};
  sigemptyset(&action.sa_mask);
  action.sa_handler = signalHandler;
  const int signals[] = {SIGHUP, SIGINT, SIGTERM, SIGQUIT};
  for (int sig : signals) {
    sigaction(sig, &action, nullptr);
  }
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, nullptr);
}

char st_try_fork(void) {
  pid_t pid = fork();
  if (pid < 0) {
    SUPLA_LOG_ERROR("Can't fork");
    return 0;
  }
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
  if (setsid() < 0 || chdir("/") < 0) {
    SUPLA_LOG_ERROR("Can't fork");
    return 0;
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  return 1;
}
