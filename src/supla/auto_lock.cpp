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
