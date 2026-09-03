// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_component.h"

namespace Supla {
namespace Control {

bool ExternalManagedWeeklySchedule::canActivate() const {
  return true;
}

bool ExternalManagedWeeklySchedule::isActive() const {
  return active_;
}

bool ExternalManagedWeeklySchedule::switchToWeeklySchedule() {
  active_ = true;
  return true;
}

void ExternalManagedWeeklySchedule::switchToManualMode() {
  active_ = false;
}

void ExternalManagedWeeklySchedule::restoreWeeklyScheduleMode(
    bool enabled) {
  active_ = enabled;
}

bool ExternalManagedWeeklySchedule::processWeeklySchedule() {
  return active_;
}

}  // namespace Control
}  // namespace Supla
