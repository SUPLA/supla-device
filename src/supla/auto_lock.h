// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_AUTO_LOCK_H_
#define SRC_SUPLA_AUTO_LOCK_H_

namespace Supla {
class Mutex;

class AutoLock {
 public:
  explicit AutoLock(Mutex *);
  ~AutoLock();
  void lock();
  void unlock();

 protected:
  Mutex* mutex;
  bool locked = false;
};

};  // namespace Supla
#endif  // SRC_SUPLA_AUTO_LOCK_H_

