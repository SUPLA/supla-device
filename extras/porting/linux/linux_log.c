// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <syslog.h>
#include <time.h>

#include <supla/debug/debug_log.h>
#include <supla-common/log.h>

extern int runAsDaemon;
extern int logLevel;

void supla_vlog(int __pri, const char *message) {
  if (__pri > logLevel) {
    return;
  }

  if (message == NULL) {
    return;
  }

  if (runAsDaemon == 1) {
    syslog(__pri, "%s", message);
  } else {
    switch (__pri) {
      case LOG_EMERG:
        printf("EME");
        break;
      case LOG_ALERT:
        printf("ALR");
        break;
      case LOG_CRIT:
        printf("CRI");
        break;
      case LOG_ERR:
        printf("ERR");
        break;
      case LOG_WARNING:
        printf("WAR");
        break;
      case LOG_NOTICE:
        printf("NOT");
        break;
      case LOG_INFO:
        printf("INF");
        break;
      case LOG_DEBUG:
        printf("DEB");
        break;
      case LOG_VERBOSE:
        printf("VER");
        break;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t t = tv.tv_sec;
    struct tm tms;
    struct tm* tm_info = localtime_r(&t, &tms);
    printf("[%02d:%02d:%02d.%03d] ",
           tm_info->tm_hour,
           tm_info->tm_min,
           tm_info->tm_sec,
           (int)(tv.tv_usec / 1000));

    switch (__pri) {
      case LOG_EMERG:
      case LOG_ALERT:
      case LOG_CRIT:
      case LOG_ERR:
        printf("\033[1;31m");
        break;
      case LOG_WARNING:
        printf("\033[1;33m");
        break;
      case LOG_INFO:
        printf("\033[1;32m");
        break;
      case LOG_NOTICE:
      case LOG_DEBUG:
      case LOG_VERBOSE:
        break;
    }


    printf("%s", message);
    printf("\033[0m\n");
    fflush(stdout);
  }

  supla_debug_log_write(__pri, message);
}
