// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_LINUX_PROCESS_H_
#define EXTRAS_PORTING_LINUX_LINUX_PROCESS_H_

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char st_app_terminate;
void st_hook_signals(void);
char st_try_fork(void);

#ifdef __cplusplus
}
#endif

#endif  // EXTRAS_PORTING_LINUX_LINUX_PROCESS_H_
