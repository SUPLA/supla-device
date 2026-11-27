// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/mutex.h>

#include <mutex>  // NOLINT(build/c++11)

namespace {

class LinuxMutex : public Supla::Mutex {
 public:
  void lock() override { mutex.lock(); }
  void unlock() override { mutex.unlock(); }

 private:
  // Timer callbacks can synchronously invoke actions taking the same lock.
  std::recursive_mutex mutex;
};

}  // namespace

Supla::Mutex *Supla::Mutex::Create() {
  return new LinuxMutex;
}
