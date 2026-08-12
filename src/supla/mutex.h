// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_MUTEX_H_
#define SRC_SUPLA_MUTEX_H_

namespace Supla {

class Mutex {
 public:
  static Mutex* Create();
  virtual ~Mutex();
  virtual void lock();
  virtual void unlock();
 protected:
  Mutex();
};

};  // namespace Supla
#endif  // SRC_SUPLA_MUTEX_H_
