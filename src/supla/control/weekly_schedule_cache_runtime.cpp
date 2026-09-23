// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_cache_runtime.h"

namespace Supla {
namespace Control {

namespace {

constexpr uint32_t kWeeklyScheduleCacheReleaseDelayMs = 15000;

}  // namespace

void WeeklyScheduleCacheRuntime::reset() {
  loaded_ = false;
  active_ = false;
  inactiveSinceMs_ = 0;
}

void WeeklyScheduleCacheRuntime::touch(bool active, uint32_t now) {
  loaded_ = true;
  active_ = active;
  inactiveSinceMs_ = active ? 0 : now;
}

bool WeeklyScheduleCacheRuntime::process(bool active, uint32_t now) {
  if (!loaded_) {
    active_ = active;
    return false;
  }

  if (active) {
    active_ = true;
    inactiveSinceMs_ = 0;
    return false;
  }

  if (active_) {
    active_ = false;
    inactiveSinceMs_ = now;
    return false;
  }

  if (static_cast<uint32_t>(now - inactiveSinceMs_) <
      kWeeklyScheduleCacheReleaseDelayMs) {
    return false;
  }

  reset();
  return true;
}

}  // namespace Control
}  // namespace Supla
