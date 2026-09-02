// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_LAST_STATE_LOGGER_H_
#define SRC_SUPLA_DEVICE_LAST_STATE_LOGGER_H_

#define LAST_STATE_LOGGER_BUFFER_SIZE 500

#include <stddef.h>

namespace Supla {
class Mutex;

namespace Device {
class LastStateLogger {
 public:
  LastStateLogger();
  virtual ~LastStateLogger();
  virtual void log(const char *, int uptimeSec);
  virtual void clear();

  // getLog locks the mutex on first call and releases it when all messages
  // have been read, so make sure you call it in a loop until null is returned
  virtual char *getLog();
  virtual bool prepareLastStateLog();

 protected:
  char buffer[LAST_STATE_LOGGER_BUFFER_SIZE] = {};
  size_t index = 0;
  Supla::Mutex *mutex = nullptr;
};
};  // namespace Device
};  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_LAST_STATE_LOGGER_H_
