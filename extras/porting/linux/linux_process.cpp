// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_process.h"

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <supla/log_wrapper.h>
#include <unistd.h>

unsigned char st_app_terminate = 0;

namespace {
pthread_t mainThread;

void signalHandler(int signal) {
  (void)signal;
  if (pthread_self() == mainThread) {
    st_app_terminate = 1;
  }
}
}  // namespace

void st_hook_signals(void) {
  mainThread = pthread_self();
  signal(SIGHUP, signalHandler);
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
  signal(SIGQUIT, signalHandler);
  signal(SIGPIPE, SIG_IGN);
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
