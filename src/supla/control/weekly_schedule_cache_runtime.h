// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_CACHE_RUNTIME_H_
#define SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_CACHE_RUNTIME_H_

#include <stdint.h>

namespace Supla {
namespace Control {

class WeeklyScheduleCacheRuntime {
 public:
  void reset();
  void touch(bool active, uint32_t now);
  bool process(bool active, uint32_t now);

 private:
  bool loaded_ = false;
  bool active_ = false;
  uint32_t inactiveSinceMs_ = 0;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_CACHE_RUNTIME_H_
