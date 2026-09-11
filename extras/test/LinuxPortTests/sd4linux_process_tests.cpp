// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <linux_process.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <thread>

TEST(Sd4linuxProcessTests, Signals) {
  for (int sig : {SIGHUP, SIGINT, SIGTERM, SIGQUIT, SIGPIPE}) {
    ASSERT_EXIT({
      alarm(5);
      if (st_app_terminate != 0) _exit(1);
      st_hook_signals();
      std::thread worker([sig]() { raise(sig); });
      worker.join();
      if (st_app_terminate != 0) _exit(2);
      raise(sig);
      _exit(st_app_terminate == (sig == SIGPIPE ? 0 : 1) ? 0 : 3);
    }, ::testing::ExitedWithCode(0), "");
  }
}

TEST(Sd4linuxProcessTests, DaemonProcess) {
  ASSERT_EXIT({
    alarm(5);
    int resultPipe[2];
    if (pipe(resultPipe) != 0) _exit(1);
    pid_t child = fork();
    if (child < 0) _exit(2);
    if (child == 0) {
      // The daemon also gets a deadline after st_try_fork's second fork.
      if (st_try_fork() != 1) _exit(3);
      alarm(5);
      char cwd[2] = {};
      unsigned char ok = getsid(0) == getpid() &&
          getcwd(cwd, sizeof(cwd)) != nullptr && cwd[0] == '/' &&
          cwd[1] == 0 && fcntl(0, F_GETFD) == -1 &&
          fcntl(1, F_GETFD) == -1 && fcntl(2, F_GETFD) == -1;
      if (write(resultPipe[1], &ok, sizeof(ok)) != sizeof(ok)) _exit(4);
      _exit(0);
    }
    close(resultPipe[1]);
    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) _exit(5);
    unsigned char ok = 0;
    if (read(resultPipe[0], &ok, sizeof(ok)) != sizeof(ok)) _exit(6);
    close(resultPipe[0]);
    _exit(ok ? 0 : 7);
  }, ::testing::ExitedWithCode(0), "");
}
