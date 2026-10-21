// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_ASYNCDNSTESTS_FAKES_FREERTOS_FREERTOS_H_
#define EXTRAS_TEST_ASYNCDNSTESTS_FAKES_FREERTOS_FREERTOS_H_
#include <mutex>
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
inline void portENTER_CRITICAL(portMUX_TYPE *lock) { lock->lock(); }
inline void portEXIT_CRITICAL(portMUX_TYPE *lock) { lock->unlock(); }
#endif  // EXTRAS_TEST_ASYNCDNSTESTS_FAKES_FREERTOS_FREERTOS_H_
