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
