// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "auto_lock.h"
#include "mutex.h"

Supla::AutoLock::AutoLock(Mutex *mut) : mutex(mut) {
  lock();
}

Supla::AutoLock::~AutoLock() {
  unlock();
}

void Supla::AutoLock::lock() {
  if (mutex && !locked) {
    locked = true;
    mutex->lock();
  }
}

void Supla::AutoLock::unlock() {
  if (mutex && locked) {
    locked = false;
    mutex->unlock();
  }
}
