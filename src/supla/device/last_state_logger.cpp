/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/auto_lock.h>
#include <supla/mutex.h>
#include <stdio.h>

#include "last_state_logger.h"

namespace Supla {
namespace Device {

LastStateLogger::LastStateLogger() {
  mutex = Supla::Mutex::Create();
}

LastStateLogger::~LastStateLogger() {
  if (mutex) {
    delete mutex;
  }
}

bool LastStateLogger::prepareLastStateLog() {
  index = 0;
  return true;
}

void LastStateLogger::log(const char *state, int uptimeSec) {
  int newStateSize = strlen(state);
  char timestamp[32] = {};
  if (uptimeSec > 7) {
    snprintf(timestamp, sizeof(timestamp), " (%d)", uptimeSec);
  }
  int timestampSize = strlen(timestamp);

  if (newStateSize + timestampSize + 1 > LAST_STATE_LOGGER_BUFFER_SIZE) {
    return;
  }

  Supla::AutoLock autoLock(mutex);
  char *copy = new char[LAST_STATE_LOGGER_BUFFER_SIZE];
  memcpy(copy, buffer, LAST_STATE_LOGGER_BUFFER_SIZE);
  memcpy(buffer, state, newStateSize);
  memcpy(buffer + newStateSize, timestamp, timestampSize);
  newStateSize += timestampSize;
  buffer[newStateSize] = '\0';
  memcpy(buffer + newStateSize + 1,
         copy,
         (LAST_STATE_LOGGER_BUFFER_SIZE - newStateSize - 1));
  buffer[LAST_STATE_LOGGER_BUFFER_SIZE - 4] = '.';
  buffer[LAST_STATE_LOGGER_BUFFER_SIZE - 3] = '.';
  buffer[LAST_STATE_LOGGER_BUFFER_SIZE - 2] = '.';
  buffer[LAST_STATE_LOGGER_BUFFER_SIZE - 1] = '\0';

  SUPLA_LOG_INFO("LAST STATE ADDED: %s", buffer);
  delete [] copy;
}

void LastStateLogger::clear() {
  Supla::AutoLock autoLock(mutex);
  memset(buffer, 0, LAST_STATE_LOGGER_BUFFER_SIZE);
}

char *LastStateLogger::getLog() {
  if (buffer[index]) {
    if (index == 0) {
      mutex->lock();
    }
    if (index < LAST_STATE_LOGGER_BUFFER_SIZE) {
      char *ptr = buffer + index;
      int logSize = strlen(buffer + index);
      index += logSize + 1;
      return ptr;
    }
  }
  mutex->unlock();
  return nullptr;
}

};  // namespace Device
};  // namespace Supla
